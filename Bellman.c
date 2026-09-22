[24bcs084@mepcolinux ex4]$cat bellhead.h
#include <stdio.h>

#define MAX 100
#define INF 999999

int bellmanFord(int graph[MAX][MAX], int n, int source, int distance[MAX], int parent[MAX]);
void printPath(int parent[MAX], int source, int destination);
void displayIteration(int iter, int n, int source, int distance[MAX], int parent[MAX]);
[24bcs084@mepcolinux ex4]$cat bellimp.c
#include "bellhead.h"

// Safe path printing: Uses an array to store path and checks for cycles
// to prevent Segmentation Faults.
void printPath(int parent[MAX], int source, int destination) {
    int path[MAX];
    int visited[MAX] = {0};
    int count = 0;
    int curr = destination;

    while (curr != -1) {
        if (visited[curr]) {
            printf("[Cycle Detected]");
            return;
        }
        visited[curr] = 1;
        path[count++] = curr;
        if (curr == source) break;
        curr = parent[curr];
    }
    int i;
    if (count > 0 && path[count - 1] == source) {
        for (i = count - 1; i >= 0; i--) {
            printf("%d", path[i]);
            if (i > 0) printf(" -> ");
        }
    } else {
        printf("No path");
    }
}

void displayIteration(int iter, int n, int source, int distance[MAX], int parent[MAX]) {
    printf("\nIteration %d:\n", iter);
    printf("%-10s | %-10s | %-20s\n", "Node", "Cost", "Path");
    printf("----------------------------------------------------------\n");
    int i;
    for (i = 0; i < n; i++) {
        printf("%-10d | ", i);
        if (distance[i] >= INF) {
            printf("%-10s | %-20s\n", "INF", "No path");
        } else {
            printf("%-10d | ", distance[i]);
            printPath(parent, source, i);
            printf("\n");
        }
    }
    printf("----------------------------------------------------------\n");
}

int bellmanFord(int graph[MAX][MAX], int n, int source, int distance[MAX], int parent[MAX]) {
    int i, u, v;

    for (i = 0; i < n; i++) {
        distance[i] = INF;
        parent[i] = -1;
    }
    distance[source] = 0;

    displayIteration(0, n, source, distance, parent);

    for (i = 1; i <= n - 1; i++) {
        int changed = 0;
        for (u = 0; u < n; u++) {
            for (v = 0; v < n; v++) {
                // If there is an edge and the source node is reachable
                if (graph[u][v] != 0 && distance[u] != INF) {
                    if (distance[u] + graph[u][v] < distance[v]) {
                        distance[v] = distance[u] + graph[u][v];
                        parent[v] = u;
                        changed = 1;
                    }
                }
            }
        }
        displayIteration(i, n, source, distance, parent);
        if (!changed) {
            printf("(No changes in this iteration. Algorithm converged early.)\n");
            break; // Optimization: If no changes, further iterations won't change anything
        }
    }

    // Final Step: Check for negative cycles
    for (u = 0; u < n; u++) {
        for (v = 0; v < n; v++) {
            if (graph[u][v] != 0 && distance[u] != INF) {
                if (distance[u] + graph[u][v] < distance[v]) {
                    return 0; // Negative cycle detected
                }
            }
        }
    }
    return 1;
}
[24bcs084@mepcolinux ex4]$cat bellapp.c
#include "bellhead.h"

int main() {
    int graph[MAX][MAX];
    int distance[MAX];
    int parent[MAX];
    int n, source, i, j;

    printf("Enter the number of nodes: ");
    if (scanf("%d", &n) != 1) return 1;

    printf("\nEnter the %d x %d cost matrix (0 for no edge):\n", n, n);
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            scanf("%d", &graph[i][j]);
        }
    }

    printf("\nEnter the source node: ");
    scanf("%d", &source);

    printf("\n--- Starting Bellman-Ford Process ---\n");

    if (bellmanFord(graph, n, source, distance, parent)) {
        printf("\nFinal Result: All shortest paths found successfully.\n");
    } else {
        printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        printf("WARNING: Negative weight cycle detected!\n");
        printf("The costs and paths above are unstable and will decrease infinitely.\n");
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }

    return 0;
}
[24bcs084@mepcolinux ex4]$  
