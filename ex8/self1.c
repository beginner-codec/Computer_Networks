k_v_nirupa@LenovoLap:~/sem5/cn/ex8$ cat self1server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_TRAINS 4
#define MAX_BOOKINGS 500
#define BUF_SIZE 1024

typedef struct {
    int id;
    char name[40];
    char source[30];
    char destination[30];
    int total_seats;
    int available_seats;
    double fare;
} Train;

typedef struct {
    int id;
    int train_id;
    int seats;
    double amount;
    int active;
    char passenger[50];
} Booking;

Train trains[MAX_TRAINS] = {
    {101, "Chennai Express", "Chennai", "Bengaluru", 100, 100, 450.0},
    {102, "Kovai Express", "Chennai", "Coimbatore", 80, 80, 550.0},
    {103, "Pandian Express", "Chennai", "Madurai", 90, 90, 500.0},
    {104, "Cheran Express", "Coimbatore", "Chennai", 70, 70, 480.0}
};

Booking bookings[MAX_BOOKINGS];
int booking_count = 0;
int next_booking_id = 1001;

Train *find_train(int id) {
    for (int i = 0; i < MAX_TRAINS; i++)
        if (trains[i].id == id)
            return &trains[i];
    return NULL;
}

Booking *find_booking(int id) {
    for (int i = 0; i < booking_count; i++)
        if (bookings[i].id == id)
            return &bookings[i];
    return NULL;
}

void log_activity(const char *client, const char *request,
                  const char *result) {
    FILE *fp = fopen("railway.log", "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];

    if (tm_info)
        strftime(timestamp, sizeof(timestamp),
                 "%Y-%m-%d %H:%M:%S", tm_info);
    else
        strcpy(timestamp, "unknown-time");

    fprintf(fp, "[%s] Client=%s | Request=%s | Result=%s\n",
            timestamp, client, request, result);
    fclose(fp);
}

int send_all(int fd, const char *data) {
    size_t total = 0, len = strlen(data);

    while (total < len) {
        ssize_t n = send(fd, data + total, len - total, 0);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

/* Read one newline-terminated request. */
int recv_line(int fd, char *buf, size_t size) {
    size_t i = 0;
    char ch;

    while (i < size - 1) {
        ssize_t n = recv(fd, &ch, 1, 0);
        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (ch == '\n') break;
        if (ch != '\r') buf[i++] = ch;
    }

    buf[i] = '\0';
    return 1;
}

/* Every response ends with a line containing only a dot. */
void respond(int fd, const char *message) {
    send_all(fd, message);
    send_all(fd, "\n.\n");
}

void process_request(int fd, const char *request,
                     const char *client_ip) {
    char cmd[20] = "";
    char passenger[50] = "";
    char extra[2];
    int train_id, seats, booking_id;
    char response[BUF_SIZE];
    const char *result = "Invalid request";

    /* SEARCH */
    if (strcmp(request, "SEARCH") == 0) {
        snprintf(response, sizeof(response),
                 "Available trains:\n");

        for (int i = 0; i < MAX_TRAINS; i++) {
            char line[200];
            snprintf(line, sizeof(line),
                     "Train %d: %s | %s to %s | Fare: %.2f | Seats: %d\n",
                     trains[i].id, trains[i].name,
                     trains[i].source, trains[i].destination,
                     trains[i].fare, trains[i].available_seats);
            if (strlen(response) + strlen(line) < sizeof(response))
                strcat(response, line);
        }
        result = "Train search successful";
        respond(fd, response);
    }

    /* AVAIL train_id */
    else if (sscanf(request, "%19s %d %1s", cmd,
                    &train_id, extra) == 2 &&
             strcmp(cmd, "AVAIL") == 0) {
        Train *t = find_train(train_id);

        if (!t) {
            result = "Train not found";
            respond(fd, "ERROR: Train not found");
        } else {
            snprintf(response, sizeof(response),
                     "Train %d: %s\nRoute: %s to %s\n"
                     "Available seats: %d\nFare per seat: %.2f",
                     t->id, t->name, t->source, t->destination,
                     t->available_seats, t->fare);
            result = "Availability checked";
            respond(fd, response);
        }
    }

    /* BOOK train_id seats passenger_name (no spaces in name) */
    else if (sscanf(request, "%19s %d %d %49s %1s",
                    cmd, &train_id, &seats, passenger, extra) == 4 &&
             strcmp(cmd, "BOOK") == 0) {
        Train *t = find_train(train_id);

        if (!t) {
            result = "Train not found";
            respond(fd, "ERROR: Invalid train ID");
        } else if (seats <= 0 || seats > t->total_seats) {
            result = "Invalid seat count";
            respond(fd, "ERROR: Seat count must be positive and valid");
        } else if (seats > t->available_seats) {
            result = "Insufficient seats";
            respond(fd, "ERROR: Not enough seats available");
        } else if (booking_count >= MAX_BOOKINGS) {
            result = "Booking database full";
            respond(fd, "ERROR: Booking database is full");
        } else {
            Booking *b = &bookings[booking_count++];
            b->id = next_booking_id++;
            b->train_id = train_id;
            b->seats = seats;
            b->amount = seats * t->fare;
            b->active = 1;
            snprintf(b->passenger, sizeof(b->passenger),
                     "%s", passenger);

            t->available_seats -= seats;

            snprintf(response, sizeof(response),
                     "Booking successful!\nBooking ID: %d\n"
                     "Passenger: %s\nTrain: %s\nSeats: %d\n"
                     "Total fare: %.2f",
                     b->id, b->passenger, t->name,
                     b->seats, b->amount);
            result = "Booking successful";
            respond(fd, response);
        }
    }

    /* CANCEL booking_id */
    else if (sscanf(request, "%19s %d %1s",
                    cmd, &booking_id, extra) == 2 &&
             strcmp(cmd, "CANCEL") == 0) {
        Booking *b = find_booking(booking_id);

        if (!b || !b->active) {
            result = "Booking not found or already cancelled";
            respond(fd, "ERROR: Booking not found or already cancelled");
        } else {
            Train *t = find_train(b->train_id);
            if (t) t->available_seats += b->seats;
            b->active = 0;

            snprintf(response, sizeof(response),
                     "Booking %d cancelled successfully",
                     booking_id);
            result = "Cancellation successful";
            respond(fd, response);
        }
    }

    /* STATUS booking_id */
    else if (sscanf(request, "%19s %d %1s",
                    cmd, &booking_id, extra) == 2 &&
             strcmp(cmd, "STATUS") == 0) {
        Booking *b = find_booking(booking_id);

        if (!b) {
            result = "Booking not found";
            respond(fd, "ERROR: Booking not found");
        } else {
            snprintf(response, sizeof(response),
                     "Booking ID: %d\nPassenger: %s\nTrain ID: %d\n"
                     "Seats: %d\nFare: %.2f\nStatus: %s",
                     b->id, b->passenger, b->train_id, b->seats,
                     b->amount, b->active ? "CONFIRMED" : "CANCELLED");
            result = "Booking status viewed";
            respond(fd, response);
        }
    }

    else if (strcmp(request, "QUIT") == 0) {
        result = "Client requested disconnect";
        respond(fd, "Goodbye!");
    }

    else {
        respond(fd,
            "ERROR: Invalid request.\n"
            "Use SEARCH, AVAIL train_id, "
            "BOOK train_id seats passenger_name, "
            "CANCEL booking_id, STATUS booking_id, or QUIT");
    }

    log_activity(client_ip, request, result);
}

int main(int argc, char *argv[]) {
    int port = 8080;

    if (argc > 1) {
        char *end;
        long p = strtol(argv[1], &end, 10);
        if (*end || p < 1 || p > 65535) {
            fprintf(stderr, "Invalid port number\n");
            return 1;
        }
        port = (int)p;
    }

    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Railway server listening on port %d\n", port);

    /* Iterative server: only one client is served at a time. */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                    (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = "unknown";
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  client_ip, sizeof(client_ip));
        printf("Client connected: %s\n", client_ip);

        char request[BUF_SIZE];

        /* This loop must finish before the next accept(). */
        while (1) {
            int n = recv_line(client_fd, request, sizeof(request));
            if (n <= 0) break;

            process_request(client_fd, request, client_ip);

            if (strcmp(request, "QUIT") == 0)
                break;
        }

        close(client_fd);
        printf("Client disconnected: %s\n", client_ip);
    }

    close(server_fd);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex8$ cat self1client.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int send_all(int fd, const char *data) {
    size_t sent = 0, len = strlen(data);

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

int receive_response(int fd) {
    char line[BUF_SIZE];
    size_t i = 0;

    while (1) {
        char ch;
        ssize_t n = recv(fd, &ch, 1, 0);

        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            return -1;
        }

        if (ch == '\n') {
            line[i] = '\0';

            if (strcmp(line, ".") == 0)
                return 1;

            puts(line);
            i = 0;
        } else if (ch != '\r' && i < sizeof(line) - 1) {
            line[i++] = ch;
        }
    }
}

int main(int argc, char *argv[]) {
    const char *ip = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 8080;

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port\n");
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons((unsigned short)port);

    if (inet_pton(AF_INET, ip, &server.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address\n");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&server,
                sizeof(server)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Connected to Railway Reservation Server\n");

    char request[BUF_SIZE];
    int choice;

    while (1) {
        printf("\n--- RAILWAY RESERVATION MENU ---\n");
        printf("1. Search trains\n");
        printf("2. Check seat availability\n");
        printf("3. Book tickets\n");
        printf("4. Cancel reservation\n");
        printf("5. View booking status\n");
        printf("6. Exit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            printf("Invalid choice\n");
            continue;
        }

        int id, seats;
        char name[50];

        switch (choice) {
        case 1:
            strcpy(request, "SEARCH");
            break;

        case 2:
            printf("Enter train ID: ");
            if (scanf("%d", &id) != 1) {
                printf("Invalid train ID\n");
                while (getchar() != '\n') {}
                continue;
            }
            snprintf(request, sizeof(request), "AVAIL %d", id);
            break;

        case 3:
            printf("Enter train ID: ");
            if (scanf("%d", &id) != 1) {
                printf("Invalid train ID\n");
                while (getchar() != '\n') {}
                continue;
            }

            printf("Enter number of seats: ");
            if (scanf("%d", &seats) != 1) {
                printf("Invalid seat count\n");
                while (getchar() != '\n') {}
                continue;
            }

            printf("Enter passenger name (no spaces): ");
            if (scanf("%49s", name) != 1) {
                printf("Invalid passenger name\n");
                continue;
            }

            snprintf(request, sizeof(request),
                     "BOOK %d %d %s", id, seats, name);
            break;

        case 4:
            printf("Enter booking ID to cancel: ");
            if (scanf("%d", &id) != 1) {
                printf("Invalid booking ID\n");
                while (getchar() != '\n') {}
                continue;
            }
            snprintf(request, sizeof(request), "CANCEL %d", id);
            break;

        case 5:
            printf("Enter booking ID: ");
            if (scanf("%d", &id) != 1) {
                printf("Invalid booking ID\n");
                while (getchar() != '\n') {}
                continue;
            }
            snprintf(request, sizeof(request), "STATUS %d", id);
            break;

        case 6:
            strcpy(request, "QUIT");
            break;

        default:
            printf("Please select 1 to 6\n");
            continue;
        }

        strcat(request, "\n");

        if (send_all(sockfd, request) < 0) {
            perror("send");
            break;
        }

        if (receive_response(sockfd) <= 0) {
            printf("Server disconnected\n");
            break;
        }

        if (choice == 6)
            break;
    }

    close(sockfd);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex8$
