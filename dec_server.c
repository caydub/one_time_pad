#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>

#define BUFFER_SIZE 140000

/* ----------------------------------------------------------------
Error function that prints an error message.
------------------------------------------------------------------- */
void socketError(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
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
Function decrypt: Decrypts a string consisting of capital characters
and spaces using a provided key consisting of capital characters
and spaces
args~
- s_to_be_decrypted:        String to be decrpyted		(char*)
- key:                      Provided key                (char*)
returns~
0 when complete
------------------------------------------------------------------- */
int decrypt(char* s_to_be_decrypted, char* key)
{
    char message_letter, key_letter;
    int decrypt_index;

    for (int i = 0; i < strlen(s_to_be_decrypted); i++)
    {
                if (s_to_be_decrypted[i] == ' ')
        {
            message_letter = (s_to_be_decrypted[i] - 6);
        } 
        else 
        {
            message_letter = (s_to_be_decrypted[i] - 'A');
        }

        if (key[i] == ' ')
        {
            key_letter = (key[i] - 6);
        }
        else
        {
            key_letter = key[i] - 'A';
        }
        decrypt_index = (message_letter - key_letter) % 27;
        if (decrypt_index < 0)
        {
            decrypt_index = decrypt_index + 27;
        }
        s_to_be_decrypted[i] = alphabet[decrypt_index];
    }
    return 0;
}

/* ----------------------------------------------------------------
Function checkBuffer: Iterate through the buffer to determine if
the server has received all the characters from the client, using
newline as the end of message indicator. 
args~
- buffer:          Client message       (char*)
- length:          Length of buffer     (int)
returns~
0 when complete.
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
Function fullSend: Sends all bytes over to client utilizing a
loop.
args~
- sfd:              Client socket file descriptor		(int)
- decrypted_text:   Buffer/string to send               (char*)
- bytes_remaining:  Bytes left to send to client        (int)
returns~
0 when complete

* NOTE * This function utilizes a very similar scheme to the
function sendall() identified in Beej's Guide to Network Programming 
Section 7.4. 

Website: https://beej.us/guide/bgnet/html/#sendall
Author: Brian “Beej Jorgensen” Hall
------------------------------------------------------------------- */
int fullSend(int sfd, char* decrypted_text, int bytes_remaining)
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
        bytes_read = send(sfd, decrypted_text + bytes_already_sent, bytes_to_send, 0);
        if (bytes_read == -1) // error sending
        {
            socketError("ERROR writing to socket");
        }
        bytes_already_sent += bytes_read;
        bytes_remaining -= bytes_read;
    }
    return 0;
}

/* ----------------------------------------------------------------
Function fullRetrieve: Retrieves all bytes from client utilizing a
loop.
args~
- sfd:              Client socket file descriptor		(int)
- buf:              Buffer/string to send               (char*)
- bytes_remaining:  Bytes left to send to client        (int)
returns~
0 when complete.
------------------------------------------------------------------- */
char* fullRetrieve(int sfd)
{
    int bytes_read = 0;
    int bytes_already_retrieved = 0;
    char* temp_text = NULL;
    char* buf = (char*)malloc(BUFFER_SIZE);
    memset(buf, '\0', BUFFER_SIZE);
    size_t received_length = 0;

    while (!checkBuffer(buf, strlen(buf)))
    {
        // read the client's message from the socket
        bytes_read = recv(sfd, buf + strlen(buf), (BUFFER_SIZE - 1) - strlen(buf), 0);

        // if error reading from socket
        if (bytes_read < 0)
        {
            close(sfd);
            socketError("ERROR reading from socket");
        }

        // if client socket closed
        if (bytes_read == 0)
        {
            close(sfd);
            return EXIT_SUCCESS;
        }
    }
    return buf;
}

/* ----------------------------------------------------------------
Function addProcess: Create a process structure using dynamic
memory allocation.
args~
- pid:		Process ID		(int)
returns~
0 when complete.
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
linked list.
args~
- pid:		Process ID		(int)
returns~
0 when complete.
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
Function checkBgs: Check bg processes to see if any have terminated.
args~
None
returns~
0 when complete.
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
Function setupAddressStruct: Set up socket address structure.
args~
- address:             Socket Adress    	(struct aockaddr_in*)
- portNumber:          Port Number          (int)
returns~
Nothing.
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

/* ----------------------------------------------------------------
                            Main
------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int connectionSocket, charsRead;
    char* received_text = NULL, *decrypted_text;
    char buf[256] = {0};
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
        socketError("ERROR opening socket");
    }

    // Set up the address struct for the server socket
    setupAddressStruct(&serverAddress, atoi(argv[1]));

    // Associate the socket to the port
    if (bind(listenSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0)
    {
        socketError("ERROR on binding");
    }

    // start listening for connetions. allow up to 5 connections to queue up
    listen(listenSocket, 5);

    // listening loop
    while(1) 
    {
        // there can only be 5 concurrent child processes/connections at a time
        if (concurrent_connections >= 5)
        {
            checkBgs();
            continue;
        }

        // accept the connection request which creates a connection socket
        connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);

        if (connectionSocket < 0)
        {
            socketError("ERROR on accept");
        }

        spawn_pid = fork();
        switch (spawn_pid)
        {
        	case -1: // error forking
                perror("fork()");
                break;
            
            case 0: // child process
                // verify connection is to dec_client
                send(connectionSocket, "dec_server", 10, 0);
                charsRead = recv(connectionSocket, buf, 255, 0);

                if (strcmp("dec_client", buf))
                {
                    close(connectionSocket);
                    return 2;
                }

                // retrieve string from client
                received_text = fullRetrieve(connectionSocket);

                // parse received string into plaintext and key
                char* plain_text = strtok(received_text, "\n");
                char* key = strtok(NULL, "\n");

                // decrypt the plaintext
                decrypt(plain_text, key);

                // re add the newline to the text, was removed during tokenization
                decrypted_text = (char *) calloc((strlen(plain_text) + 2), sizeof(char));
                strcpy(decrypted_text, plain_text);
                decrypted_text[strlen(plain_text)] = '\n';

                // send back to client
                fullSend(connectionSocket, decrypted_text, strlen(decrypted_text));

                free(received_text);
                free(plain_text);
                free(key);
                free(decrypted_text);

                // Close the connection socket for this client
                close(connectionSocket);
                return 0;

            default:
                // add child pid into linked list to keep track of when it ends
                addProcess(connectionSocket);
                break;
        }
    }

    // Close the listening socket
    close(listenSocket);
    return 0;
}
