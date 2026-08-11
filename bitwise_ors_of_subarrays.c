#include<stdlib.h>

#define SIZE 4194304

unsigned int hashValue(unsigned int x)
{
    x^=x>>16;
    x*=0x7feb352d;
    x^=x>>15;
    x*=0x846ca68b;
    x^=x>>16;
    return x;
}

int subarrayBitwiseORs(int*arr,int arrSize)
{
    int*prev;
    int*curr;
    int*table;
    int prevSize,currSize;
    int i,j,value,index;
    int count;

    prev=(int*)malloc(sizeof(int)*32);
    curr=(int*)malloc(sizeof(int)*32);
    table=(int*)calloc(SIZE,sizeof(int));

    prevSize=0;
    count=0;

    for(i=0;i<arrSize;i++)
    {
        currSize=0;

        curr[currSize++]=arr[i];

        for(j=0;j<prevSize;j++)
        {
            value=prev[j]|arr[i];

            if(curr[currSize-1]!=value)
                curr[currSize++]=value;
        }

        for(j=0;j<currSize;j++)
        {
            value=curr[j]+1;
            index=hashValue((unsigned int)value)&(SIZE-1);

            while(table[index]!=0)
            {
                if(table[index]==value)
                    break;

                index=(index+1)&(SIZE-1);
            }

            if(table[index]==0)
            {
                table[index]=value;
                count++;
            }
        }

        for(j=0;j<currSize;j++)
            prev[j]=curr[j];

        prevSize=currSize;
    }

    free(prev);
    free(curr);
    free(table);

    return count;
}
