/* server.c - UDP Library Management Server
 * Compile: gcc -Wall -Wextra -O2 server.c -o server
 * Run:     ./server 8080
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_BOOKS 100
#define MAX_PACKET 4096
#define MAX_RESPONSE 3500
#define CACHE_SIZE 256
#define DB_FILE "library.txt"
#define LOG_FILE "activity.log"

typedef struct {
    int id;
    char title[80];
    char author[60];
    int total;
    int issued;
} Book;

typedef struct {
    int valid;
    struct sockaddr_in client;
    uint64_t request_id;
    char response[MAX_RESPONSE];
} CacheEntry;

static Book books[MAX_BOOKS];
static int book_count = 0;
static CacheEntry cache[CACHE_SIZE];
static unsigned next_cache = 0;

static void log_activity(const char *ip, const char *action,
                         const char *result)
{
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) {
        perror("open activity.log");
        return;
    }

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S", &tm_now);

    fprintf(f, "[%s] client=%s action=%s result=%s\n",
            timestamp, ip, action, result);
    fclose(f);
}

/* Create a starter database if the file does not exist. */
static int init_database(void)
{
    FILE *f = fopen(DB_FILE, "r");
    if (f) {
        fclose(f);
        return 0;
    }

    f = fopen(DB_FILE, "w");
    if (!f) return -1;

    fprintf(f, "101|Computer Networks|Tanenbaum|5|0\n");
    fprintf(f, "102|Operating Systems|Silberschatz|4|0\n");
    fprintf(f, "103|C Programming|Kernighan Ritchie|3|0\n");
    fprintf(f, "104|Database Systems|Elmasri|2|0\n");
    fclose(f);
    return 0;
}

static int load_database(void)
{
    FILE *f = fopen(DB_FILE, "r");
    if (!f) return -1;

    book_count = 0;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        Book b = {0};

        if (sscanf(line, "%d|%79[^|]|%59[^|]|%d|%d",
                   &b.id, b.title, b.author,
                   &b.total, &b.issued) == 5) {
            if (book_count >= MAX_BOOKS ||
                b.id <= 0 || b.total < 0 ||
                b.issued < 0 || b.issued > b.total) {
                fclose(f);
                return -1;
            }
            books[book_count++] = b;
        } else {
            fclose(f);
            return -1;
        }
    }

    if (ferror(f)) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

/* Save to a temporary file and rename it to avoid partial writes. */
static int save_database(void)
{
    FILE *f = fopen("library.tmp", "w");
    if (!f) return -1;

    for (int i = 0; i < book_count; i++) {
        if (fprintf(f, "%d|%s|%s|%d|%d\n",
                    books[i].id, books[i].title,
                    books[i].author, books[i].total,
                    books[i].issued) < 0) {
            fclose(f);
            unlink("library.tmp");
            return -1;
        }
    }

    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        fclose(f);
        unlink("library.tmp");
        return -1;
    }

    if (fclose(f) != 0) {
        unlink("library.tmp");
        return -1;
    }

    if (rename("library.tmp", DB_FILE) != 0) {
        unlink("library.tmp");
        return -1;
    }

    return 0;
}

static int find_book(int id)
{
    for (int i = 0; i < book_count; i++)
        if (books[i].id == id) return i;
    return -1;
}

static int same_client(const struct sockaddr_in *a,
                       const struct sockaddr_in *b)
{
    return a->sin_addr.s_addr == b->sin_addr.s_addr &&
           a->sin_port == b->sin_port;
}

static CacheEntry *find_cached(const struct sockaddr_in *client,
                               uint64_t request_id)
{
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid &&
            cache[i].request_id == request_id &&
            same_client(&cache[i].client, client))
            return &cache[i];
    }
    return NULL;
}

static void cache_response(const struct sockaddr_in *client,
                           uint64_t request_id,
                           const char *response)
{
    CacheEntry *e = &cache[next_cache];
    e->valid = 1;
    e->client = *client;
    e->request_id = request_id;
    snprintf(e->response, sizeof(e->response), "%s", response);
    next_cache = (next_cache + 1) % CACHE_SIZE;
}

static void process_request(const char *command, const char *arg,
                            char *out, size_t out_size)
{
    int id;

    if (strcmp(command, "LIST") == 0) {
        size_t used = (size_t)snprintf(out, out_size,
            "Available books (ID | Title | Author | Available)\n");

        for (int i = 0; i < book_count; i++) {
            int available = books[i].total - books[i].issued;
            if (available <= 0) continue;

            if (used >= out_size) break;
            int n = snprintf(out + used, out_size - used,
                "%d | %s | %s | %d\n",
                books[i].id, books[i].title,
                books[i].author, available);

            if (n < 0 || (size_t)n >= out_size - used) {
                used = out_size;
                break;
            }
            used += (size_t)n;
        }

        if (used < out_size && used == strlen(
            "Available books (ID | Title | Author | Available)\n")) {
            snprintf(out + used, out_size - used,
                     "No books are currently available.\n");
        }
        return;
    }

    if (strcmp(command, "SEARCH") == 0) {
        if (!arg[0]) {
            snprintf(out, out_size, "ERROR: SEARCH requires a keyword.");
            return;
        }

        int found = 0;
        size_t used = (size_t)snprintf(out, out_size, "Search results:\n");

        for (int i = 0; i < book_count; i++) {
            if (strcasestr(books[i].title, arg) ||
                strcasestr(books[i].author, arg)) {
                int available = books[i].total - books[i].issued;
                if (used >= out_size) break;

                int n = snprintf(out + used, out_size - used,
                    "%d | %s | %s | Available: %d\n",
                    books[i].id, books[i].title,
                    books[i].author, available);

                if (n < 0 || (size_t)n >= out_size - used) break;
                used += (size_t)n;
                found = 1;
            }
        }

        if (!found)
            snprintf(out, out_size, "No matching books found.");
        return;
    }

    if (strcmp(command, "ISSUE") != 0 &&
        strcmp(command, "RETURN") != 0) {
        snprintf(out, out_size, "ERROR: Unknown command.");
        return;
    }

    char extra;
    if (sscanf(arg, "%d %c", &id, &extra) != 1 || id <= 0) {
        snprintf(out, out_size,
                 "ERROR: A valid positive book ID is required.");
        return;
    }

    int index = find_book(id);
    if (index < 0) {
        snprintf(out, out_size, "ERROR: Book ID %d does not exist.", id);
        return;
    }

    Book *b = &books[index];

    if (strcmp(command, "ISSUE") == 0) {
        if (b->issued >= b->total) {
            snprintf(out, out_size,
                     "ERROR: No copies of '%s' are available.", b->title);
            return;
        }

        b->issued++;
        if (save_database() != 0) {
            b->issued--;
            snprintf(out, out_size,
                     "ERROR: Database save failed; issue cancelled.");
            return;
        }

        snprintf(out, out_size, "SUCCESS: Issued '%s' (ID %d).",
                 b->title, b->id);
        return;
    }

    if (b->issued <= 0) {
        snprintf(out, out_size,
                 "ERROR: No issued copy of '%s' to return.", b->title);
        return;
    }

    b->issued--;
    if (save_database() != 0) {
        b->issued++;
        snprintf(out, out_size,
                 "ERROR: Database save failed; return cancelled.");
        return;
    }

    snprintf(out, out_size, "SUCCESS: Returned '%s' (ID %d).",
             b->title, b->id);
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

    if (init_database() != 0 || load_database() != 0) {
        fprintf(stderr, "Cannot initialize/load %s\n", DB_FILE);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons((uint16_t)port);

    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("Library UDP server listening on port %d\n", port);

    for (;;) {
        char packet[MAX_PACKET];
        struct sockaddr_in client = {0};
        socklen_t client_len = sizeof(client);

        ssize_t n = recvfrom(sock, packet, sizeof(packet) - 1, 0,
                             (struct sockaddr *)&client, &client_len);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recvfrom");
            continue;
        }
        packet[n] = '\0';

        char ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));

        uint64_t req_id = 0;
        char command[16] = {0};
        char arg[256] = {0};
        char response[MAX_RESPONSE] = {0};
        char wire_response[MAX_RESPONSE + 64] = {0};

        /* Reject malformed requests; never execute partial commands. */
        char *sep = strchr(packet, '|');
        if (!sep) {
            snprintf(wire_response, sizeof(wire_response),
                     "0|ACK|ERROR: Malformed request.");
            sendto(sock, wire_response, strlen(wire_response), 0,
                   (struct sockaddr *)&client, client_len);
            log_activity(ip, "MALFORMED", "rejected");
            continue;
        }

        *sep = '\0';
        char *end = NULL;
        errno = 0;
        unsigned long long parsed = strtoull(packet, &end, 10);

        if (errno || !packet[0] || !end || *end ||
            parsed == 0 || strchr(sep + 1, '|') == NULL) {
            snprintf(wire_response, sizeof(wire_response),
                     "0|ACK|ERROR: Malformed request.");
            sendto(sock, wire_response, strlen(wire_response), 0,
                   (struct sockaddr *)&client, client_len);
            log_activity(ip, "MALFORMED", "rejected");
            continue;
        }
        req_id = (uint64_t)parsed;

        char *arg_sep = strchr(sep + 1, '|');
        *arg_sep = '\0';

        if (strlen(sep + 1) >= sizeof(command) ||
            strlen(arg_sep + 1) >= sizeof(arg)) {
            snprintf(wire_response, sizeof(wire_response),
                     "%" PRIu64 "|ACK|ERROR: Request too long.", req_id);
            sendto(sock, wire_response, strlen(wire_response), 0,
                   (struct sockaddr *)&client, client_len);
            log_activity(ip, "MALFORMED", "rejected");
            continue;
        }

        strcpy(command, sep + 1);
        strcpy(arg, arg_sep + 1);

        /* A retransmission gets the original response, not a second action. */
        CacheEntry *old = find_cached(&client, req_id);
        if (old) {
            sendto(sock, old->response, strlen(old->response), 0,
                   (struct sockaddr *)&client, client_len);
            log_activity(ip, command, "duplicate request; cached response");
            continue;
        }

        process_request(command, arg, response, sizeof(response));
        snprintf(wire_response, sizeof(wire_response),
                 "%" PRIu64 "|ACK|%s", req_id, response);

        cache_response(&client, req_id, wire_response);
        log_activity(ip, command, response);

        if (sendto(sock, wire_response, strlen(wire_response), 0,
                   (struct sockaddr *)&client, client_len) < 0)
            perror("sendto");
    }

    close(sock);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex7$ cat self3client.c

/* client.c - UDP Library Management Client
 * Compile: gcc -Wall -Wextra -O2 client.c -o client
 * Run:     ./client 127.0.0.1 8080
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_PACKET 4096
#define TIMEOUT_SEC 2
#define MAX_RETRIES 3

static uint64_t next_request_id(void)
{
    static uint64_t counter = 0;
    if (counter == 0) {
        counter = ((uint64_t)time(NULL) << 20) ^
                  ((uint64_t)getpid() << 8);
        if (counter == 0) counter = 1;
    }
    return ++counter;
}

static int send_request(int sock, struct sockaddr_in *server,
                        const char *command, const char *arg)
{
    uint64_t req_id = next_request_id();
    char request[MAX_PACKET];
    char response[MAX_PACKET];

    int n = snprintf(request, sizeof(request), "%" PRIu64 "|%s|%s",
                     req_id, command, arg);

    if (n < 0 || (size_t)n >= sizeof(request)) {
        fprintf(stderr, "Request is too long.\n");
        return -1;
    }

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        if (sendto(sock, request, (size_t)n, 0,
                   (struct sockaddr *)server, sizeof(*server)) < 0) {
            perror("sendto");
            continue;
        }

        printf("Request sent (ID=%" PRIu64
               ", attempt %d/%d)\n",
               req_id, attempt, MAX_RETRIES);

        struct timeval timeout = {TIMEOUT_SEC, 0};
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int ready = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (ready < 0) {
            if (errno == EINTR) {
                attempt--;
                continue;
            }
            perror("select");
            return -1;
        }

        if (ready == 0) {
            printf("Timeout: retransmitting request...\n");
            continue;
        }

        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        ssize_t received = recvfrom(sock, response,
                                    sizeof(response) - 1, 0,
                                    (struct sockaddr *)&from, &from_len);
        if (received < 0) {
            if (errno == EINTR) {
                attempt--;
                continue;
            }
            perror("recvfrom");
            continue;
        }

        /* Ignore datagrams not sent by the configured server. */
        if (from.sin_addr.s_addr != server->sin_addr.s_addr ||
            from.sin_port != server->sin_port) {
            attempt--;
            continue;
        }

        response[received] = '\0';

        char expected[32];
        snprintf(expected, sizeof(expected), "%" PRIu64, req_id);

        char *first = strchr(response, '|');
        if (!first) {
            attempt--;
            continue;
        }
        *first = '\0';

        if (strcmp(response, expected) != 0) {
            /* Stale response for an earlier request. */
            attempt--;
            continue;
        }

        char *second = strchr(first + 1, '|');
        if (!second) {
            fprintf(stderr, "Invalid server response.\n");
            return -1;
        }
        *second = '\0';

        if (strcmp(first + 1, "ACK") != 0) {
            fprintf(stderr, "Invalid acknowledgment.\n");
            return -1;
        }

        printf("\n--- Server response (ACK received) ---\n%s\n",
               second + 1);
        return 0;
    }

    fprintf(stderr, "Server did not respond after %d attempts.\n",
            MAX_RETRIES);
    return -1;
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

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, argv[1], &server.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address.\n");
        close(sock);
        return 1;
    }

    char input[256];
    char arg[256];

    for (;;) {
        printf("\n===== LIBRARY MANAGEMENT SYSTEM =====\n");
        printf("1. List available books\n");
        printf("2. Search books\n");
        printf("3. Issue a book\n");
        printf("4. Return a book\n");
        printf("5. Exit\n");
        printf("Enter choice: ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin))
            break;

        char *nl = strchr(input, '\n');
        if (nl) *nl = '\0';

        if (strcmp(input, "1") == 0) {
            send_request(sock, &server, "LIST", "");
        } else if (strcmp(input, "2") == 0) {
            printf("Enter title/author keyword: ");
            fflush(stdout);
            if (!fgets(arg, sizeof(arg), stdin)) break;
            nl = strchr(arg, '\n');
            if (nl) *nl = '\0';

            if (!arg[0] || strchr(arg, '|')) {
                printf("Enter a nonempty keyword without '|'.\n");
                continue;
            }
            send_request(sock, &server, "SEARCH", arg);
        } else if (strcmp(input, "3") == 0 ||
                   strcmp(input, "4") == 0) {
            printf("Enter book ID: ");
            fflush(stdout);
            if (!fgets(arg, sizeof(arg), stdin)) break;
            nl = strchr(arg, '\n');
            if (nl) *nl = '\0';

            char *p = arg;
            while (*p == ' ' || *p == '\t') p++;

            char *tail;
            errno = 0;
            long id = strtol(p, &tail, 10);
            while (*tail == ' ' || *tail == '\t') tail++;

            if (errno || p == tail || *tail ||
                id <= 0 || id > 2147483647L) {
                printf("Invalid book ID.\n");
                continue;
            }

            snprintf(arg, sizeof(arg), "%ld", id);
            send_request(sock, &server,
                         strcmp(input, "3") == 0 ? "ISSUE" : "RETURN",
                         arg);
        } else if (strcmp(input, "5") == 0) {
            printf("Exiting client.\n");
            break;
        } else {
            printf("Invalid choice. Please select 1-5.\n");
        }
    }

    close(sock);
    return 0;
}
