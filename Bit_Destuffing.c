//Bit De-Stuffing

[24bcs084@mepcolinux exe1]$cat boDeStuffhead.h
#include <stdio.h>
#include <string.h>
#define MAX_FILE_SIZE 10000
#define MAX_DATA 2000
void bitDestuffing(char input[], char output[]);
int checkInterference(char data[]);
[24bcs084@mepcolinux exe1]$cat boDeStuffimp.c
#include "boDeStuffhead.h"
void bitDestuffing(char input[], char output[])
{
    int i = 0, j = 0;
    int count = 0;
    while(input[i] != '\0')
    {
        output[j++] = input[i];
        if(input[i] == '1')
        {
            count++;
            if(count == 5)
            {
                i++;
                if(input[i] == '0')
                {
                    count = 0;
                }
                else
                {
                    output[j++] = input[i];
                }
            }
        }
        else
        {
            count = 0;
        }
        i++;
    }
    output[j] = '\0';
}
int checkInterference(char data[])
{
    int count = 0;
    int i;
    for (i = 0; data[i] != '\0'; i++)
    {
        if (data[i] == '1')
        {
            count++;
            if (count == 7)
                return 0;
        }
        else
        {
            count = 0;
        }
    }
    return 0;
}
[24bcs084@mepcolinux exe1]$cat boDeStuffapp.c
#include "boDeStuffhead.h"
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
    int length = strlen(fileData);
    int i = 0;
    while (i < length)
    {
        char frame[MAX_DATA];
        if (strncmp(&fileData[i], "01111110", 8) == 0)
        {
            int start = i;
            i += 8;
            int j = 0;
            while (i < length && strncmp(&fileData[i], "01111110", 8) != 0)
            {
                frame[j++] = fileData[i++];
            }
            frame[j] = '\0';
            int frameLen = strlen(frame);
            if (frameLen > 48)
            {
                strcpy(stuffedData, frame + 16);
                stuffedData[frameLen - 48] = '\0';
                if (checkInterference(stuffedData))
                {
                   printf("\n---------------------------------\n");
                   printf("INTERFERENCE OCCURRED!\n");
                   printf("ERROR IN DATA OF CURRENT FRAME.\n");
                   printf("---------------------------------\n\n");
                   fprintf(out, "ERROR IN FRAME\n");
                  }
         else
         {
             bitDestuffing(stuffedData, originalData);
             fprintf(out, "%s\n", originalData);
             printf("Received Frame Data: %s\n", originalData);
               }
          }
            i += 8;
        }
        else
        {
            i++;
        }
    }
    fclose(fp);
    fclose(out);
    printf("\nBit de-stuffing completed successfully.\n");
    printf("Output written to output.txt\n");
    return 0;
}
