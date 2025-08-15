#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char test_string[] = "YO GABBA GABBA IS SO COOL";
char test_key[] = " FF NDN FJNDSLKFNROGNRNGIDFSKLGNFKLSNBVKF";
char message_letter, key_letter;

char* valid_characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
int encrypt_index, decrypt_index;

int encrypt(char* s_to_be_encrypted, char* key)
{
    for (int i = 0; i < strlen(s_to_be_encrypted); i++)
    {
        // ascii value of space is 32
        if (s_to_be_encrypted[i] == ' ')
        {
            // subtracting to 26 makes it the last value of our alphabet array
            message_letter = (s_to_be_encrypted[i] - 6);
        } 
        // ascii value of 'A' is 65
        else 
        {
            // subtracting by 65 places letters at correct index in our alphabet array
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
        s_to_be_encrypted[i] = valid_characters[encrypt_index];
    }
    return 0;
}

int decrypt(char* s_to_be_decrypted, char* key)
{
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
        s_to_be_decrypted[i] = valid_characters[decrypt_index];
    }
    return 0;
}

int main()
{
    printf("%s\n", test_string);
    encrypt(test_string, test_key);
    printf("%s\n", test_string);
    decrypt(test_string, test_key);
    printf("%s\n", test_string);
    return 0;
}
