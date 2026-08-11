//477. Total Hamming Distance
int totalHammingDistance(int*nums,int numsSize)
{
    int i,j;
    int ones,zeros;
    int count;

    count=0;

    for(i=0;i<30;i++)
    {
        ones=0;
        zeros=0;

        for(j=0;j<numsSize;j++)
        {
            if((nums[j]>>i)&1)
                ones++;
            else
                zeros++;
        }

        count+=ones*zeros;
    }

    return count;
}
