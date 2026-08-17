[24bcs084@mepcolinux exe1]$cat byStuffhead.h
#include <stdio.h>
#include <string.h>
#define MAX_DATA 1000
#define MAX_STUFFED 2000
struct Frame
{
    char flagStart[9];
    char address[9];
    char control[9];
    char data[MAX_STUFFED];
    char fcs[33];
    char flagEnd[9];
};
extern struct Frame frame;
void byteStuffing(char input[], char output[]);
[24bcs084@mepcolinux exe1]$cat byStuffimp.c
#include "byStuffhead.h"
struct Frame frame;
const char ESC[] = "01111101";
void byteStuffing(char input[], char output[])
{
    int i = 0;
    int j = 0;
    while (input[i] != '\0')
    {
        char byte[9];
        strncpy(byte, &input[i], 8);
        byte[8] = '\0';
        if (strcmp(byte, "01111110") == 0 ||
            strcmp(byte, ESC) == 0)
        {
            strcpy(&output[j], ESC);
            j += 8;
        }
        strcpy(&output[j], byte);
        j += 8;
        i += 8;
    }
    output[j] = '\0';
}
[24bcs084@mepcolinux exe1]$cat byStuffapp.c
#include "byStuffhead.h"
int main()
{
    int n, i;
    char inputData[MAX_DATA];
    FILE *fp;
    strcpy(frame.flagStart, "01111110");
    strcpy(frame.address, "11111111");
    strcpy(frame.control, "00000000");
    strcpy(frame.fcs, "00000000000000000000000000000000");
    strcpy(frame.flagEnd, "01111110");
    frame.data[0] = '\0';
    fp = fopen("frames.txt", "w");
    if (fp == NULL)
    {
        printf("Unable to create file.\n");
        return 1;
    }
    printf("Enter number of frames: ");
    scanf("%d", &n);
    for (i = 0; i < n; i++)
    {
        printf("\nEnter binary data for Frame %d\n", i + 1);
        printf("(Enter data in multiples of 8 bits): ");
        scanf("%s", inputData);
        if (strlen(inputData) % 8 != 0)
        {
            printf("Error! Data must be a multiple of 8 bits.\n");
            i--;
            continue;
        }
        byteStuffing(inputData, frame.data);
        printf("\nFrame %d\n", i + 1);
        printf("Flag(Start): %s\n", frame.flagStart);
        printf("Address    : %s\n", frame.address);
        printf("Control    : %s\n", frame.control);
        printf("Data       : %s\n", frame.data);
        printf("FCS        : %s\n", frame.fcs);
        printf("Flag(End)  : %s\n", frame.flagEnd);
        fprintf(fp, "%s%s%s%s%s%s",frame.flagStart,frame.address,frame.control,frame.data,frame.fcs,frame.flagEnd);
    }
    fclose(fp);
    printf("\nAll frames written successfully to frames.txt\n");
    return 0;
}
