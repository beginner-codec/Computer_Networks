k_v_nirupa@LenovoLap:~/sem5/cn/ex9$ cat self3server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <stdint.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <limits.h>

#define PORT 8081
#define MAX_CLIENTS 50
#define BUFFER_SIZE 4096
#define MAX_FILES 1000
#define MAX_NAME 200
#define FILE_DIR "/home/k_v_nirupa/sem5/cn/ex9/shared"

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

int send_all(int sock, const void *data, size_t len)
{
    const char *p = (const char *)data;

    while (len > 0)
    {
        ssize_t n = send(sock, p, len, 0);
        if (n <= 0)
            return -1;
        p += n;
        len -= (size_t)n;
    }

    return 0;
}

int recv_all(int sock, void *data, size_t len)
{
    char *p = (char *)data;

    while (len > 0)
    {
        ssize_t n = recv(sock, p, len, 0);
        if (n <= 0)
            return -1;
        p += n;
        len -= (size_t)n;
    }

    return 0;
}

int send_u32(int sock, uint32_t value)
{
    uint32_t net = htonl(value);
    return send_all(sock, &net, sizeof(net));
}

int recv_u32(int sock, uint32_t *value)
{
    uint32_t net;

    if (recv_all(sock, &net, sizeof(net)) < 0)
        return -1;

    *value = ntohl(net);
    return 0;
}

int send_u64(int sock, uint64_t value)
{
    unsigned char b[8];
    int i;

    for (i = 7; i >= 0; i--)
    {
        b[i] = (unsigned char)(value & 255);
        value >>= 8;
    }

    return send_all(sock, b, 8);
}

int valid_filename(const char *name)
{
    size_t i, len = strlen(name);

    if (len == 0 || len > MAX_NAME)
        return 0;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return 0;

    for (i = 0; i < len; i++)
    {
        if (!isalnum((unsigned char)name[i]) &&
            name[i] != '.' && name[i] != '_' &&
            name[i] != '-')
            return 0;
    }

    return 1;
}

int send_file_list(int sock)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char path[PATH_MAX];
    char names[MAX_FILES][MAX_NAME + 1];
    uint32_t count = 0;
    uint32_t i;
    size_t len;

    pthread_mutex_lock(&file_mutex);

    dir = opendir(FILE_DIR);

    if (dir == NULL)
    {
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL &&
           count < MAX_FILES)
    {
        if (!valid_filename(entry->d_name))
            continue;

        if (snprintf(path, sizeof(path), "%s/%s",
                     FILE_DIR, entry->d_name) >= (int)sizeof(path))
            continue;

        if (lstat(path, &st) < 0 || !S_ISREG(st.st_mode))
            continue;

        len = strlen(entry->d_name);

        if (len > MAX_NAME)
            continue;

        strcpy(names[count], entry->d_name);
        count++;
    }

    closedir(dir);
    pthread_mutex_unlock(&file_mutex);

    if (send_u32(sock, count) < 0)
        return -1;

    for (i = 0; i < count; i++)
    {
        len = strlen(names[i]);

        if (send_u32(sock, (uint32_t)len) < 0)
            return -1;

        if (send_all(sock, names[i], len) < 0)
            return -1;
    }

    return 0;
}

int send_file(int sock, const char *filename)
{
    char path[PATH_MAX];
    char buffer[BUFFER_SIZE];
    struct stat st;
    FILE *fp;
    uint64_t remaining;
    size_t n;

    if (!valid_filename(filename))
        return send_u64(sock, UINT64_MAX);

    if (snprintf(path, sizeof(path), "%s/%s",
                 FILE_DIR, filename) >= (int)sizeof(path))
        return send_u64(sock, UINT64_MAX);

    pthread_mutex_lock(&file_mutex);

    if (lstat(path, &st) < 0 || !S_ISREG(st.st_mode))
    {
        pthread_mutex_unlock(&file_mutex);
        return send_u64(sock, UINT64_MAX);
    }

    fp = fopen(path, "rb");

    if (fp == NULL)
    {
        pthread_mutex_unlock(&file_mutex);
        return send_u64(sock, UINT64_MAX);
    }

    if (fstat(fileno(fp), &st) < 0 || !S_ISREG(st.st_mode))
    {
        fclose(fp);
        pthread_mutex_unlock(&file_mutex);
        return send_u64(sock, UINT64_MAX);
    }

    remaining = (uint64_t)st.st_size;

    if (send_u64(sock, remaining) < 0)
    {
        fclose(fp);
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }

    pthread_mutex_unlock(&file_mutex);

    while (remaining > 0)
    {
        size_t wanted = remaining < BUFFER_SIZE
                        ? (size_t)remaining : BUFFER_SIZE;

        n = fread(buffer, 1, wanted, fp);

        if (n == 0)
        {
            fclose(fp);
            return -1;
        }

        if (send_all(sock, buffer, n) < 0)
        {
            fclose(fp);
            return -1;
        }

        remaining -= n;
    }

    fclose(fp);
    printf("Sent file: %s\n", filename);

    return 0;
}

void *handle_client(void *arg)
{
    int sock = *(int *)arg;
    char command[16];
    char filename[MAX_NAME + 1];
    uint32_t name_len;

    free(arg);

    while (1)
    {
        memset(command, 0, sizeof(command));

        if (recv_all(sock, command, sizeof(command)) < 0)
            break;

        command[sizeof(command) - 1] = '\0';

        if (strcmp(command, "LIST") == 0)
        {
            if (send_file_list(sock) < 0)
                break;
        }
        else if (strcmp(command, "GET") == 0)
        {
            if (recv_u32(sock, &name_len) < 0)
                break;

            if (name_len == 0 || name_len > MAX_NAME)
            {
                if (send_u64(sock, UINT64_MAX) < 0)
                    break;
                continue;
            }

            if (recv_all(sock, filename, name_len) < 0)
                break;

            filename[name_len] = '\0';

            if (send_file(sock, filename) < 0)
                break;
        }
        else if (strcmp(command, "QUIT") == 0)
        {
            break;
        }
        else
        {
            if (send_u64(sock, UINT64_MAX) < 0)
                break;
        }
    }

    close(sock);
    printf("Client disconnected.\n");

    return NULL;
}

int main(void)
{
    int server_fd, client_fd;
    int opt = 1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    pthread_t thread_id;
    struct stat st;

    signal(SIGPIPE, SIG_IGN);

    if (stat(FILE_DIR, &st) < 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Shared directory does not exist: %s\n",
                FILE_DIR);
        return 1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("File Sharing Server running on port %d\n", PORT);
    printf("Shared directory: %s\n", FILE_DIR);

    while (1)
    {
        int *client_sock;

        addr_len = sizeof(client_addr);

        client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &addr_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        client_sock = malloc(sizeof(int));

        if (client_sock == NULL)
        {
            close(client_fd);
            continue;
        }

        *client_sock = client_fd;

        if (pthread_create(&thread_id, NULL,
                           handle_client, client_sock) != 0)
        {
            perror("pthread_create");
            close(client_fd);
            free(client_sock);
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex9$ cat self3client.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8081
#define BUFFER_SIZE 4096
#define MAX_NAME 200

int send_all(int sock, const void *data, size_t len)
{
    const char *p = (const char *)data;

    while (len > 0)
    {
        ssize_t n = send(sock, p, len, 0);

        if (n <= 0)
            return -1;

        p += n;
        len -= (size_t)n;
    }

    return 0;
}

int recv_all(int sock, void *data, size_t len)
{
    char *p = (char *)data;

    while (len > 0)
    {
        ssize_t n = recv(sock, p, len, 0);

        if (n <= 0)
            return -1;

        p += n;
        len -= (size_t)n;
    }

    return 0;
}

int send_u32(int sock, uint32_t value)
{
    uint32_t net = htonl(value);
    return send_all(sock, &net, sizeof(net));
}

int recv_u32(int sock, uint32_t *value)
{
    uint32_t net;

    if (recv_all(sock, &net, sizeof(net)) < 0)
        return -1;

    *value = ntohl(net);
    return 0;
}

int recv_u64(int sock, uint64_t *value)
{
    unsigned char b[8];
    int i;

    if (recv_all(sock, b, 8) < 0)
        return -1;

    *value = 0;

    for (i = 0; i < 8; i++)
        *value = (*value << 8) | b[i];

    return 0;
}

int main(void)
{
    int sock;
    struct sockaddr_in server_addr;
    char command[32];
    char filename[MAX_NAME + 1];
    char buffer[BUFFER_SIZE];
    uint64_t file_size, remaining;
    uint32_t count, name_len;
    FILE *fp;
    size_t n;
    uint32_t i;

    signal(SIGPIPE, SIG_IGN);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock);
        return 1;
    }

    while (1)
    {
        printf("\n1. List files\n2. Download file\n3. Exit\n");
        printf("Enter choice: ");

        if (fgets(command, sizeof(command), stdin) == NULL)
            break;

        if (command[0] == '1')
        {
            memset(command, 0, sizeof(command));
            memcpy(command, "LIST", 4);

            if (send_all(sock, command, 16) < 0)
                break;

            if (recv_u32(sock, &count) < 0)
                break;

            if (count > 1000)
            {
                printf("Invalid response from server.\n");
                break;
            }

            printf("\nAvailable files:\n");

            for (i = 0; i < count; i++)
            {
                if (recv_u32(sock, &name_len) < 0)
                    goto disconnect;

                if (name_len == 0 || name_len > MAX_NAME)
                    goto disconnect;

                if (recv_all(sock, filename, name_len) < 0)
                    goto disconnect;

                filename[name_len] = '\0';

                printf("%u. %s\n", i + 1, filename);
            }

            if (count == 0)
                printf("No files available.\n");
        }
        else if (command[0] == '2')
        {
            printf("Enter filename to download: ");

            if (fgets(filename, sizeof(filename), stdin) == NULL)
                break;

            filename[strcspn(filename, "\n")] = '\0';
            name_len = (uint32_t)strlen(filename);

            if (name_len == 0 || name_len > MAX_NAME)
            {
                printf("Invalid filename.\n");
                continue;
            }

            memset(command, 0, sizeof(command));
            memcpy(command, "GET", 3);

            if (send_all(sock, command, 16) < 0 ||
                send_u32(sock, name_len) < 0 ||
                send_all(sock, filename, name_len) < 0)
                break;

            if (recv_u64(sock, &file_size) < 0)
                break;

            if (file_size == UINT64_MAX)
            {
                printf("File not found or invalid filename.\n");
                continue;
            }

            fp = fopen(filename, "wb");

            if (fp == NULL)
            {
                perror("Cannot create output file");
                break;
            }

            remaining = file_size;

            while (remaining > 0)
            {
                size_t wanted = remaining < BUFFER_SIZE
                                ? (size_t)remaining
                                : BUFFER_SIZE;

                if (recv_all(sock, buffer, wanted) < 0)
                {
                    printf("Transfer interrupted.\n");
                    fclose(fp);
                    remove(filename);
                    goto disconnect;
                }

                n = fwrite(buffer, 1, wanted, fp);

                if (n != wanted)
                {
                    perror("File write error");
                    fclose(fp);
                    remove(filename);
                    goto disconnect;
                }

                remaining -= wanted;
            }

            if (fclose(fp) != 0)
            {
                perror("File close error");
                remove(filename);
                continue;
            }

            printf("File '%s' downloaded successfully (%llu bytes).\n",
                   filename, (unsigned long long)file_size);
        }
        else if (command[0] == '3')
        {
            memset(command, 0, sizeof(command));
            memcpy(command, "QUIT", 4);
            send_all(sock, command, 16);
            break;
        }
        else
        {
            printf("Invalid choice.\n");
        }
    }

disconnect:
    close(sock);
    printf("Disconnected from server.\n");

    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex9$
