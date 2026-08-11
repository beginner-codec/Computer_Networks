// 461. Hamming Distance
int hammingDistance(int x,int y)
{
    int n;
    int count;

    n=x^y;
    count=0;

    while(n!=0)
    {
        count+=n&1;
        n=n>>1;
    }

    return count;
}
