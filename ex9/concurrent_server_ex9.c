   Implementation of Concurrent Server
[24bcs084@mepcolinux ex5]$ cat server.c
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT 8080
#define BACKLOG 4
#define BUF_SIZE 8192
#define MAX_LINE 512
#define MAX_FILE_SIZE (1024ULL * 1024 * 1024)

static int send_all(int fd, const void *data, size_t len) {
    const char *p = data;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_all(int fd, void *data, size_t len) {
    char *p = data;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

/* Read one newline-terminated protocol line. */
static int recv_line(int fd, char *buf, size_t cap) {
    size_t i = 0;
    while (i + 1 < cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        if (c == '\n') {
            buf[i] = '\0';
            return 0;
        }
        if (c == '\r' || c == '\0') return -1;
        buf[i++] = c;
    }
    return -1;
}

/* Permit only simple filenames, never paths. */
static int valid_filename(const char *name) {
    if (!name || !*name || strlen(name) > 200) return 0;
    if (strstr(name, "..")) return 0;

    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        if (!( (*p >= 'a' && *p <= 'z') ||
               (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') ||
               *p == '.' || *p == '_' || *p == '-'))
            return 0;
    }
    return 1;
}

static int parse_size_line(int fd, uint64_t *size) {
    char line[64], *end;
    if (recv_line(fd, line, sizeof(line)) < 0) return -1;

    errno = 0;
    unsigned long long value = strtoull(line, &end, 10);
    if (errno || end == line || *end != '\0' ||
        value > MAX_FILE_SIZE)
        return -1;

    *size = (uint64_t)value;
    return 0;
}

static int receive_file(int fd, const char *path, uint64_t size) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    char buffer[BUF_SIZE];
    uint64_t remaining = size;
    int result = 0;

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer)
                           ? sizeof(buffer) : (size_t)remaining;
        if (recv_all(fd, buffer, chunk) < 0) {
            result = -1;
            break;
        }
        if (fwrite(buffer, 1, chunk, fp) != chunk) {
            result = -1;
            break;
        }
        remaining -= chunk;
    }

    if (fclose(fp) != 0) result = -1;
    if (result < 0) {
        remove(path);
        return -1;
    }
    return 0;
}

static int send_file(int fd, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    if (fseeko(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    off_t end = ftello(fp);
    if (end < 0 || (uint64_t)end > MAX_FILE_SIZE ||
        fseeko(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    uint64_t size = (uint64_t)end;
    char header[64];
    int n = snprintf(header, sizeof(header), "OK %" PRIu64 "\n", size);
    if (n < 0 || (size_t)n >= sizeof(header) ||
        send_all(fd, header, (size_t)n) < 0) {
        fclose(fp);
        return -1;
    }

    char buffer[BUF_SIZE];
    size_t got;
    while ((got = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send_all(fd, buffer, got) < 0) {
            fclose(fp);
            return -1;
        }
    }

    int result = ferror(fp) ? -1 : 0;
    fclose(fp);
    return result;
}

static void handle_client(int fd) {
    char line[MAX_LINE];

    while (1) {
        if (recv_line(fd, line, sizeof(line)) < 0) break;

        char command[16], filename[201], extra;
        if (sscanf(line, "%15s %200s %c",
                   command, filename, &extra) != 2 ||
            !valid_filename(filename)) {
            if (send_all(fd, "ERR Invalid command or filename\n", 32) < 0)
                break;
            continue;
        }

        char path[256];
        if (snprintf(path, sizeof(path), "server/%s", filename)
                >= (int)sizeof(path)) {
            send_all(fd, "ERR Invalid path\n", 17);
            continue;
        }

        if (strcmp(command, "STORE") == 0) {
            if (send_all(fd, "READY\n", 6) < 0) break;

            uint64_t size;
            if (parse_size_line(fd, &size) < 0) break;

            if (receive_file(fd, path, size) < 0) {
                if (send_all(fd, "ERR Transfer failed\n", 20) < 0)
                    break;
            } else {
                if (send_all(fd, "OK Stored\n", 10) < 0) break;
                printf("Stored server/%s (%" PRIu64 " bytes)\n",
                       filename, size);
                fflush(stdout);
            }
        } else if (strcmp(command, "RETRIEVE") == 0) {
            FILE *test = fopen(path, "rb");
            if (!test) {
                if (send_all(fd, "ERR File not found\n", 19) < 0)
                    break;
                continue;
            }
            fclose(test);

            if (send_file(fd, path) < 0) break;
            printf("Sent server/%s\n", filename);
            fflush(stdout);
        } else {
            if (send_all(fd, "ERR Unknown command\n", 20) < 0)
                break;
        }
    }

    close(fd);
}

int main(void) {
    if (mkdir("server", 0755) < 0 && errno != EEXIST) {
        perror("mkdir server");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    /* Reap terminated children so they do not become zombies. */
    signal(SIGCHLD, SIG_IGN);

    printf("TCP file server listening on port %d...\n", PORT);
    fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            close(server_fd);
            handle_client(client_fd);
            _exit(0);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}

[24bcs084@mepcolinux ex5]$ cat client.c
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUF_SIZE 8192
#define MAX_LINE 512
#define MAX_FILE_SIZE (1024ULL * 1024 * 1024)

static int send_all(int fd, const void *data, size_t len) {
    const char *p = data;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_all(int fd, void *data, size_t len) {
    char *p = data;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_line(int fd, char *buf, size_t cap) {
    size_t i = 0;
    while (i + 1 < cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        if (c == '\n') {
            buf[i] = '\0';
            return 0;
        }
        if (c == '\r' || c == '\0') return -1;
        buf[i++] = c;
    }
    return -1;
}

static int valid_filename(const char *name) {
    if (!name || !*name || strlen(name) > 200 ||
        strstr(name, ".."))
        return 0;

    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        if (!( (*p >= 'a' && *p <= 'z') ||
               (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') ||
               *p == '.' || *p == '_' || *p == '-'))
            return 0;
    }
    return 1;
}

static int parse_size_line(int fd, uint64_t *size) {
    char line[64], *end;
    if (recv_line(fd, line, sizeof(line)) < 0) return -1;

    errno = 0;
    unsigned long long value = strtoull(line, &end, 10);
    if (errno || end == line || *end != '\0' ||
        value > MAX_FILE_SIZE)
        return -1;

    *size = (uint64_t)value;
    return 0;
}

static int store_file(int fd, const char *filename) {
    char path[256], response[MAX_LINE];

    snprintf(path, sizeof(path), "client/%s", filename);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("Cannot open client file");
        return -1;
    }

    if (fseeko(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    off_t end = ftello(fp);
    if (end < 0 || (uint64_t)end > MAX_FILE_SIZE ||
        fseeko(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        fprintf(stderr, "File is too large or cannot be read.\n");
        return -1;
    }

    uint64_t size = (uint64_t)end;
    char command[MAX_LINE];
    int n = snprintf(command, sizeof(command), "STORE %s\n", filename);

    if (n < 0 || (size_t)n >= sizeof(command) ||
        send_all(fd, command, (size_t)n) < 0 ||
        recv_line(fd, response, sizeof(response)) < 0) {
        fclose(fp);
        return -1;
    }

    if (strcmp(response, "READY") != 0) {
        printf("Server: %s\n", response);
        fclose(fp);
        return -1;
    }

    char size_line[64];
    n = snprintf(size_line, sizeof(size_line), "%" PRIu64 "\n", size);
    if (n < 0 || (size_t)n >= sizeof(size_line) ||
        send_all(fd, size_line, (size_t)n) < 0) {
        fclose(fp);
        return -1;
    }

    char buffer[BUF_SIZE];
    size_t got;
    int result = 0;

    while ((got = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send_all(fd, buffer, got) < 0) {
            result = -1;
            break;
        }
    }

    if (ferror(fp)) result = -1;
    fclose(fp);

    if (result < 0) return -1;

    if (recv_line(fd, response, sizeof(response)) < 0) return -1;
    printf("Server: %s\n", response);
    return strcmp(response, "OK Stored") == 0 ? 0 : -1;
}

static int retrieve_file(int fd, const char *filename) {
    char command[MAX_LINE], response[MAX_LINE], path[256];

    snprintf(command, sizeof(command), "RETRIEVE %s\n", filename);
    if (send_all(fd, command, strlen(command)) < 0 ||
        recv_line(fd, response, sizeof(response)) < 0)
        return -1;

    if (strncmp(response, "OK ", 3) != 0) {
        printf("Server: %s\n", response);
        return -1;
    }

    char size_text[64];
    snprintf(size_text, sizeof(size_text), "%s", response + 3);

    errno = 0;
    char *end;
    unsigned long long value = strtoull(size_text, &end, 10);
    if (errno || end == size_text || *end != '\0' ||
        value > MAX_FILE_SIZE) {
        fprintf(stderr, "Invalid size received from server.\n");
        return -1;
    }

    uint64_t size = (uint64_t)value;
    snprintf(path, sizeof(path), "client/%s", filename);

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("Cannot create client file");
        return -1;
    }

    char buffer[BUF_SIZE];
    uint64_t remaining = size;
    int result = 0;

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer)
                           ? sizeof(buffer) : (size_t)remaining;

        if (recv_all(fd, buffer, chunk) < 0) {
            result = -1;
            break;
        }

        if (fwrite(buffer, 1, chunk, fp) != chunk) {
            result = -1;
            break;
        }

        remaining -= chunk;
    }

    if (fclose(fp) != 0) result = -1;

    if (result < 0) {
        remove(path);
        fprintf(stderr, "File transfer failed.\n");
        return -1;
    }

    printf("Downloaded %s (%" PRIu64 " bytes) to %s\n",
           filename, size, path);
    return 0;
}

int main(void) {
    if (mkdir("client", 0755) < 0 && errno != EEXIST) {
        perror("mkdir client");
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server.sin_addr) != 1) {
        fprintf(stderr, "Invalid server IP address.\n");
        close(fd);
        return 1;
    }

    if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    printf("Connected to TCP server %s:%d\n", SERVER_IP, PORT);
    printf("Commands: STORE filename | RETRIEVE filename | QUIT\n");

    char line[MAX_LINE];

    while (1) {
        printf("\nfile-client> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;

        line[strcspn(line, "\n")] = '\0';

        char command[16], filename[201], extra;
        int fields = sscanf(line, "%15s %200s %c",
                            command, filename, &extra);

        if (strcmp(line, "QUIT") == 0) break;

        if (fields != 2 || !valid_filename(filename)) {
            printf("Usage: STORE filename | RETRIEVE filename | QUIT\n");
            continue;
        }

        if (strcmp(command, "STORE") == 0) {
            if (store_file(fd, filename) == 0)
                printf("Upload complete.\n");
            else
                printf("Upload failed.\n");
        } else if (strcmp(command, "RETRIEVE") == 0) {
            if (retrieve_file(fd, filename) == 0)
                printf("Download complete.\n");
            else
                printf("Download failed.\n");
        } else {
            printf("Unknown command. Use STORE, RETRIEVE, or QUIT.\n");
        }
    }

    close(fd);
    printf("Client disconnected.\n");
    return 0;
}
