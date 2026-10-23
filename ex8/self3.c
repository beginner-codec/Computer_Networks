k_v_nirupa@LenovoLap:~/sem5/cn/ex8$ cat self3server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_MOVIES 3
#define MAX_SHOWS 2
#define MAX_BOOKINGS 500
#define BUF_SIZE 4096

typedef struct {
    int id;
    char time[20];
    int total_seats;
    int available_seats;
    double price;
} Show;

typedef struct {
    int id;
    char title[80];
    char language[30];
    int show_count;
    Show shows[MAX_SHOWS];
} Movie;

typedef struct {
    int id;
    int movie_id;
    int show_id;
    int tickets;
    double amount;
    int active;
} Booking;

static Movie movies[MAX_MOVIES] = {
    {1, "Leo", "Tamil", 2, {
        {1, "10:00 AM", 100, 100, 150.0},
        {2, "06:00 PM", 100, 100, 200.0}
    }},
    {2, "Interstellar", "English", 2, {
        {1, "01:00 PM", 80, 80, 180.0},
        {2, "08:00 PM", 80, 80, 250.0}
    }},
    {3, "Jailer", "Tamil", 2, {
        {1, "11:00 AM", 120, 120, 130.0},
        {2, "07:00 PM", 120, 120, 190.0}
    }}
};

static Booking bookings[MAX_BOOKINGS];
static int booking_count = 0;
static int next_booking_id = 1001;

/* Receive one complete newline-terminated TCP request. */
static int recv_line(int fd, char *buf, size_t size)
{
    size_t n = 0;

    while (n + 1 < size) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);

        if (r == 0)
            return n == 0 ? 0 : -1;

        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (c == '\n')
            break;
        if (c == '\r')
            continue;

        if ((unsigned char)c < 32 ||
            (unsigned char)c > 126)
            return -1;

        buf[n++] = c;
    }

    if (n + 1 >= size) {
        buf[0] = '\0';
        return -1;
    }

    buf[n] = '\0';
    return 1;
}

static int send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            return -1;

        sent += (size_t)n;
    }

    return 0;
}

static int send_response(int fd, const char *response)
{
    if (send_all(fd, response, strlen(response)) < 0)
        return -1;

    return send_all(fd, "\n.\n", 3);
}

static int find_movie(int id)
{
    for (int i = 0; i < MAX_MOVIES; i++)
        if (movies[i].id == id)
            return i;

    return -1;
}

static int find_show(int movie_index, int show_id)
{
    Movie *m = &movies[movie_index];

    for (int i = 0; i < m->show_count; i++)
        if (m->shows[i].id == show_id)
            return i;

    return -1;
}

static int find_booking(int id)
{
    for (int i = 0; i < booking_count; i++)
        if (bookings[i].id == id && bookings[i].active)
            return i;

    return -1;
}

static void process_request(const char *request,
                            char *out, size_t size)
{
    char command[20] = {0};
    int movie_id, show_id, tickets, booking_id;
    char extra;

    if (sscanf(request, "%19s", command) != 1) {
        snprintf(out, size, "ERROR: Empty request.");
        return;
    }

    /* FIXED: LIST now correctly accepts a request with no arguments. */
    if (strcmp(command, "LIST") == 0) {
        if (strcmp(request, "LIST") != 0) {
            snprintf(out, size, "ERROR: LIST takes no arguments.");
            return;
        }

        snprintf(out, size,
            "MOVIE LIST\n"
            "ID | Movie | Language\n"
            "1 | Leo | Tamil\n"
            "2 | Interstellar | English\n"
            "3 | Jailer | Tamil");
        return;
    }

    /* Display show timings, available seats and ticket prices. */
    if (strcmp(command, "SHOWS") == 0) {
        if (sscanf(request, "%19s %d %c",
                   command, &movie_id, &extra) != 2 ||
            movie_id <= 0) {
            snprintf(out, size, "ERROR: Usage: SHOWS movie_id");
            return;
        }

        int mi = find_movie(movie_id);

        if (mi < 0) {
            snprintf(out, size, "ERROR: Movie not found.");
            return;
        }

        Movie *m = &movies[mi];
        int used = snprintf(out, size,
            "SHOWS FOR %s (%s)\n"
            "Show ID | Time | Available Seats | Price\n",
            m->title, m->language);

        if (used < 0 || (size_t)used >= size) {
            snprintf(out, size, "ERROR: Response too large.");
            return;
        }

        for (int i = 0; i < m->show_count; i++) {
            Show *s = &m->shows[i];
            int n = snprintf(out + used, size - (size_t)used,
                "%d | %s | %d | Rs. %.2f\n",
                s->id, s->time, s->available_seats, s->price);

            if (n < 0 || (size_t)n >= size - (size_t)used)
                break;

            used += n;
        }
        return;
    }

    /* Check availability for a particular show. */
    if (strcmp(command, "AVAIL") == 0) {
        if (sscanf(request, "%19s %d %d %c",
                   command, &movie_id, &show_id, &extra) != 3 ||
            movie_id <= 0 || show_id <= 0) {
            snprintf(out, size,
                "ERROR: Usage: AVAIL movie_id show_id");
            return;
        }

        int mi = find_movie(movie_id);

        if (mi < 0) {
            snprintf(out, size, "ERROR: Movie not found.");
            return;
        }

        int si = find_show(mi, show_id);

        if (si < 0) {
            snprintf(out, size, "ERROR: Show not found.");
            return;
        }

        Show *s = &movies[mi].shows[si];

        snprintf(out, size,
            "SEAT AVAILABILITY\nMovie: %s\nShow: %s\n"
            "Available seats: %d\nTicket price: Rs. %.2f",
            movies[mi].title, s->time,
            s->available_seats, s->price);
        return;
    }

    /* Book tickets. */
    if (strcmp(command, "BOOK") == 0) {
        if (sscanf(request, "%19s %d %d %d %c",
                   command, &movie_id, &show_id,
                   &tickets, &extra) != 4 ||
            movie_id <= 0 || show_id <= 0 || tickets <= 0) {
            snprintf(out, size,
                "ERROR: Usage: BOOK movie_id show_id ticket_count");
            return;
        }

        int mi = find_movie(movie_id);

        if (mi < 0) {
            snprintf(out, size, "ERROR: Movie not found.");
            return;
        }

        int si = find_show(mi, show_id);

        if (si < 0) {
            snprintf(out, size, "ERROR: Show not found.");
            return;
        }

        Show *s = &movies[mi].shows[si];

        if (booking_count >= MAX_BOOKINGS) {
            snprintf(out, size, "ERROR: Booking storage is full.");
            return;
        }

        if (tickets > s->available_seats) {
            snprintf(out, size,
                "BOOKING FAILED: Only %d seats are available.",
                s->available_seats);
            return;
        }

        Booking *b = &bookings[booking_count];

        b->id = next_booking_id;
        b->movie_id = movie_id;
        b->show_id = show_id;
        b->tickets = tickets;
        b->amount = tickets * s->price;
        b->active = 1;

        s->available_seats -= tickets;
        booking_count++;
        next_booking_id++;

        snprintf(out, size,
            "BOOKING SUCCESSFUL!\n"
            "Booking ID: %d\nMovie: %s\nShow: %s\n"
            "Tickets: %d\nTotal amount: Rs. %.2f\n"
            "Remaining seats: %d",
            b->id, movies[mi].title, s->time,
            tickets, b->amount, s->available_seats);
        return;
    }

    /* Cancel a booking and restore the seats. */
    if (strcmp(command, "CANCEL") == 0) {
        if (sscanf(request, "%19s %d %c",
                   command, &booking_id, &extra) != 2 ||
            booking_id <= 0) {
            snprintf(out, size, "ERROR: Usage: CANCEL booking_id");
            return;
        }

        int bi = find_booking(booking_id);

        if (bi < 0) {
            snprintf(out, size,
                "CANCELLATION FAILED: Active booking not found.");
            return;
        }

        Booking *b = &bookings[bi];
        int mi = find_movie(b->movie_id);

        if (mi < 0) {
            snprintf(out, size, "ERROR: Booking data invalid.");
            return;
        }

        int si = find_show(mi, b->show_id);

        if (si < 0) {
            snprintf(out, size, "ERROR: Show data invalid.");
            return;
        }

        movies[mi].shows[si].available_seats += b->tickets;
        b->active = 0;

        snprintf(out, size,
            "CANCELLATION SUCCESSFUL!\n"
            "Booking ID: %d\nTickets cancelled: %d\n"
            "Seats restored: %d",
            b->id, b->tickets, b->tickets);
        return;
    }

    /* Display details of an active booking. */
    if (strcmp(command, "DETAILS") == 0) {
        if (sscanf(request, "%19s %d %c",
                   command, &booking_id, &extra) != 2 ||
            booking_id <= 0) {
            snprintf(out, size, "ERROR: Usage: DETAILS booking_id");
            return;
        }

        int bi = find_booking(booking_id);

        if (bi < 0) {
            snprintf(out, size,
                "ERROR: Active booking not found.");
            return;
        }

        Booking *b = &bookings[bi];
        int mi = find_movie(b->movie_id);
        int si = mi >= 0 ? find_show(mi, b->show_id) : -1;

        if (mi < 0 || si < 0) {
            snprintf(out, size, "ERROR: Booking data invalid.");
            return;
        }

        Show *s = &movies[mi].shows[si];

        snprintf(out, size,
            "BOOKING DETAILS\n"
            "Booking ID: %d\nMovie: %s\nShow: %s\n"
            "Tickets: %d\nPrice per ticket: Rs. %.2f\n"
            "Total amount: Rs. %.2f\nStatus: ACTIVE",
            b->id, movies[mi].title, s->time,
            b->tickets, s->price, b->amount);
        return;
    }

    snprintf(out, size,
        "ERROR: Unknown command. Use LIST, SHOWS, AVAIL, "
        "BOOK, CANCEL, DETAILS or QUIT.");
}

int main(int argc, char *argv[])
{
    int port = 8080;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        char *end;
        long p = strtol(argv[1], &end, 10);

        if (*end || p < 1 || p > 65535) {
            fprintf(stderr, "Invalid port number.\n");
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
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Movie Ticket TCP Server listening on port %d\n", port);
    fflush(stdout);

    /* Iterative server: process one client at a time. */
    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
            (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));

        printf("Client connected: %s:%u\n",
               ip, (unsigned)ntohs(client_addr.sin_port));
        fflush(stdout);

        for (;;) {
            char request[BUF_SIZE];
            char response[BUF_SIZE];

            int r = recv_line(client_fd, request, sizeof(request));

            if (r <= 0)
                break;

            printf("Request from %s: %s\n", ip, request);
            fflush(stdout);

            if (strcmp(request, "QUIT") == 0) {
                send_all(client_fd, "Goodbye!\n", 9);
                break;
            }

            process_request(request, response, sizeof(response));

            if (send_response(client_fd, response) < 0)
                break;
        }

        printf("Client disconnected: %s\n", ip);
        fflush(stdout);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex8$ cat self3client.c

/* movie_client.c
 * TCP Movie Ticket Booking Client
 * Compile: gcc -Wall -Wextra movie_client.c -o movie_client
 * Run:     ./movie_client 127.0.0.1 8080
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUF_SIZE 4096

static int send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("send");
            return -1;
        }

        if (n == 0)
            return -1;

        sent += (size_t)n;
    }

    return 0;
}

/* Send a request as one newline-terminated TCP protocol message. */
static int send_request(int fd, const char *request)
{
    if (send_all(fd, request, strlen(request)) < 0)
        return -1;

    return send_all(fd, "\n", 1);
}

/* Read one line from TCP. */
static int recv_line(int fd, char *buf, size_t size)
{
    size_t n = 0;

    while (n + 1 < size) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);

        if (r == 0)
            return 0;

        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (c == '\n')
            break;

        if (c == '\r')
            continue;

        buf[n++] = c;
    }

    buf[n] = '\0';
    return 1;
}

/* A response ends when the server sends a line containing ".". */
static int receive_response(int fd)
{
    char line[BUF_SIZE];

    printf("\n========== SERVER RESPONSE ==========\n");

    for (;;) {
        int r = recv_line(fd, line, sizeof(line));

        if (r <= 0) {
            if (r < 0)
                perror("recv");
            else
                printf("Server disconnected.\n");
            return -1;
        }

        if (strcmp(line, ".") == 0)
            break;

        puts(line);
    }

    printf("=====================================\n");
    return 0;
}

static int read_number(const char *prompt, int *value)
{
    char input[128];
    char extra;

    printf("%s", prompt);
    fflush(stdout);

    if (!fgets(input, sizeof(input), stdin))
        return 0;

    if (strchr(input, '\n') == NULL) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
            ;
        printf("Input is too long.\n");
        return -1;
    }

    if (sscanf(input, "%d %c", value, &extra) != 1 ||
        *value <= 0) {
        printf("Please enter a positive integer.\n");
        return -1;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server-ip> <port>\n", argv[0]);
        return 1;
    }

    char *end;
    long port = strtol(argv[2], &end, 10);

    if (*end || port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port number.\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);

    if (inet_pton(AF_INET, argv[1],
                  &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address.\n");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("Connected to Movie Ticket Booking Server.\n");

    for (;;) {
        int choice;

        printf("\n===== MOVIE TICKET BOOKING =====\n");
        printf("1. View movie list\n");
        printf("2. View show timings and prices\n");
        printf("3. Check seat availability\n");
        printf("4. Book tickets\n");
        printf("5. Cancel booking\n");
        printf("6. Display booking details\n");
        printf("7. Exit\n");

        int rc = read_number("Enter choice: ", &choice);

        if (rc == 0)
            break;

        if (rc < 0 || choice > 7) {
            printf("Choose an option from 1 to 7.\n");
            continue;
        }

        char request[BUF_SIZE];
        int movie_id, show_id, tickets, booking_id;

        if (choice == 1) {
            snprintf(request, sizeof(request), "LIST");
        } else if (choice == 2) {
            rc = read_number("Enter movie ID: ", &movie_id);
            if (rc == 0) break;
            if (rc < 0) continue;

            snprintf(request, sizeof(request), "SHOWS %d", movie_id);
        } else if (choice == 3) {
            rc = read_number("Enter movie ID: ", &movie_id);
            if (rc == 0) break;
            if (rc < 0) continue;

            rc = read_number("Enter show ID: ", &show_id);
            if (rc == 0) break;
            if (rc < 0) continue;

            snprintf(request, sizeof(request),
                     "AVAIL %d %d", movie_id, show_id);
        } else if (choice == 4) {
            rc = read_number("Enter movie ID: ", &movie_id);
            if (rc == 0) break;
            if (rc < 0) continue;

            rc = read_number("Enter show ID: ", &show_id);
            if (rc == 0) break;
            if (rc < 0) continue;

            rc = read_number("Enter number of tickets: ", &tickets);
            if (rc == 0) break;
            if (rc < 0) continue;

            snprintf(request, sizeof(request),
                     "BOOK %d %d %d",
                     movie_id, show_id, tickets);
        } else if (choice == 5) {
            rc = read_number("Enter booking ID to cancel: ",
                             &booking_id);
            if (rc == 0) break;
            if (rc < 0) continue;

            snprintf(request, sizeof(request),
                     "CANCEL %d", booking_id);
        } else if (choice == 6) {
            rc = read_number("Enter booking ID: ", &booking_id);
            if (rc == 0) break;
            if (rc < 0) continue;

            snprintf(request, sizeof(request),
                     "DETAILS %d", booking_id);
        } else {
            if (send_request(sock, "QUIT") < 0)
                fprintf(stderr, "Could not send exit request.\n");
            else {
                char reply[BUF_SIZE];
                int r = recv_line(sock, reply, sizeof(reply));
                if (r > 0)
                    printf("%s\n", reply);
            }
            break;
        }

        printf("Sending request: %s\n", request);

        if (send_request(sock, request) < 0) {
            fprintf(stderr, "Failed to send request.\n");
            break;
        }

        if (receive_response(sock) < 0)
            break;
    }

    close(sock);
    printf("Client closed.\n");
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex8$
