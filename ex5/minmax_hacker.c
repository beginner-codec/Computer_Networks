
#include <stdio.h>
#include <stdlib.h>

long* riddle(int arr_count,long* arr,int* result_count)
{
    int n=arr_count;
    int *stack=malloc(n*sizeof(int));
    int *left=malloc(n*sizeof(int));
    int *right=malloc(n*sizeof(int));
    long *ans=calloc(n+1,sizeof(long));
    long *result=malloc(n*sizeof(long));
    int i,top=-1,len;

    for(i=0;i<n;i++)
    {
        while(top!=-1 && arr[stack[top]]>=arr[i])
            top--;
        left[i]=(top==-1)?-1:stack[top];
        stack[++top]=i;
    }

    top=-1;

    for(i=n-1;i>=0;i--)
    {
        while(top!=-1 && arr[stack[top]]>=arr[i])
            top--;
        right[i]=(top==-1)?n:stack[top];
        stack[++top]=i;
    }

    for(i=0;i<n;i++)
    {
        len=right[i]-left[i]-1;
        if(ans[len]<arr[i])
            ans[len]=arr[i];
    }

    for(i=n-1;i>=1;i--)
    {
        if(ans[i]<ans[i+1])
            ans[i]=ans[i+1];
    }

    for(i=0;i<n;i++)
        result[i]=ans[i+1];

    *result_count=n;

    free(stack);
    free(left);
    free(right);
    free(ans);

    return result;
}

int main()
{
    int n,i,result_count;
    scanf("%d",&n);

    long *arr=malloc(n*sizeof(long));

    for(i=0;i<n;i++)
        scanf("%ld",&arr[i]);

    long *result=riddle(n,arr,&result_count);

    for(i=0;i<result_count;i++)
    {
        if(i>0)
            printf(" ");
        printf("%ld",result[i]);
    }

    printf("\n");

    free(arr);
    free(result);

    return 0;
}
