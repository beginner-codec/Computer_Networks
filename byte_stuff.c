/*
Develop a program to implement the Byte Stuffing technique using special Start of Frame (SOF), End of Frame (EOF), and Escape (ESC) characters. Whenever the data contains SOF, EOF, or ESC characters, the sender should insert an escape character before transmitting. At the receiver, perform byte de-stuffing to reconstruct the original message accurately. Simulate transmission errors by modifying one or more bytes, detect frame inconsistencies, and display the original message, stuffed frame, transmitted frame, received frame, and recovered data.
*/
#include <stdio.h>
#include <string.h>
#define SOF '@'
#define EOF_CHAR '#'
#define ESC '/'
void byteStuff(char data[], char stuffed[])
{
    int i, j;
    i = 0;
    j = 0;
    stuffed[j] = SOF;
    j++;
    while (data[i] != '\0')
    {
        if (data[i] == SOF ||
            data[i] == EOF_CHAR ||
            data[i] == ESC)
        {
            stuffed[j] = ESC;
            j++;
        }
        stuffed[j] = data[i];
        j++;
        i++;
    }
    stuffed[j] = EOF_CHAR;
    j++;
    stuffed[j] = '\0';
}
int byteDestuff(char frame[], char recovered[])
{
    int i, j, len;
    i = 1;
    j = 0;
    len = strlen(frame);
    if (frame[0] != SOF || frame[len - 1] != EOF_CHAR)
    {
        return 0;
    }
    while (i < len - 1)
    {
        if (frame[i] == ESC)
        {
            i++;
            if (i >= len - 1)
            {
                return 0;
            }
        }
        recovered[j] = frame[i];
        j++;
        i++;
    }
    recovered[j] = '\0';
    return 1;
}
int main()
{
    char data[100];
    char stuffed[200];
    char transmitted[200];
    char recovered[100];
    int errors;
    int i;
    int pos;
    char newByte;
    printf("Enter the original message: ");
    fgets(data, sizeof(data), stdin);
    data[strcspn(data, "\n")] = '\0';
    byteStuff(data, stuffed);
    printf("\nOriginal Message : %s\n", data);
    printf("Stuffed Frame     : %s\n", stuffed);
    strcpy(transmitted, stuffed);
    printf("\nEnter number of bytes to modify (0 for no error): ");
    scanf("%d", &errors);
    i = 0;
    while (i < errors)
    {
        printf("Enter position to modify (0 to %d): ",
               (int)strlen(transmitted) - 1);
        scanf("%d", &pos);
        if (pos >= 0 && pos < strlen(transmitted))
        {
            printf("Enter new byte: ");
            scanf(" %c", &newByte);
            transmitted[pos] = newByte;
            i++;
        }
        else
        {
            printf("Invalid position! Try again.\n");
        }
    }
    printf("\nTransmitted Frame : %s\n", transmitted);
    if (byteDestuff(transmitted, recovered))
    {
        printf("Received Frame    : %s\n", transmitted);
        printf("Recovered Data    : %s\n", recovered);
        if (strcmp(data, recovered) == 0)
        {
            printf("\nStatus: Data received correctly.\n");
        }
        else
        {
            printf("\nStatus: ERROR DETECTED - Data is corrupted.\n");
        }
    }
    else
    {
        printf("Received Frame    : %s\n", transmitted);
        printf("Recovered Data    : Invalid Frame\n");
        printf("\nStatus: ERROR DETECTED - Frame inconsistency.\n");
    }
    return 0;
}
