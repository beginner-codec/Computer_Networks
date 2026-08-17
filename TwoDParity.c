//Dimensional Parity Generation and Checking

//[24bcs084@mepcolinux ex2]$cat parityhead.h
#include <stdio.h>
#include <string.h>
#define MAX_BLOCKS 100
#define MAX_BITS 500
void generateParity(char data[]);
void checkParity();
[24bcs084@mepcolinux ex2]$cat parityimp.c
#include "parityhead.h"
void generateParity(char data[])
{
    int len = strlen(data);
    int rows = (len + 6) / 7;
    int matrix[MAX_BLOCKS + 1][8];
    int index = 0;
    int i,j;
    for(i=0;i<rows;i++)
    {
        int ones=0;
        for(j=0;j<7;j++)
        {
            if(index<len)
                matrix[i][j]=data[index++]-'0';
            else
                matrix[i][j]=0;
            if(matrix[i][j]==1)
                ones++;
        }
        if(ones%2==0)
            matrix[i][7]=1;
        else
            matrix[i][7]=0;
    }
    for(j=0;j<8;j++)
    {
        int ones=0;
        for(i=0;i<rows;i++)
        {
            if(matrix[i][j]==1)
                ones++;
        }
        if(ones%2==0)
            matrix[rows][j]=1;
        else
            matrix[rows][j]=0;
    }
    FILE *fp=fopen("parity.txt","w");
    if(fp==NULL)
    {
        printf("Cannot create file.\n");
        return;
    }
    printf("\nGenerated 2-D Odd Parity Matrix\n\n");
    for(i=0;i<=rows;i++)
    {
        for(j=0;j<8;j++)
        {
            printf("%d ",matrix[i][j]);
            fprintf(fp,"%d",matrix[i][j]);
        }
        printf("\n");
        fprintf(fp,"\n");
    }
    fclose(fp);
    printf("\nParity matrix stored in parity.txt\n");
}
void checkParity()
{
    int rows;
    printf("\nEnter number of DATA rows (excluding parity row): ");
    scanf("%d",&rows);
    int arr[MAX_BLOCKS+1][8];
    printf("\nEnter the received parity matrix (%d rows x 8 bits)\n",rows+1);
    int i,j;
    char row[9];
    for(i=0;i<=rows;i++)
    {
        printf("Row %d : ", i+1);
        scanf("%8s", row);

        if(strlen(row) != 8)
        {
            printf("Each row must contain exactly 8 bits.\n");
            return;
        }
        for(j=0;j<8;j++)
        {
            if(row[j] != '0' && row[j] != '1')
            {
                printf("Invalid bit entered.\n");
                return;
            }
            arr[i][j] = row[j] - '0';
        }
    }
    int error=0;
    printf("\nChecking Row Parity\n");
    for(i=0;i<rows;i++)
    {
        int ones=0;
        for(j=0;j<8;j++)
        {
            if(arr[i][j]==1)
                ones++;
        }
        if(ones%2==1)
            printf("Row %d : Correct\n",i+1);
        else
        {
            printf("Row %d : Error\n",i+1);
            error=1;
        }
    }
    printf("\nChecking Column Parity\n");
    for(j=0;j<8;j++)
    {
        int ones=0;
        for(i=0;i<=rows;i++)
        {
            if(arr[i][j]==1)
                ones++;
        }
        if(ones%2==1)
            printf("Column %d : Correct\n",j+1);
        else
        {
            printf("Column %d : Error\n",j+1);
            error=1;
        }
    }
    if(error)
        printf("\nError Detected.\n");
    else
        printf("\nNo Error Detected.\n");
}
[24bcs084@mepcolinux ex2]$cat parityapp.c
#include "parityhead.h"
int main()
{
    int choice,i;
    char data[MAX_BITS];
    do
    {
        printf("\n==============================");
        printf("\n      PARITY MENU");
        printf("\n==============================");
        printf("\n1. Generate 2-D Odd Parity");
        printf("\n2. Verify Parity");
        printf("\n3. Exit");
        printf("\nEnter your choice : ");
        scanf("%d",&choice);
        switch(choice)
        {
            case 1:
                printf("\nEnter binary data : ");
                scanf("%s",data);
                if(strlen(data)%7!=0) {
                   printf("Invalid data");
                   return 0;
                }
                for(i=0;data[i]!='\0';i++)
                {
                    if(data[i]!='0' && data[i]!='1')
                    {
                        printf("Invalid Binary Data\n");
                        return 0;
                    }
                }
                generateParity(data);
                break;
            case 2:
                checkParity();
                break;
            case 3:
                printf("\nProgram Ended.\n");
                break;
            default:
                printf("\nInvalid Choice.\n");
        }
    }while(choice!=3);
    return 0;
}
