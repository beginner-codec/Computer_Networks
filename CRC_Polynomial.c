CRC Polynomial
[24bcs084@mepcolinux ex2]$cat boStuffhead.h
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define MAX_DATA 500
#define MAX_STUFFED 1000
#define MAX 1000
#define DIVISOR_DEGREE 8
struct Frame
{
    char flagStart[9];
    char address[9];
    char control[9];
    char data[MAX_STUFFED];
    char fcs[9];
    char flagEnd[9];
};
extern struct Frame frame;
void bitStuffing(char input[], char output[]);
void binaryToPolynomial(char bits[], int poly[]);
void printPolynomial(int poly[]);
int highestDegree(int poly[]);
void polynomialDivision(int dividend[], int remainder[]);
void generateFCS(char data[], char fcs[]);
[24bcs084@mepcolinux ex2]$cat boStuffimp.c
#include "boStuffhead.h"
int divisor[9] = {1,1,1,0,0,0,0,0,1};
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
void binaryToPolynomial(char bits[], int poly[])
{
    int len = strlen(bits);
    int i;
    /* Initialize polynomial */
    for(i = 0; i < MAX; i++)
        poly[i] = 0;

    /* Convert binary to polynomial coefficients */
    for(i = 0; i < len; i++)
    {
        if(bits[i] == '1')
        {
            int degree = len - 1 - i;
            poly[degree] = 1;
        }
    }
}

void printPolynomial(int poly[])
{
    int first = 1;
    int highest = 0;
    int i;
    /* Find highest degree */
    for(i = MAX-1; i >= 0; i--)
    {
        if(poly[i] == 1)
        {
            highest = i;
            break;
        }
    }

    /* Print polynomial */
    for(i = highest; i >= 0; i--)
    {
        if(poly[i] == 1)
        {
            if(!first)
                printf(" + ");

            if(i == 0)
                printf("1");
            else if(i == 1)
                printf("x");
            else
                printf("x^%d", i);

            first = 0;
        }
    }

    if(first)
        printf("0");
}
/* Find highest degree of a polynomial */
int highestDegree(int poly[])
{
   int i;
    for(i = MAX-1; i >= 0; i--)
    {
        if(poly[i] != 0)
            return i;
    }

    return 0;
}
/* Polynomial Division using Absolute Value Subtraction */
void polynomialDivision(int dividend[], int remainder[])
{
    int i;
    int work[MAX];
    /* Copy dividend into working array */
    for(i = 0; i < MAX; i++)
        work[i] = dividend[i];
    int highest = highestDegree(work);
    /* Continue division until degree becomes less than divisor degree */
    while(highest >= DIVISOR_DEGREE)
    {
        int shift = highest - DIVISOR_DEGREE;
        /* Multiply shifted divisor and subtract */
        for(i = 0; i <= DIVISOR_DEGREE; i++)
        {
            if(divisor[i] == 1)
            {
                int position = i + shift;
                /* Absolute value subtraction */
                work[position] = abs(work[position] - divisor[i]);
            }
        }
        /* Find next highest degree */
        highest = highestDegree(work);
    }
    /* Copy final remainder */
    for(i = 0; i < MAX; i++)
        remainder[i] = work[i];
}
/* Generate CRC-8 Frame Check Sequence */
void generateFCS(char data[], char fcs[])
{
    int i,j;
    int dataPoly[MAX];
    int augmented[MAX];
    int remainder[MAX];
    int highest;
    /* Convert binary data to polynomial */
    binaryToPolynomial(data, dataPoly);
    /* Initialize augmented polynomial */
    for(i = 0; i < MAX; i++)
        augmented[i] = 0;
    /* Multiply by x^8 (append eight zeros) */
    for(i = 0; i < MAX - 8; i++)
        augmented[i + 8] = dataPoly[i];
    printf("\n-----------------------------------");
    printf("\nCRC-8 FCS Generation");
    printf("\n-----------------------------------");
    printf("\n\nData Polynomial : ");
    printPolynomial(dataPoly);
    printf("\n\nAugmented Polynomial : ");
    printPolynomial(augmented);
    /* Perform Polynomial Division */
    polynomialDivision(augmented, remainder);
    printf("\n\nCRC Remainder : ");
    printPolynomial(remainder);
    highest = highestDegree(remainder);
    for(i = 0; i < 8; i++)
        fcs[i] = '0';
    fcs[8] = '\0';
    for(i = 0; i <= highest && i < 8; i++)
    {
        if(remainder[i] == 1)
            fcs[7 - i] = '1';
    }
    printf("\n\nGenerated FCS : %s\n", fcs);
}
[24bcs084@mepcolinux ex2]$cat boStuffapp.c
#include"boStuffhead.h"
struct Frame frame;
int main()
{
    int n, i;
    char inputData[MAX_DATA];
    FILE *fp;
    strcpy(frame.flagStart, "01111110");
    strcpy(frame.flagEnd, "01111110");
    strcpy(frame.address, "00000000");
    strcpy(frame.control, "11111111");
    frame.fcs[0] = '\0';
    frame.data[0] = '\0';
    fp = fopen("frames.txt", "w");
    if(fp == NULL)
    {
        printf("Unable to create file.\n");
        return 1;
    }
    printf("Enter number of frames : ");
    scanf("%d",&n);
    for(i=0;i<n;i++)
    {
        printf("\nEnter binary data for Frame %d : ",i+1);
        scanf("%s",inputData);
        bitStuffing(inputData,frame.data);
        generateFCS(frame.data,frame.fcs);
        printf("\nFrame %d\n",i+1);
        printf("----------------------------\n");
        printf("Flag(Start) : %s\n",frame.flagStart);
        printf("Address     : %s\n",frame.address);
        printf("Control     : %s\n",frame.control);
        printf("Data        : %s\n",frame.data);
        printf("FCS         : %s\n",frame.fcs);
        printf("Flag(End)   : %s\n",frame.flagEnd);
        fprintf(fp,"%s%s%s%s%s%s\n",frame.flagStart,frame.address,frame.control,frame.data,frame.fcs,frame.flagEnd);
    }
    fclose(fp);
    printf("\nFrames written successfully.\n");
    return 0;
}
