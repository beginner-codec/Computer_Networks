#include<stdio.h>

#define INF 999

int main()
{
    int n,i,j,k;
    int cost[10][10],dist[10][10],next[10][10];

    printf("Enter number of routers: ");
    scanf("%d",&n);

    printf("Enter cost matrix:\n");
    for(i=0;i<n;i++)
    {
        for(j=0;j<n;j++)
        {
            scanf("%d",&cost[i][j]);

            dist[i][j]=cost[i][j];

            if(i==j)
                next[i][j]=i;
            else if(cost[i][j]!=INF)
                next[i][j]=j;
            else
                next[i][j]=-1;
        }
    }

    for(k=0;k<n;k++)
    {
        for(i=0;i<n;i++)
        {
            for(j=0;j<n;j++)
            {
                if(dist[i][k]!=INF && dist[k][j]!=INF)
                {
                    if(dist[i][j]>dist[i][k]+dist[k][j])
                    {
                        dist[i][j]=dist[i][k]+dist[k][j];
                        next[i][j]=next[i][k];
                    }
                }
            }
        }
    }

    printf("\nFinal Routing Tables:\n");

    for(i=0;i<n;i++)
    {
        printf("\nRouter %d:\n",i+1);
        printf("Destination\tCost\tNext Hop\n");

        for(j=0;j<n;j++)
        {
            if(dist[i][j]==INF)
                printf("%d\t\tINF\t-\n",j+1);
            else
                printf("%d\t\t%d\t%d\n",j+1,dist[i][j],next[i][j]+1);
        }
    }

    return 0;
}
