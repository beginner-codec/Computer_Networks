#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<string.h>

int isPowerOfTwo(int n)
{
    return n&&(n&(n-1))==0;
}

int main()
{
    char data[100];
    int code[200];
    int received[200];
    int parity[20];
    int m,r,n;
    int i,j,k;
    int pos;
    int syndrome;
    int p;
    int dataIndex;
    int errorPos;

    printf("Enter binary data: ");
    scanf("%s",data);

    m=strlen(data);
    r=0;

    while((1<<r)<m+r+1)
        r++;

    n=m+r;

    dataIndex=m-1;

    for(i=1;i<=n;i++)
    {
        if(isPowerOfTwo(i))
            code[i]=0;
        else
        {
            code[i]=data[dataIndex]-'0';
            dataIndex--;
        }
    }

    for(i=0;i<r;i++)
    {
        p=1<<i;
        parity[i]=0;

        for(j=p;j<=n;j++)
        {
            if(j&p)
                parity[i]^=code[j];
        }

        code[p]=parity[i];
    }

    printf("\nHamming Code: ");

    for(i=n;i>=1;i--)
        printf("%d",code[i]);

    printf("\n");

    printf("Parity Bits:\n");

    for(i=0;i<r;i++)
    {
        p=1<<i;
        printf("P%d = %d\n",p,code[p]);
    }

    for(i=1;i<=n;i++)
        received[i]=code[i];

    srand(time(NULL));

    errorPos=(rand()%n)+1;

    received[errorPos]^=1;

    printf("\nRandom Error Position: %d\n",errorPos);

    printf("Received Codeword: ");

    for(i=n;i>=1;i--)
        printf("%d",received[i]);

    printf("\n");

    syndrome=0;

    printf("\nSyndrome Calculation:\n");

    for(i=0;i<r;i++)
    {
        p=1<<i;
        parity[i]=0;

        for(j=p;j<=n;j++)
        {
            if(j&p)
                parity[i]^=received[j];
        }

        printf("P%d = %d\n",p,parity[i]);

        if(parity[i])
            syndrome+=p;
    }

    printf("Syndrome = %d\n",syndrome);

    if(syndrome==0)
    {
        printf("No Error Detected\n");
    }
    else
    {
        printf("Error Detected at Position: %d\n",syndrome);

        received[syndrome]^=1;

        printf("Corrected Codeword: ");

        for(i=n;i>=1;i--)
            printf("%d",received[i]);

        printf("\n");
    }

    dataIndex=0;

    printf("Recovered Data: ");

    for(i=n;i>=1;i--)
    {
        if(!isPowerOfTwo(i))
        {
            printf("%d",received[i]);
            dataIndex++;
        }
    }

    printf("\n");

    return 0;
}
