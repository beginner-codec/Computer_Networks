k_v_nirupa@LenovoLap:~/sem5/cn/ex10$ cat tcp_congestion.c

#include <stdio.h>
#include <stdlib.h>

#define TOTAL 1000
#define QUEUE_SIZE 20
#define PACKET_SIZE 1000
#define SIM_TIME 20

int main()
{
    int i;
    int cwnd=1;
    int ssthresh=32;
    int queue=0;
    int sent=0,received=0,lost=0;
    int delivered[TOTAL]={0};
    double time=0;
    double delay=0;
    double throughput;
    double pdr;
    double avgDelay;
    double bandwidth=2.0;
    double linkDelay=0.020;

    FILE *fp=fopen("cwnd.dat","w");

    if(fp==NULL)
    {
        printf("File error\n");
        return 1;
    }

    printf("TCP CONGESTION CONTROL SIMULATION\n");
    printf("---------------------------------\n");

    for(i=0;i<TOTAL;i++)
    {
        sent++;

        if(queue<QUEUE_SIZE)
        {
            queue++;
            delivered[i]=1;
        }
        else
        {
            lost++;
            ssthresh=cwnd/2;

            if(ssthresh<2)
                ssthresh=2;

            cwnd=ssthresh;
        }

        if(i%10==0 && queue>0)
        {
            queue--;
            received++;
            delay+=linkDelay;
        }

        if(cwnd<ssthresh)
            cwnd=cwnd*2;
        else
            cwnd++;

        if(cwnd>QUEUE_SIZE)
            cwnd=QUEUE_SIZE;

        time+=0.02;
        fprintf(fp,"%lf %d\n",time,cwnd*PACKET_SIZE);
    }

    fclose(fp);

    throughput=(received*PACKET_SIZE*8.0)/(SIM_TIME*1000000.0);
    pdr=(sent>0)?(received*100.0/sent):0;
    avgDelay=(received>0)?(delay*1000.0/received):0;

    printf("Packets sent: %d\n",sent);
    printf("Packets received: %d\n",received);
    printf("Packets lost: %d\n",lost);
    printf("Throughput: %.4f Mbps\n",throughput);
    printf("Packet Delivery Ratio: %.2f %%\n",pdr);
    printf("Average Delay: %.4f ms\n",avgDelay);
    printf("Congestion window saved in cwnd.dat\n");

    return 0;
}
k_v_nirupa@LenovoLap:~/sem5/cn/ex10$ cat plot_cwnd.py

import matplotlib.pyplot as plt

times = []
cwnd = []

with open("cwnd.dat", "r") as file:
    for line in file:
        values = line.split()
        if len(values) >= 2:
            times.append(float(values[0]))
            cwnd.append(int(values[1]))

if not times:
    print("No congestion window data found.")
else:
    plt.figure(figsize=(10, 5))
    plt.plot(times, cwnd, marker=".", markersize=3)
    plt.xlabel("Simulation Time (seconds)")
    plt.ylabel("Congestion Window (bytes)")
    plt.title("TCP NewReno Congestion Window vs Time")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("cwnd.png", dpi=300)
    plt.show()

k_v_nirupa@LenovoLap:~/sem5/cn/ex10$
