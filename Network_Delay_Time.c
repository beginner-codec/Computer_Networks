//743. Network Delay Time
int networkDelayTime(int** times, int timesSize, int* timesColSize, int n, int k) {
    int dist[101];
    int visited[101];
    int i,j;

    for(i=1;i<=n;i++) {
        dist[i]=1000000000;
        visited[i]=0;
    }

    dist[k]=0;

    for(i=1;i<=n;i++) {
        int u=-1;
        int min=1000000000;

        for(j=1;j<=n;j++) {
            if(!visited[j] && dist[j]<min) {
                min=dist[j];
                u=j;
            }
        }

        if(u==-1)
            break;

        visited[u]=1;

        for(j=0;j<timesSize;j++) {
            if(times[j][0]==u) {
                int v=times[j][1];
                int w=times[j][2];

                if(dist[u]+w<dist[v])
                    dist[v]=dist[u]+w;
            }
        }
    }

    int answer=0;

    for(i=1;i<=n;i++) {
        if(dist[i]==1000000000)
            return -1;

        if(dist[i]>answer)
            answer=dist[i];
    }

    return answer;
}
