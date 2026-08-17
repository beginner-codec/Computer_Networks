[24bcs084@mepcolinux ex2]$cat checksumhead.h
#include <stdio.h>
#include <string.h>
#define MAX_BITS 1024
#define MAX_BLOCKS 128
void generateChecksum(char data[]);
void verifyChecksum(char data[]);
[24bcs084@mepcolinux ex2]$cat checksumimp.c
#include "checksumhead.h"
void createMatrix(char data[], int arr[MAX_BLOCKS][8], int *rows)
{
    int len = strlen(data);
    *rows = len / 8;
    int index = 0;
    int i,j;
    for (i = 0; i < *rows; i++)
    {
        for (j = 0; j < 8; j++)
        {
            arr[i][j] = data[index++] - '0';
        }
    }
}
void addBlocks(int sum[], int block[])
{
    int carry = 0;
    int i,j;
    for (i = 7; i >= 0; i--)
    {
        int temp = sum[i] + block[i] + carry;
        sum[i] = temp % 2;
        carry = temp / 2;
    }
    while (carry)
    {
        for (j= 7; j >= 0; j--)
        {
            int temp = sum[j] + carry;
            sum[j] = temp % 2;
            carry = temp / 2;
            if (carry == 0)
                break;
        }
    }
}
void onesComplement(int sum[])
{
    int i;
    for (i = 0; i < 8; i++)
    {
        sum[i] = !sum[i];
    }
}
void printBlock(int block[])
{
    int i;
    for (i = 0; i < 8; i++)
        printf("%d", block[i]);
    printf("\n");
}
void generateChecksum(char data[])
{
    int arr[MAX_BLOCKS][8];
    int rows,i;
    createMatrix(data, arr, &rows);
    printf("\n8-bit Blocks\n\n");
    for (i = 0; i < rows; i++)
        printBlock(arr[i]);
    int sum[8] = {0};
    for (i = 0; i < rows; i++)
        addBlocks(sum, arr[i]);
    printf("\nBinary Sum (after End-Around Carry) : ");
    for(i = 0; i < 8; i++)
        printf("%d", sum[i]);
    printf("\n");
    onesComplement(sum);
    printf("\nChecksum : ");
    for(i = 0; i < 8; i++)
        printf("%d", sum[i]);
    printf("\n");
}
void verifyChecksum(char data[])
{
    int arr[MAX_BLOCKS][8];
    int rows;
    int i;
    createMatrix(data, arr, &rows);
    printf("\nReceived 8-bit Blocks\n\n");
    for (i = 0; i < rows; i++)
        printBlock(arr[i]);
    int sum[8] = {0};
    for (i = 0; i < rows; i++)
        addBlocks(sum, arr[i]);
    printf("\nBinary Sum (after End-Around Carry) : ");
    for (i = 0; i < 8; i++)
        printf("%d", sum[i]);
    printf("\n");
    onesComplement(sum);
    printf("\nOne's Complement : ");
    int error = 0;
    for (i = 0; i < 8; i++)
    {
        printf("%d", sum[i]);
        if (sum[i] != 0)
            error = 1;
    }
    printf("\n");
    if (error)
        printf("\nError Detected.\n");
    else
        printf("\nNo Error Detected.\n");
}
[24bcs084@mepcolinux ex2]$cat checksumapp.c
#include "checksumhead.h"
int main()
{
    char data[MAX_BITS];
    char received[MAX_BITS];
    printf("CHECKSUM GENERATION\n");
    printf("\nEnter binary data (multiple of 8 bits): ");
    scanf("%s", data);
    if (strlen(data) % 8 != 0)
    {
        printf("Error! Data length must be a multiple of 8 bits.\n");
        return 1;
    }
    generateChecksum(data);
    printf("\n\nCHECKSUM VERIFICATION\n");
    printf("\nEnter the received data (including checksum): ");
    scanf("%s", received);
    if (strlen(received) % 8 != 0)
    {
        printf("Error! Data length must be a multiple of 8 bits.\n");
        return 1;
    }
    verifyChecksum(received);
    return 0;
}
