  [24bcs084@mepcolinux ex4]$cat dihead.h
#include<stdio.h>
#define MAX 100
#define INF 999999
void dijkstra(int graph[MAX][MAX], int n, int source,int distance[MAX], int parent[MAX]);
void printPath(int parent[MAX], int source, int destination);
[24bcs084@mepcolinux ex4]$cat diapp.c
#include "dihead.h"
int main()
{
    int graph[MAX][MAX];
    int distance[MAX];
    int parent[MAX];
    int n, source;
    printf("Enter the number of nodes: ");
    scanf("%d", &n);
    printf("\nEnter the %d x %d cost matrix:\n", n, n);
    int i,j;
    for (i = 0; i < n; i++)
    {
        for (j = 0; j < n; j++)
        {
            scanf("%d", &graph[i][j]);
        }
    }
    printf("\nEnter the source node (0 to %d): ", n - 1);
    scanf("%d", &source);
    if (source < 0 || source >= n)
    {
        printf("Invalid source node.\n");
        return 1;
    }
    dijkstra(graph, n, source, distance, parent);
    printf("\n============================================\n");
    printf("Shortest Paths from Source Node %d\n", source);
    printf("============================================\n");
    for (i = 0; i < n; i++)
    {
        printf("\nDestination: %d\n", i);

        if (distance[i] == INF)
        {
            printf("Minimum Cost : INF\n");
            printf("Path         : No path\n");
        }
        else
        {
            printf("Minimum Cost : %d\n", distance[i]);
            printf("Path         : ");
            printPath(parent, source, i);
            printf("\n");
        }
    }
   return 0;
}
[24bcs084@mepcolinux ex4]$cat diimp.c
#include "dihead.h"
int findMinDistance(int distance[MAX], int visited[MAX], int n)
{
    int min = INF;
    int minIndex = -1;
    int i;
    for (i = 0; i < n; i++)
    {
        if (!visited[i] && distance[i] < min)
        {
            min = distance[i];
            minIndex = i;
        }
    }
    return minIndex;
}
void dijkstra(int graph[MAX][MAX], int n, int source,int distance[MAX], int parent[MAX])
{
    int visited[MAX] = {0};
    int i;
    for (i = 0; i < n; i++)
    {
        distance[i] = INF;
        parent[i] = -1;
    }
    distance[source] = 0;
    int count,neighbor;
    for (count = 0; count < n - 1; count++)
    {
        int current = findMinDistance(distance, visited, n);
        if (current == -1)
            break;
        visited[current] = 1;
        for (neighbor = 0; neighbor < n; neighbor++)
        {
            if (!visited[neighbor] &&
                graph[current][neighbor] != 0 &&
                distance[current] != INF &&
                distance[current] + graph[current][neighbor]
                    < distance[neighbor])
            {
                distance[neighbor] =
                    distance[current] + graph[current][neighbor];

                parent[neighbor] = current;
            }
        }
    }
}
void printPath(int parent[MAX], int source, int destination)
{
    if (destination == source)
    {
        printf("%d", source);
        return;
    }
    if (parent[destination] == -1)
    {
        printf("No path");
        return;
    }
    printPath(parent, source, parent[destination]);
    printf(" -> %d", destination);
}
[24bcs084@mepcolinux ex4]$
