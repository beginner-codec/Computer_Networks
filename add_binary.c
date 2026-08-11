#include<string.h>
#include<stdlib.h>

char*addBinary(char*a,char*b)
{
    int i,j,k,carry,sum;
    int len1,len2,len;
    char*result;

    len1=strlen(a);
    len2=strlen(b);
    len=len1>len2?len1:len2;

    result=(char*)malloc(len+2);

    i=len1-1;
    j=len2-1;
    k=len;
    carry=0;

    result[k+1]='\0';

    while(i>=0||j>=0||carry)
    {
        sum=carry;

        if(i>=0)
            sum+=a[i--]-'0';

        if(j>=0)
            sum+=b[j--]-'0';

        result[k--]=(sum%2)+'0';
        carry=sum/2;
    }

    if(k>=0)
        result[k]='0';

    return result+k+1;
}
