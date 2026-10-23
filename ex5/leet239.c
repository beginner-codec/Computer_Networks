#include<stdlib.h>
int* maxSlidingWindow(int* nums,int numsSize,int k,int* returnSize)
{
    int *result=(int*)malloc((numsSize-k+1)*sizeof(int));
    int *deque=(int*)malloc(numsSize*sizeof(int));
    int front=0,rear=-1,i;
    *returnSize=numsSize-k+1;
    for(i=0;i<numsSize;i++)
    {
        while(front<=rear&&deque[front]<=i-k)
            front++;
        while(front<=rear&&nums[deque[rear]]<=nums[i])
            rear--;
        deque[++rear]=i;
        if(i>=k-1)
            result[i-k+1]=nums[deque[front]];
    }
    free(deque);
    return result;
}
