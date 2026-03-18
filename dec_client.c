#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netdb.h>

/* ----------------------------------------------------------------
Error function that prints an error message before exiting the
process.
------------------------------------------------------------------- */
void socketError(const char *msg, int error_code)
{
    fprintf(stderr, "%s\n", msg);
    exit(error_code);
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
            return 1;
        }
    }
    return 0;
}

/* ----------------------------------------------------------------
Function fullSend: Sends all bytes over to client utilizing a
loop.
args~
- sfd:              Client socket file descriptor		(int)
- text_to_send:     Buffer/string to send               (char*)
- bytes_remaining:  Bytes left to send to client        (int)
returns~
0 when complete.

* NOTE * This function utilizes a very similar scheme to the
function sendall() identified in Beej's Guide to Network Programming 
Section 7.4. 

Website: https://beej.us/guide/bgnet/html/#sendall
Author: Brian “Beej Jorgensen” Hall
------------------------------------------------------------------- */
int fullSend(int sfd, char* text_to_send, int bytes_remaining)
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
        bytes_read = send(sfd, text_to_send + bytes_already_sent, bytes_to_send, 0);
        if (bytes_read == -1) // error sending
        {
            close(sfd);
            socketError("dec_client error: error during send", 1);
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
returns~
Retrieved string.
------------------------------------------------------------------- */
char* fullRetrieve(int sfd)
{
    char* received_text = NULL;
    int bytes_read = 0;
    char* temp_text = NULL;
    char buf[1001] = "";
    size_t received_length = 0;

    while (!checkBuffer(buf, strlen(buf)))
    {
        memset(buf, '\0', 1001); // clear buffer for more reading

        // read the server's message from the socket
        bytes_read = recv(sfd, buf, 1000, 0);

        // if error reading from socket
        if (bytes_read < 0)
        {
            close(sfd);
            socketError("dec_client error: recv failed", 1);
        }

        // if client socket closed
        if (bytes_read == 0)
        {
            close(sfd);
            exit(1);
        }

        received_length += strlen(buf);

        // each iteration, transfer buffer into final output
        if (received_text == NULL)
        {
            received_text = strdup(buf);
            if (received_text == NULL)
            {
                close(sfd);
                socketError("dec_client error: error reading from socket, initial", 1);
            }
        }
        else 
        {
            temp_text = realloc(received_text, (received_length + 1) * sizeof(char));
            if (temp_text == NULL)
            {
                close(sfd);
                socketError("dec_client error: error reading from socket, reallocation", 1);
            }
            else
            {
                received_text = temp_text;
                received_text[received_length] = '\0';
                temp_text = NULL;
                strcat(received_text, buf);
            }
        }
    }
    return received_text;
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
    // Get the DNS entry for this host name
    struct hostent* hostInfo = gethostbyname("localhost");

    if (hostInfo == NULL)
    {
        socketError("dec_client error: error locating localhost", 1);
    }

    // Copy the first IP address from the DNS entry to sin_addr.s_addr
    memcpy((char*) &address->sin_addr.s_addr,
    hostInfo->h_addr_list[0],
    hostInfo->h_length);
}

/* ----------------------------------------------------------------
Function readFile: Opens a .txt file which only has a string 
followed by a newline in the current directory and reads the text
into a string.
args~
- path:             Path to File    	(char*)
returns~
String containing text from file.
------------------------------------------------------------------- */
char* readFile(char* path)
{   
    FILE* input_file;
    char* buf;
    char temp_char;
    size_t file_length;

    // open file for read only
    input_file = fopen(path, "r");
    if (input_file == NULL)
    {
        fprintf(stderr,"%s does not exist\n", path);
        exit(0);
    }

    // retrieve the length of the text in the file
    fseek(input_file, 0, SEEK_END); // move pointer to end of file
    file_length = ftell(input_file);
    rewind(input_file); // reset pointer

    buf = (char *) calloc(file_length + 1, sizeof(char));

    for (int i = 0; i < file_length; i++)
    {
        temp_char = fgetc(input_file);
        if ((temp_char < 'A' || temp_char > 'Z') && (temp_char != ' ' && temp_char != '\n'))
        {
            fclose(input_file);
            free(buf);
            socketError("dec_client error: input contains bad characters", 1);
        }
        buf[i] = temp_char;
    }
    fclose(input_file);
    return buf;
}

/* ----------------------------------------------------------------
Function combineString: Combines two strings.
args~
- string_1:         String to combine       (char*)
- string_2:         String to combine       (char*)
returns~
Combined string.
------------------------------------------------------------------- */
char* combineStrings(char* string_1, char* string_2)
{
    char* combined;
    size_t combined_length = strlen(string_1) + strlen(string_2);
    combined = (char *) calloc(combined_length + 1, sizeof(char));
    strcpy(combined, string_1);
    strcat(combined, string_2);

    return combined;
}

/* ----------------------------------------------------------------
                            Main
------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    int socketFD, charsRead;
    char *plaintext = NULL, *cypher_key = NULL, *text_and_key, *received_text;
    struct sockaddr_in serverAddress;
    char buf[256] = {0};

    // Check usage & args
    if (argc < 4)
    {
        fprintf(stderr,"USAGE: %s plaintext key port\n", argv[0]);
        exit(0);
    }

    // transfers contents of plain text and key into buffers
    plaintext = readFile(argv[1]);
    cypher_key = readFile(argv[2]);

    if (strlen(cypher_key) < strlen(plaintext))
    {
        char msg[100];
        sprintf(msg, "Error: key '%s' is too short", argv[2]);
        socketError(msg, 2);
    }

    // combine the plaintext and key for ease of sending
    text_and_key = combineStrings(plaintext, cypher_key);

    // create a socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
    {
        socketError("CLIENT: ERROR opening socket", 1);
    }

    // set up the server address struct
    setupAddressStruct(&serverAddress, atoi(argv[3]));

    // sonnect to server
    if (connect(socketFD, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0) 
    {
        socketError("CLIENT: ERROR connecting", 1);
    }

    // verify connection is to dec_server
    charsRead = recv(socketFD, buf, 255, 0);
    if (charsRead < 0)
    {
        close(socketFD);
        socketError("dec_client error: recv failed", 1);
    }

    send(socketFD, "dec_client", 10, 0);

    if (strcmp("dec_server", buf))
    {
        close(socketFD);
        char msg[50];
        sprintf(msg, "Error: could not contact dec_server on port %d", atoi(argv[3]));
        socketError(msg, 2);
    }

    fullSend(socketFD, text_and_key, strlen(text_and_key));

    received_text = fullRetrieve(socketFD);
    
    printf("%s", received_text);

    free(plaintext);
    free(cypher_key);
    free(text_and_key);
    free(received_text);
    close(socketFD);

    return 0;
}