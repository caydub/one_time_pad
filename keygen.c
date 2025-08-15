#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VALID_CHAR_COUNT    27

char* valid_characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

int main (int argc, char **argv)
{
    size_t key_length;
    int alph_index;
    char* key;

    if (argc < 2 || argc > 2)
    {
        fprintf(stderr, "An error occurred: Example usage: ./keygen 10\n");
        return EXIT_FAILURE;
    }

    key_length = atoi(argv[1]);

    if (key_length == 0)
    {
        fprintf(stderr, "An error occurred: Length must be an integer greater than 0\n");
        return EXIT_FAILURE;
    }
    
    key = (char*) calloc(key_length + 2, sizeof(char)); // allocate mem for string, new line, and null
    
    for (size_t i = 0; i < key_length; i++) // generate key
    {
        alph_index = rand() % (VALID_CHAR_COUNT); // keep index within range of allowed characters
        key[i] = valid_characters[alph_index];
    }

    key[key_length] = '\n';
    printf("%s", key);

    free(key);

    return EXIT_SUCCESS;
}