      Implementation of Iterative Server

[24bcs084@mepcolinux ex8]$ cat its.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#define IP "127.0.0.1"
#define PORT 8080
#define MAX_DATA 50
[24bcs084@mepcolinux ex8]$ cat server.c
#include "its.h"

int main() {
    int serverfd, clientfd;
    struct sockaddr_in server;
    int opt = 1;
    int client_count = 0;

    serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(serverfd < 0) {
        printf("Socket creation failed!\n");
        return 1;
    }

    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(IP);

    if(bind(serverfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Bind failed!\n");
        close(serverfd);
        return 1;
    }

    if(listen(serverfd, 3) < 0) {
        printf("Listen failed!\n");
        close(serverfd);
        return 1;
    }

    printf("Server running at %s:%d\n", IP, PORT);
    printf("Maximum pending connection queue: 3\n\n");

    while(1) {
        printf("Waiting for clients...\n");
        fflush(stdout);

        clientfd = accept(serverfd, NULL, NULL);
        if(clientfd < 0) {
            printf("Accept failed!\n");
            continue;
        }

        client_count++;

        printf("Client %d connected.\n\n", client_count);
        fflush(stdout);

        int k = fork();

        if(k < 0) {
            printf("Fork failed!\n");
            close(clientfd);
            continue;
        }

        if(k == 0) {
            // Child: Send messages to the client
            close(serverfd);

            while(1) {
                char s[MAX_DATA] = {0};

                // No input prompt; type directly in the terminal.
                if(fgets(s, sizeof(s), stdin) == NULL)
                    strcpy(s, "STOP");

                s[strcspn(s, "\n")] = '\0';

                int sent = 0;

                while(sent < MAX_DATA) {
                    int n = send(clientfd, s + sent,
                                 MAX_DATA - sent, 0);
                    if(n <= 0)
                        break;
                    sent += n;
                }

                if(sent < MAX_DATA || strcmp(s, "STOP") == 0)
                    break;
            }

            close(clientfd);
            exit(0);
        }

        // Parent: Receive messages from the client
        while(1) {
            char s[MAX_DATA] = {0};
            int received = 0;

            while(received < MAX_DATA) {
                int n = recv(clientfd, s + received,
                             MAX_DATA - received, 0);
                if(n <= 0)
                    break;
                received += n;
            }

            if(received != MAX_DATA) {
                printf("\nClient %d disconnected.\n", client_count);
                fflush(stdout);
                break;
            }

            s[MAX_DATA - 1] = '\0';

            printf("\nClient %d: %s\n\n", client_count, s);
            fflush(stdout);

            if(strcmp(s, "STOP") == 0) {
                printf("Client %d ended the chat.\n", client_count);
                fflush(stdout);
                break;
            }
        }

        shutdown(clientfd, SHUT_RDWR);
        close(clientfd);

        kill(k, SIGTERM);
        waitpid(k, NULL, 0);

        printf("Session ended.\n\n");
        fflush(stdout);
    }

    close(serverfd);
    return 0;
}
[24bcs084@mepcolinux ex8]$ cat client.c
#include "its.h"

int main() {
    int sockfd;
    struct sockaddr_in server;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0) {
        printf("Socket creation failed!\n");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(IP);

    if(connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Connection failed!\n");
        close(sockfd);
        return 1;
    }

    printf("Connected to Server...\n\n");

    int k = fork();

    if(k < 0) {
        printf("Fork failed!\n");
        close(sockfd);
        return 1;
    }

    if(k == 0) {
        // Child: Send messages to the server
        while(1) {
            char s[MAX_DATA] = {0};

            // No input prompt; type directly in the terminal.
            if(fgets(s, sizeof(s), stdin) == NULL)
                strcpy(s, "STOP");

            s[strcspn(s, "\n")] = '\0';

            int sent = 0;

            while(sent < MAX_DATA) {
                int n = send(sockfd, s + sent,
                             MAX_DATA - sent, 0);
                if(n <= 0)
                    break;
                sent += n;
            }

            if(sent < MAX_DATA || strcmp(s, "STOP") == 0)
                break;
        }

        close(sockfd);
        exit(0);
    }

    // Parent: Receive messages from the server
    while(1) {
        char s[MAX_DATA] = {0};
        int received = 0;

        while(received < MAX_DATA) {
            int n = recv(sockfd, s + received,
                         MAX_DATA - received, 0);
            if(n <= 0)
                break;
            received += n;
        }

        if(received != MAX_DATA) {
            printf("\nServer disconnected.\n");
            fflush(stdout);
            break;
        }

        s[MAX_DATA - 1] = '\0';

        printf("\nServer: %s\n\n", s);
        fflush(stdout);

        if(strcmp(s, "STOP") == 0) {
            printf("Server ended the chat.\n");
            fflush(stdout);
            break;
        }
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    kill(k, SIGTERM);
    waitpid(k, NULL, 0);

    return 0;
}
[24bcs084@mepcolinux ex8]$
