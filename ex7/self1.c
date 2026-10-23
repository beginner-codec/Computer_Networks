k_v_nirupa@LenovoLap:~/sem5/cn/ex7$ cat self1server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define SIZE 1024

int main(void)
{
    int sockfd;
    char buffer[SIZE];
    char response[SIZE];

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    printf("Arithmetic UDP server listening on port %d\n", PORT);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        memset(response, 0, sizeof(response));

        int n = recvfrom(sockfd, buffer, SIZE - 1, 0,
                         (struct sockaddr *)&client_addr,
                         &client_len);

        if (n < 0) {
            perror("Receive failed");
            continue;
        }

        buffer[n] = '\0';

        /*
           Client request format:
           request_id|number1|operator|number2

           Example:
           1|10|+|20
        */

        int request_id;
        double a, b, result;
        char op;
        char extra;

        int fields = sscanf(buffer, "%d|%lf|%c|%lf %c",
                            &request_id, &a, &op, &b, &extra);

        if (fields != 4) {
            snprintf(response, sizeof(response),
                     "0|ACK|ERROR: Malformed request.");
        }
        else {
            switch (op) {
                case '+':
                    result = a + b;
                    snprintf(response, sizeof(response),
                             "%d|ACK|Result = %.2f",
                             request_id, result);
                    break;

                case '-':
                    result = a - b;
                    snprintf(response, sizeof(response),
                             "%d|ACK|Result = %.2f",
                             request_id, result);
                    break;

                case '*':
                    result = a * b;
                    snprintf(response, sizeof(response),
                             "%d|ACK|Result = %.2f",
                             request_id, result);
                    break;

                case '/':
                    if (b == 0.0) {
                        snprintf(response, sizeof(response),
                                 "%d|ACK|ERROR: Division by zero.",
                                 request_id);
                    }
                    else {
                        result = a / b;
                        snprintf(response, sizeof(response),
                                 "%d|ACK|Result = %.2f",
                                 request_id, result);
                    }
                    break;

                default:
                    snprintf(response, sizeof(response),
                             "%d|ACK|ERROR: Invalid operator.",
                             request_id);
            }
        }

        sendto(sockfd, response, strlen(response), 0,
               (struct sockaddr *)&client_addr, client_len);

        printf("Request: %s\nResponse: %s\n\n",
               buffer, response);
    }

    close(sockfd);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex7$ cat self1client.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 8080
#define SIZE 1024

int main(void)
{
    int sockfd;
    int request_id = 1;
    char buffer[SIZE];
    char response[SIZE];

    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    /* Set a 3-second receive timeout */
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        perror("Setting timeout failed");
        close(sockfd);
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (1) {
        double a, b;
        char op;

        printf("\n--- Arithmetic Operations ---\n");
        printf("Enter operation (+, -, *, /) or q to quit: ");

        if (scanf(" %c", &op) != 1)
            break;

        if (op == 'q' || op == 'Q')
            break;

        if (op != '+' && op != '-' &&
            op != '*' && op != '/') {
            printf("Invalid operator! Try again.\n");
            continue;
        }

        printf("Enter two numbers: ");

        if (scanf("%lf %lf", &a, &b) != 2) {
            printf("Invalid number input.\n");
            break;
        }

        /*
           Request format:
           request_id|number1|operator|number2

           Example:
           1|10|+|20
        */

        snprintf(buffer, sizeof(buffer),
                 "%d|%.15g|%c|%.15g",
                 request_id, a, op, b);

        int received = 0;

        /* Send request and retry once if no response arrives */
        for (int attempt = 0; attempt < 2; attempt++) {
            if (sendto(sockfd, buffer, strlen(buffer), 0,
                       (struct sockaddr *)&server_addr,
                       sizeof(server_addr)) < 0) {
                perror("Send failed");
                break;
            }

            memset(response, 0, sizeof(response));

            int n = recvfrom(sockfd, response, SIZE - 1, 0,
                             (struct sockaddr *)&server_addr,
                             &server_len);

            if (n >= 0) {
                response[n] = '\0';

                int reply_id;
                char ack[16];
                char message[SIZE];
                char extra;

                if (sscanf(response, "%d|%15[^|]|%1023[^\n]%c",
                           &reply_id, ack, message, &extra) >= 3
                    && reply_id == request_id
                    && strcmp(ack, "ACK") == 0) {

                    printf("Server response: %s\n", message);
                    received = 1;
                    break;
                }

                printf("Invalid server response.\n");
                received = 1;
                break;
            }

            printf("Timeout. Retrying request...\n");
        }

        if (!received)
            printf("Server not responding. Please try again.\n");

        request_id++;
    }

    printf("Client disconnected.\n");

    close(sockfd);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex7$
