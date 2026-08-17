[24bcs084@mepcolinux exe1]$cat byDeStuffhead.h
#include <stdio.h>
#include <string.h>
#define MAX_FILE_SIZE 10000
#define MAX_DATA 2000
extern const char FLAG[];
extern const char ESC[];
void byteDestuffing(char input[], char output[]);
[24bcs084@mepcolinux exe1]$cat byDeStuffimp.c
#include "byDeStuffhead.h"
const char FLAG[] = "01111110";
const char ESC[]  = "01111101";
void byteDestuffing(char input[], char output[])
{
    int i = 0;
    int j = 0;
    while (input[i] != '\0')
    {
        char current[9];
        char next[9];
        strncpy(current, &input[i], 8);
        current[8] = '\0';
        if (input[i + 8] != '\0')
        {
            strncpy(next, &input[i + 8], 8);
            next[8] = '\0';
        }
        else
        {
            next[0] = '\0';
        }
        if (strcmp(current, ESC) == 0 &&
            (strcmp(next, FLAG) == 0 || strcmp(next, ESC) == 0))
        {
            strcpy(&output[j], next);
            j += 8;
            i += 16;
        }
        else
        {
            strcpy(&output[j], current);
            j += 8;
            i += 8;
        }
    }
    output[j] = '\0';
}
[24bcs084@mepcolinux exe1]$cat byDeStuffapp.c
#include "byDeStuffhead.h"
int main()
{
    FILE *fp, *out;
    char fileData[MAX_FILE_SIZE];
    char stuffedData[MAX_DATA];
    char originalData[MAX_DATA];
    fp = fopen("frames.txt", "r");
    if (fp == NULL)
    {
        printf("Unable to open frames.txt\n");
        return 1;
    }
    out = fopen("output.txt", "w");
    if (out == NULL)
    {
        printf("Unable to create output.txt\n");
        fclose(fp);
        return 1;
    }
    fscanf(fp, "%s", fileData);
    fclose(fp);
    int len = strlen(fileData);
    int i = 0;
    while (i <= len - 8)
    {
        if (strncmp(&fileData[i], FLAG, 8) == 0)
        {
            int start = i;
            int dataStart = start + 24;
            int j = dataStart;
            while (j <= len - 8)
            {
                if (strncmp(&fileData[j], FLAG, 8) == 0)
                {
                    if (j >= 8 &&
                        strncmp(&fileData[j - 8], ESC, 8) == 0)
                    {
                        j += 8;
                        continue;
                    }
                    break;
                }
 l               j += 8;
            }
            if (j > len - 8)
                break;
            int end = j;
            int dataEnd = end - 32;
            int index = 0;
            int k;
            for (k = dataStart; k < dataEnd; k++)
            {
                stuffedData[index++] = fileData[k];
            }
            stuffedData[index] = '\0';
            byteDestuffing(stuffedData, originalData);
            printf("\nFrame found");
            printf("\nStuffed Data  : %s", stuffedData);
            printf("\nOriginal Data : %s\n", originalData);
            fprintf(out, "%s\n", originalData);
            i = end + 8;
        }
        else
        {
            i++;
        }
    }
    fclose(out);
    printf("\nOriginal data written to output.txt\n");
    return 0;
}
