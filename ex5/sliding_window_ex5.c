      Implementation of Sliding Window Protocol

[24bcs084@mepcolinux ex5]$ cat arq.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>

#define PORT 8134
#define IP "127.0.0.1"
#define MAX_FRAMES 50
#define GBN 1
#define SR 2

#define TYPE_ACK 1
#define TYPE_NACK 0

struct config {
    int window_size;
    int arq_type;
    int total_frames;
    int timeout_sec;
};

struct frame {
    int seq_no;
    char data[20];
};

struct ack {
    int type;
    int ack_no;
};

/* Send an entire structure over TCP. */
static inline int send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0)
            return -1;
        sent += (size_t)n;
    }
    return 0;
}

/* Receive an entire structure over TCP. */
static inline int recv_all(int fd, void *buf, size_t len) {
    char *p = buf;
    size_t received = 0;

    while (received < len) {
        ssize_t n = recv(fd, p + received, len - received, 0);

        if (n == 0)
            return 0;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        received += (size_t)n;
    }
    return 1;
}
[24bcs084@mepcolinux ex5]$ cat sender.c

#include "arq.h"

static void print_window(int base, int window_size,
                         int total_frames, int *received_ack,
                         int is_sr) {
    int last = base + window_size - 1;

    if (last >= total_frames)
        last = total_frames - 1;

    printf("\n[Sender Window]: |");
    for (int i = base; i <= last; i++) {
        if (is_sr && received_ack[i])
            printf(" %d(ACK) |", i);
        else
            printf(" %d |", i);
    }
    printf("\n");
}

static int valid_ack(struct ack *ack, int total_frames) {
    return (ack->type == TYPE_ACK || ack->type == TYPE_NACK) &&
           ack->ack_no >= 0 &&
           ack->ack_no < total_frames;
}

int main(void) {
    int sockfd;
    struct sockaddr_in server;
    struct config config;
    struct frame frames[MAX_FRAMES];
    struct ack ack;
    int received_ack[MAX_FRAMES] = {0};
    int base = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(IP);

    if (connect(sockfd, (struct sockaddr *)&server,
                sizeof(server)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Enter Timeout (seconds): ");
    if (scanf("%d", &config.timeout_sec) != 1 ||
        config.timeout_sec <= 0) {
        fprintf(stderr, "Timeout must be positive.\n");
        close(sockfd);
        return 1;
    }

    printf("Enter window size (N): ");
    if (scanf("%d", &config.window_size) != 1 ||
        config.window_size < 1 ||
        config.window_size > MAX_FRAMES) {
        fprintf(stderr, "Invalid window size.\n");
        close(sockfd);
        return 1;
    }

    printf("1. Go-Back-N\n2. Selective Repeat\nChoice: ");
    if (scanf("%d", &config.arq_type) != 1 ||
        (config.arq_type != GBN && config.arq_type != SR)) {
        fprintf(stderr, "Invalid ARQ type.\n");
        close(sockfd);
        return 1;
    }

    printf("Enter total frames: ");
    if (scanf("%d", &config.total_frames) != 1 ||
        config.total_frames < 1 ||
        config.total_frames > MAX_FRAMES) {
        fprintf(stderr, "Total frames must be between 1 and %d.\n",
                MAX_FRAMES);
        close(sockfd);
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = config.timeout_sec;
    tv.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                   &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
        close(sockfd);
        return 1;
    }

    if (send_all(sockfd, &config, sizeof(config)) < 0) {
        perror("send config");
        close(sockfd);
        return 1;
    }

    for (int i = 0; i < config.total_frames; i++) {
        frames[i].seq_no = i;
        snprintf(frames[i].data, sizeof(frames[i].data),
                 "Data-%d", i);
    }

    if (config.arq_type == GBN) {
        printf("\n--- GO-BACK-N: CUMULATIVE ACK ---\n");

        while (base < config.total_frames) {
            int last = base + config.window_size - 1;
            if (last >= config.total_frames)
                last = config.total_frames - 1;

            print_window(base, config.window_size,
                         config.total_frames, NULL, 0);

            /*
             * Send the current outstanding window.
             * After an error/timeout, this loop starts at
             * the current base and retransmits from there.
             */
            for (int i = base; i <= last; i++) {
                printf(" -> Sending frame %d\n", i);

                if (send_all(sockfd, &frames[i],
                             sizeof(frames[i])) < 0) {
                    perror("send frame");
                    close(sockfd);
                    return 1;
                }
            }

            while (base <= last) {
                int result = recv_all(sockfd, &ack, sizeof(ack));

                if (result == 0) {
                    fprintf(stderr, "Receiver disconnected.\n");
                    close(sockfd);
                    return 1;
                }

                if (result < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf(" [!] Timeout. Retransmitting "
                               "from frame %d\n", base);
                        break;
                    }

                    perror("receive ACK");
                    close(sockfd);
                    return 1;
                }

                if (!valid_ack(&ack, config.total_frames)) {
                    printf(" [!] Invalid ACK ignored.\n");
                    continue;
                }

                if (ack.type == TYPE_ACK) {
                    printf(" <- Cumulative ACK %d received\n",
                           ack.ack_no);

                    if (ack.ack_no >= base) {
                        base = ack.ack_no + 1;
                    } else {
                        printf("    Stale cumulative ACK ignored.\n");
                    }

                } else {
                    printf(" <- NACK %d received; "
                           "retransmit from frame %d\n",
                           ack.ack_no, ack.ack_no);

                    if (ack.ack_no >= base && ack.ack_no <= last) {
                        base = ack.ack_no;
                        break;
                    }
                }
            }
        }

    } else {
        printf("\n--- SELECTIVE REPEAT: INDIVIDUAL ACK ---\n");

        while (base < config.total_frames) {
            int last = base + config.window_size - 1;
            if (last >= config.total_frames)
                last = config.total_frames - 1;

            print_window(base, config.window_size,
                         config.total_frames, received_ack, 1);

            /* Send only frames not individually ACKed. */
            for (int i = base; i <= last; i++) {
                if (!received_ack[i]) {
                    printf(" -> Sending frame %d\n", i);

                    if (send_all(sockfd, &frames[i],
                                 sizeof(frames[i])) < 0) {
                        perror("send frame");
                        close(sockfd);
                        return 1;
                    }
                }
            }

            int timed_out = 0;

            while (1) {
                int all_acked = 1;

                for (int i = base; i <= last; i++) {
                    if (!received_ack[i]) {
                        all_acked = 0;
                        break;
                    }
                }

                if (all_acked)
                    break;

                int result = recv_all(sockfd, &ack, sizeof(ack));

                if (result == 0) {
                    fprintf(stderr, "Receiver disconnected.\n");
                    close(sockfd);
                    return 1;
                }

                if (result < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf(" [!] Timeout. Retransmitting "
                               "unacknowledged frames only.\n");
                        timed_out = 1;
                        break;
                    }

                    perror("receive ACK");
                    close(sockfd);
                    return 1;
                }

                if (!valid_ack(&ack, config.total_frames)) {
                    printf(" [!] Invalid ACK ignored.\n");
                    continue;
                }

                if (ack.ack_no < base || ack.ack_no > last) {
                    printf("    ACK %d is outside current window; "
                           "ignored.\n", ack.ack_no);
                    continue;
                }

                if (ack.type == TYPE_ACK) {
                    received_ack[ack.ack_no] = 1;

                    printf(" <- Individual ACK %d received\n",
                           ack.ack_no);

                } else {
                    printf(" <- NACK %d received; "
                           "retransmitting only frame %d\n",
                           ack.ack_no, ack.ack_no);

                    if (send_all(sockfd, &frames[ack.ack_no],
                                 sizeof(frames[ack.ack_no])) < 0) {
                        perror("retransmit frame");
                        close(sockfd);
                        return 1;
                    }
                }
            }

            /*
             * On timeout, the outer loop resends only frames
             * in this window that have not been ACKed.
             */
            (void)timed_out;

            while (base < config.total_frames &&
                   received_ack[base]) {
                base++;
            }
        }
    }

    printf("\nAll frames acknowledged successfully.\n");
    close(sockfd);
    return 0;
}

[24bcs084@mepcolinux ex5]$ cat receiver.c

#include "arq.h"

int main(void) {
    int sockfd, newsockfd;
    struct sockaddr_in server, client;
    struct config config;
    struct frame frame;
    struct ack ack;
    int expected = 0;
    int received[MAX_FRAMES] = {0};
    int choice;
    socklen_t client_len = sizeof(client);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server,
             sizeof(server)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, 5) < 0) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    printf("TCP server listening...\n");

    newsockfd = accept(sockfd, (struct sockaddr *)&client,
                       &client_len);
    if (newsockfd < 0) {
        perror("accept");
        close(sockfd);
        return 1;
    }

    if (recv_all(newsockfd, &config, sizeof(config)) != 1) {
        fprintf(stderr, "Failed to receive configuration\n");
        close(newsockfd);
        close(sockfd);
        return 1;
    }

    if (config.total_frames < 1 ||
        config.total_frames > MAX_FRAMES ||
        config.window_size < 1 ||
        config.window_size > MAX_FRAMES ||
        (config.arq_type != GBN && config.arq_type != SR)) {
        fprintf(stderr, "Invalid configuration received\n");
        close(newsockfd);
        close(sockfd);
        return 1;
    }

    printf("Config: %s | Window Size: %d | Frames: %d\n",
           config.arq_type == GBN ? "GBN" : "SR",
           config.window_size, config.total_frames);

    while (expected < config.total_frames) {
        int result = recv_all(newsockfd, &frame, sizeof(frame));

        if (result != 1)
            break;

        if (frame.seq_no < 0 ||
            frame.seq_no >= config.total_frames) {
            fprintf(stderr, "Invalid frame sequence number\n");
            continue;
        }

        printf("\n[Received Frame %d] Expected: %d\n",
               frame.seq_no, expected);

        printf("Action (0: Accept, 1: Simulate loss, "
               "2: Corrupt/NACK): ");
        fflush(stdout);

        if (scanf("%d", &choice) != 1) {
            fprintf(stderr, "Invalid input\n");
            break;
        }

        /* Simulated loss: no ACK or NACK. */
        if (choice == 1) {
            printf(" -> Simulated loss. No ACK/NACK sent.\n");
            continue;
        }

        if (choice != 0 && choice != 2) {
            printf(" -> Invalid choice; treating as corruption.\n");
            choice = 2;
        }

        if (config.arq_type == GBN) {
            /*
             * GBN: cumulative ACK.
             * ACK k means every frame through k was received.
             */
            if (choice == 2) {
                ack.type = TYPE_NACK;
                ack.ack_no = expected;

                printf(" -> Corrupted. Sending NACK %d\n",
                       ack.ack_no);
            } else if (frame.seq_no == expected) {
                received[frame.seq_no] = 1;

                ack.type = TYPE_ACK;
                ack.ack_no = expected;
                expected++;
                printf(" -> Accepted. Sending cumulative ACK %d\n",
                   ack.ack_no);
        } else {
            /*
             * GBN discards out-of-order frames and repeats
             * the last cumulative ACK.
             */
            ack.type = TYPE_ACK;
            ack.ack_no = expected - 1;

            printf(" -> Out of order; sending cumulative ACK %d\n",
                   ack.ack_no);
        }

        if (send_all(newsockfd, &ack, sizeof(ack)) < 0)
            break;

    } else {
        /*
         * SR: individual ACK/NACK.
         * ACK k acknowledges only frame k.
         */
        if (choice == 2) {
            ack.type = TYPE_NACK;
            ack.ack_no = frame.seq_no;

            printf(" -> Corrupted. Sending individual NACK %d\n",
                   ack.ack_no);

            if (send_all(newsockfd, &ack, sizeof(ack)) < 0)
                break;

            continue;
        }

        if (frame.seq_no < expected && received[frame.seq_no]) {
            /* Duplicate: resend its individual ACK. */
            ack.type = TYPE_ACK;
            ack.ack_no = frame.seq_no;

            printf(" -> Duplicate frame. Resending ACK %d\n",
                   ack.ack_no);

        } else if (frame.seq_no >= expected &&
                   frame.seq_no < expected + config.window_size) {
            received[frame.seq_no] = 1;

            ack.type = TYPE_ACK;
            ack.ack_no = frame.seq_no;

            printf(" -> Accepted/buffered. Sending individual ACK %d\n",
                   ack.ack_no);

        } else {
            printf(" -> Outside SR receiver window; discarded.\n");
            continue;
        }

        if (send_all(newsockfd, &ack, sizeof(ack)) < 0)
            break;

        while (expected < config.total_frames &&
               received[expected]) {
            expected++;
        }
    }
}

if (expected == config.total_frames)
    printf("\nAll frames received successfully\n");
else
    printf("\nReceiver stopped before all frames arrived\n");

close(newsockfd);
close(sockfd);
return 0;
}
