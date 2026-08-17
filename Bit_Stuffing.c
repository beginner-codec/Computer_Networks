//Bit Stuffing

[24bcs084@mepcolinux exe1]$cat boStuffhead.h
#include<stdio.h>
#include<string.h>
#define MAX_DATA 500
#define MAX_STUFFED 1000
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
void bitStuffing(char input[], char output[]);
[24bcs084@mepcolinux exe1]$cat boStuffimp.c
#include "boStuffhead.h"
struct Frame frame;
void bitStuffing(char input[], char output[])
{
    int i = 0, j = 0;
    while (input[i] != '\0')
    {
        output[j++] = input[i];
        if (input[i] == '0')
        {
            int count = 0;
            int k;
            for (k = 1; k <= 5; k++)
            {
                if (input[i + k] == '1')
                    count++;
                else
                    break;
            }
            if (count == 5)
            {
                for (k = 1; k <= 5; k++)
                    output[j++] = input[i + k];
                output[j++] = '0';
                i += 5;
            }
        }
        i++;
   }
    output[j] = '\0';
}
[24bcs084@mepcolinux exe1]$cat boStuffapp.c
#include "boStuffhead.h"
int main()
{
    int n, i;
    char inputData[MAX_DATA];
    FILE *fp;
    strcpy(frame.flagStart, "01111110");
    strcpy(frame.flagEnd, "01111110");
    strcpy(frame.address, "00000000");
    strcpy(frame.control, "11111111");
    strcpy(frame.fcs, "00000000000000000000000000000000");
    frame.data[0] = '\0';
    fp = fopen("frames.txt", "w");
    if (fp == NULL)
    {
        printf("Unable to create file.\n");
        return 1;
    }
    printf("Enter the number of frames: ");
    scanf("%d", &n);
    for (i = 0; i < n; i++)
    {
        printf("\nEnter binary data for Frame %d: ", i + 1);
        scanf("%s", inputData);
        bitStuffing(inputData, frame.data);
        printf("\nFrame %d\n", i + 1);
        printf("Flag (Start) : %s\n", frame.flagStart);
        printf("Address      : %s\n", frame.address);
        printf("Control      : %s\n", frame.control);
        printf("Data         : %s\n", frame.data);
        printf("FCS          : %s\n", frame.fcs);
        printf("Flag (End)   : %s\n", frame.flagEnd);
        fprintf(fp, "%s%s%s%s%s%s",frame.flagStart,frame.address,frame.control,frame.data,frame.fcs,frame.flagEnd);
    }
    fclose(fp);
    printf("\nAll frames have been written to frames.txt successfully.\n");
    return 0;
}
