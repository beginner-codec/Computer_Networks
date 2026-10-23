k_v_nirupa@LenovoLap:~/sem5/cn/ex10$ cat tcp_compare.c

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define FLOWS 2
#define ROUNDS 100
#define PACKET_SIZE 1000

double cwnd[2]={1,1};
double ssthresh[2]={32,32};

void updateNewReno(int i,int loss)
{
    if(loss)
    {
        ssthresh[i]=cwnd[i]/2.0;
        if(ssthresh[i]<2)
            ssthresh[i]=2;
        cwnd[i]=ssthresh[i];
    }
    else if(cwnd[i]<ssthresh[i])
        cwnd[i]*=2;
    else
        cwnd[i]+=1.0/cwnd[i];
}

void updateCubic(int i,int loss)
{
    if(loss)
    {
        cwnd[i]*=0.7;
        if(cwnd[i]<2)
            cwnd[i]=2;
        ssthresh[i]=cwnd[i];
    }
    else
        cwnd[i]+=0.4;
}

int main()
{
    int i,j,condition;
    int capacity;
    int sent[2],received[2],lost[2],retrans[2];
    double throughput[2],delay[2];
    double fairness;
    double totalThroughput;
    double errorRate;
    double totalSent,totalReceived;
    double avgDelay;
    double lossProbability;
    double rate[2];
    FILE *fp,*result;

    printf("TCP NEWRENO VS TCP CUBIC SIMULATION\n");
    printf("1. Low congestion\n");
    printf("2. Moderate congestion\n");
    printf("3. High congestion\n");
    printf("Choose condition: ");
    scanf("%d",&condition);

    if(condition==1)
    {
        capacity=100;
        errorRate=0.001;
    }
    else if(condition==2)
    {
        capacity=35;
        errorRate=0.02;
    }
    else
    {
        capacity=15;
        errorRate=0.08;
    }

    fp=fopen("cwnd_compare.dat","w");
    result=fopen("results.dat","w");

    if(fp==NULL || result==NULL)
    {
        printf("File opening error\n");
        return 1;
    }

    for(j=0;j<FLOWS;j++)
    {
        cwnd[j]=1;
        ssthresh[j]=32;
        sent[j]=received[j]=lost[j]=retrans[j]=0;
        throughput[j]=delay[j]=0;
    }

    for(i=0;i<ROUNDS;i++)
    {
        int offered[2];
        int totalOffered;
        int congestion;

        for(j=0;j<FLOWS;j++)
        {
            offered[j]=(int)cwnd[j];
            if(offered[j]<1)
                offered[j]=1;
        }

        totalOffered=offered[0]+offered[1];
        congestion=(totalOffered>capacity);

        for(j=0;j<FLOWS;j++)
        {
            int delivered;
            double probability=errorRate;

            sent[j]+=offered[j];

            if(congestion)
                probability+=0.20;

            if(probability>0.95)
                probability=0.95;

            delivered=0;

            for(int k=0;k<offered[j];k++)
            {
                double r=(double)rand()/RAND_MAX;

                if(totalOffered<=capacity && r>=errorRate)
                    delivered++;
                else if(totalOffered>capacity)
                {
                    if(r>=probability)
                        delivered++;
                }
            }

            received[j]+=delivered;
            lost[j]+=offered[j]-delivered;
            retrans[j]+=offered[j]-delivered;

            if(j==0)
                updateNewReno(j,delivered<offered[j]);
            else
                updateCubic(j,delivered<offered[j]);

            if(cwnd[j]>100)
                cwnd[j]=100;

            delay[j]+=20.0+5.0*totalOffered/capacity;

            fprintf(fp,"%d %d %.2f\n",i,j,cwnd[j]*PACKET_SIZE);
        }
    }

    totalSent=0;
    totalReceived=0;
    totalThroughput=0;
    avgDelay=0;

    printf("\nRESULTS\n");
    printf("------------------------------------------\n");

    for(j=0;j<FLOWS;j++)
    {
        throughput[j]=received[j]*PACKET_SIZE*8.0/
                      (ROUNDS*0.02*1000000.0);

        rate[j]=throughput[j];
        delay[j]/=ROUNDS;

        totalSent+=sent[j];
        totalReceived+=received[j];
        totalThroughput+=throughput[j];
        avgDelay+=delay[j];

        printf("%s\n",j==0?"TCP NewReno":"TCP CUBIC");
        printf("Packets sent: %d\n",sent[j]);
        printf("Packets received: %d\n",received[j]);
        printf("Packet loss: %d\n",lost[j]);
        printf("Retransmissions (estimated): %d\n",retrans[j]);
        printf("Throughput: %.4f Mbps\n",throughput[j]);
        printf("Average delay: %.2f ms\n",delay[j]);
        printf("------------------------------------------\n");

        fprintf(result,"%s %.4f %d %.2f %d\n",
                j==0?"NewReno":"CUBIC",
                throughput[j],lost[j],delay[j],retrans[j]);
    }

    if(rate[0]+rate[1]>0)
        fairness=pow(rate[0]+rate[1],2)/
                 (FLOWS*(rate[0]*rate[0]+rate[1]*rate[1]));
    else
        fairness=0;

    printf("Combined throughput: %.4f Mbps\n",totalThroughput);
    printf("Overall packet delivery ratio: %.2f %%\n",
           totalSent>0?totalReceived*100.0/totalSent:0);
    printf("Jain fairness index: %.4f\n",fairness);

    fclose(fp);
    fclose(result);

    printf("\nFiles created: cwnd_compare.dat, results.dat\n");

    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex10$ cat plt_compare.py

import matplotlib.pyplot as plt

data = {0: [], 1: []}

with open("cwnd_compare.dat") as f:
    for line in f:
        t, flow, cwnd = line.split()
        data[int(flow)].append((int(t), float(cwnd)))

plt.figure(figsize=(10, 5))

plt.plot(
    [p[0] for p in data[0]],
    [p[1] for p in data[0]],
    label="TCP NewReno"
)

plt.plot(
    [p[0] for p in data[1]],
    [p[1] for p in data[1]],
    label="TCP CUBIC"
)

plt.xlabel("Simulation Round")
plt.ylabel("Congestion Window (bytes)")
plt.title("TCP NewReno vs TCP CUBIC")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("cwnd_compare.png", dpi=300)
plt.show()

k_v_nirupa@LenovoLap:~/sem5/cn/ex10$
