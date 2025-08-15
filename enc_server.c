#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>

/* ----------------------------------------------------------------
Error function that prints an error message before exiting the
process.
------------------------------------------------------------------- */
void socketError(const char *msg)
{
    perror(msg);
    exit(1);
}

/* ----------------------------------------------------------------
Linked list for managing child processes. For this program there
will only be a maximum of 5 at a time.
------------------------------------------------------------------- */
struct process
{
	pid_t pid;
	struct process* next;
};

/* ---------- Global Variables ---------- */
struct process* head = NULL;
struct process* tail = NULL;
int last_fg_child_status = 0;
int last_bg_child_status;
int concurrent_connections = 0, newline_counter = 0;
char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

/* ----------------------------------------------------------------
Function encrypt: Encrypts a string consisting of capital characters
and spaces using a provided key consisting of capital characters
and spaces
args~
- s_to_be_encrypted:        String to be encrpyted		(char*)
- key:                      Provided key                (char*)
returns~
0 when complete
------------------------------------------------------------------- */
int encrypt(char* s_to_be_encrypted, char* key)
{
    char message_letter, key_letter;
    int encrypt_index;

    for (int i = 0; i < strlen(s_to_be_encrypted); i++)
    {
        // ascii value of space is 32
        if (s_to_be_encrypted[i] == ' ')
        {
            // subtracting to 26 makes it the last value of our alphabet
            message_letter = (s_to_be_encrypted[i] - 6);
        } 
        // ascii value of 'A' is 65
        else 
        {
            // subtracting by 65 places letters at correct index in our alphabet
            message_letter = (s_to_be_encrypted[i] - 'A');
        }

        if (key[i] == ' ')
        {
            key_letter = (key[i] - 6);
        }
        else
        {
            key_letter = key[i] - 'A';
        }
        encrypt_index = (message_letter + key_letter) % 27;
        s_to_be_encrypted[i] = alphabet[encrypt_index];
    }
    return 0;
}

/* ----------------------------------------------------------------
Function fullSend: Sends all bytes over to client utilizing a
loop.
args~
- sfd:              Client socket file descriptor		(int)
- encrypted_text:   Buffer/string to send               (char*)
- bytes_remaining:  Bytes left to send to client        (int)
returns~
0 when complete
------------------------------------------------------------------- */
int fullSend(int sfd, char* encrypted_text, int bytes_remaining)
{
    int bytes_to_send;
    int bytes_already_sent = 0;
    int bytes_read = 0;


    while (bytes_remaining)
    {
        // limit amount of data able to be sent at once
        if (bytes_remaining > 1000) 
        {
            bytes_to_send = 1000;
        }
        else
        {
            bytes_to_send = bytes_remaining;
        }
        bytes_read = send(sfd, encrypted_text + bytes_already_sent, bytes_to_send, 0);
        if (bytes_read == -1)
        {
            error("ERROR writing to socket");
        }
        bytes_already_sent += bytes_read;
        bytes_remaining -= bytes_read;
    }

    return 0;
}

/* ----------------------------------------------------------------
Function fullRetrieve: Retrieves all bytes fromt client utilizing a
loop.
args~
- sfd:              Client socket file descriptor		(int)
- buf:              Buffer/string to send               (char*)
- bytes_remaining:  Bytes left to send to client        (int)
returns~
0 when complete
------------------------------------------------------------------- */
int fullRetrieve(int sfd, char* received_text)
{
    int bytes_read = 0;
    int bytes_already_retrieved = 0;
    char* temp_text = NULL;
    char buf[1001] = "";
    size_t received_length = 0;

    while (!checkBuffer(buf, strlen(buf)))
    {
        memset(buf, '\0', 1001); // clear buffer for more reading

        // read the client's message from the socket
        bytes_read = recv(sfd, buf, 1000, 0);

        // if error reading from socket
        if (bytes_read < 0)
        {
            close(sfd);
            error("ERROR reading from socket");
        }

        // if client socket closed
        if (bytes_read == 0)
        {
            close(sfd);
            return EXIT_FAILURE;
        }

        received_length += strlen(buf);

        // each iteration, transfer buffer into final output
        if (received_text == NULL)
        {
            received_text = strdup(buf);
            if (received_text == NULL)
            {
                error("ERROR transfering data to server");
            }
        }
        else 
        {
            temp_text = realloc(received_text, (received_length + 1) * sizeof(char));
            if (temp_text == NULL)
            {
                error("ERROR transfering data to server");
            }
            else
            {
                received_text = temp_text;
                received_text[received_length] = "\0";
                temp_text = NULL;
                strcat(received_text, buf);
            }
        }
    }
}

/* ----------------------------------------------------------------
Function addProcess: Create a process structure using dynamic
memory allocation
args~
- pid:		Process ID		(int)
returns~
0 when complete
------------------------------------------------------------------- */
int addProcess (pid_t pid)
{
	struct process* curr_process = malloc(sizeof(struct process));
	curr_process->pid = pid;
	curr_process->next = NULL;
    concurrent_connections++;

    if(head == NULL) {
        head = curr_process;
		tail = curr_process;
    } else {
        tail->next = curr_process;
        tail = curr_process;
    }
	return 0;
}

/* ----------------------------------------------------------------
Function removeProcess: Remove a process structure from the 
linked list
args~
- pid:		Process ID		(int)
returns~
0 when complete
------------------------------------------------------------------- */
int removeProcess (pid_t pid)
{
	struct process* process = head;
	struct process* prev_process = NULL;

	while (process != NULL) {
		if (process->pid == pid && prev_process == NULL) {
			head = process->next;
			if (head == NULL) {
				tail = NULL;
			}
			free(process);
			process = NULL;
		} else if (process->pid == pid) {
			prev_process->next = process->next;
			if (prev_process->next == NULL) {
				tail = prev_process;
			}
			free(process);
			process = NULL;
		} else {
			prev_process = process;
			process = process->next;
		}
        concurrent_connections--;
	}
	return 0;
}

/* ----------------------------------------------------------------
Function checkBgs: Check bg processes to see if any have terminated
args~
None
returns~
0 when complete
------------------------------------------------------------------- */
int checkBgs () 
{
	struct process* process = head;
	while (process != NULL) {
		pid_t bg_pid = waitpid(process->pid, &last_bg_child_status, WNOHANG);
		if (bg_pid) {
			process = process->next;
			removeProcess(bg_pid);
			continue;
		}
		process = process->next;
	}
	return 0;
}

/* ----------------------------------------------------------------
Function checkBuffer: Iterate through the buffer to determine if
the server has received all the characters from the client. 

Newline '\n' is the character which first separates the received
plaintext from the received key and the second time indicates
there are no more characters remaining to be received.
------------------------------------------------------------------- */
int checkBuffer (char* buffer, int length) 
{
    for (int i = 0; i < length; i++)
    {
        if (buffer[i] == '\0')
        {
            break;
        }

        if (buffer[i] == '\n')
        {
            newline_counter++;
            if (newline_counter == 2)
            {
                return 1;
            }
        }
    }
    return 0;
}

/* ----------------------------------------------------------------
Address struct for server sockets.
------------------------------------------------------------------- */
void setupAddressStruct(struct sockaddr_in* address, int portNumber)
{
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address));

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);
    // Allow a client at any address to connect to this server
    address->sin_addr.s_addr = INADDR_ANY;
}

int main(int argc, char *argv[])
{
    int connectionSocket, charsRead;
    char* received_text = NULL;
    char buf[256] = "";
    size_t buf_length = 0, received_length = 0;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t sizeOfClientInfo = sizeof(clientAddress);
    pid_t spawn_pid;


    // Check usage & args
    if (argc != 2)
    {
        fprintf(stderr,"USAGE: %s port &\n", argv[0]);
        exit(1);
    }

    // Create the socket that will listen for connections
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0)
    {
        error("ERROR opening socket");
    }

    // Set up the address struct for the server socket
    setupAddressStruct(&serverAddress, atoi(argv[1]));

    // Associate the socket to the port
    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        error("ERROR on binding");
    }

    // start listening for connetions. allow up to 5 connections to queue up
    listen(listenSocket, 5);

    // listening loop
    while(1) 
    {
        // check child processes/connections to see if any have terminated
        checkBgs();

        // there can only be 5 concurrent child processes/connections at a time
        if (concurrent_connections == 5)
        {
            sleep(2);
            continue;
        }

        // accept the connection request which creates a connection socket
        connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);

        if (connectionSocket < 0)
        {
            error("ERROR on accept");
        }

        spawn_pid = fork();
        switch (spawn_pid)
        {
        	case -1: // error forking
                perror("fork()");
                break;
            
            case 0: // child process
                // verify connection is to enc_client
                send(connectionSocket, "enc_server", 10, 0);
                charsRead = recv(connectionSocket, buf, 255, 0);

                if (!strcmp("enc_client", buf))
                {
                    close(connectionSocket);
                    return EXIT_FAILURE;
                }

                // retrieve string from client
                fullRetrieve(connectionSocket, received_text);

                // parse received string into plaintext and key
                char* plain_text = strtok(received_text, "\n");
                char* key = strtok(NULL, "\n");

                // encrypt the plaintext
                encrypt(plain_text, key);

                // send back to client
                fullSend(connectionSocket, plain_text, strlen(plain_text));

                free(received_text);

                // Close the connection socket for this client
                close(connectionSocket);
                return EXIT_SUCCESS;

            default:
                // add child pid into linked list to keep track of when it ends
                addProcess(connectionSocket);
                break;
        }
        {
        continue; // parent continues listening
    }

    // Close the listening socket
    close(listenSocket);
    return EXIT_SUCCESS;
    }
