#include<stdio.h>
#include<string.h>

char data[100];
char generator[30];
char temp[100];
char remainder[30];
char codeword[100];

void xorOperation(int n)
{
    int i;

    for(i=1;i<n;i++)
    {
        if(temp[i]==generator[i])
            temp[i-1]='0';
        else
            temp[i-1]='1';
    }
}

void crc(char input[],char divisor[])
{
    int dataLen;
    int genLen;
    int i,j;

    dataLen=strlen(input);
    genLen=strlen(divisor);

    strcpy(temp,input);

    for(i=0;i<genLen;i++)
        remainder[i]=temp[i];

    for(i=0;i<dataLen-genLen+1;i++)
    {
        if(remainder[0]=='1')
        {
            for(j=0;j<genLen;j++)
                temp[j]=remainder[j];

            xorOperation(genLen);

            for(j=0;j<genLen-1;j++)
                remainder[j]=temp[j];

            remainder[genLen-1]=input[i+genLen];
        }
        else
        {
            for(j=1;j<genLen;j++)
                remainder[j-1]=remainder[j];

            remainder[genLen-1]=input[i+genLen];
        }
    }

    remainder[genLen-1]='\0';
}

int main()
{
    int dataLen;
    int genLen;
    int i;
    int errors;
    int pos;
    char received[100];
    char extended[100];

    printf("Enter binary data: ");
    scanf("%s",data);

    printf("Enter generator polynomial: ");
    scanf("%s",generator);

    dataLen=strlen(data);
    genLen=strlen(generator);

    strcpy(extended,data);

    for(i=0;i<genLen-1;i++)
        extended[dataLen+i]='0';

    extended[dataLen+genLen-1]='\0';

    crc(extended,generator);

    strcpy(codeword,data);

    for(i=0;i<genLen-1;i++)
        codeword[dataLen+i]=remainder[i];

    codeword[dataLen+genLen-1]='\0';

    printf("\nTransmitted Codeword : %s\n",codeword);

    strcpy(received,codeword);

    printf("Enter number of bit errors: ");
    scanf("%d",&errors);

    for(i=0;i<errors;i++)
    {
        printf("Enter bit position to flip (0-%d): ",(int)strlen(received)-1);
        scanf("%d",&pos);

        if(received[pos]=='0')
            received[pos]='1';
        else
            received[pos]='0';
    }

    printf("Received Codeword    : %s\n",received);

    crc(received,generator);

    printf("Remainder            : %s\n",remainder);

    for(i=0;i<genLen-1;i++)
    {
        if(remainder[i]!='0')
            break;
    }

    if(i==genLen-1)
        printf("Status               : No Error Detected\n");
    else
        printf("Status               : Error Detected\n");

    return 0;
}
