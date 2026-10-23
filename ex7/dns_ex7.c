      Implementation of Domain Name System
[24bcs084@mepcolinux ex7]$ cat echo.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define IP "127.0.0.1"
#define MAX_DATA 1024
[24bcs084@mepcolinux ex7]$ cat server.c
#include "echo.h"
#include <ctype.h>

int get_domain_file(const char *hostname, char *filename)
{
    const char *domains[] = {
        "google.com", "mepcoeng.ac.in", "yahoo.com", "github.com", "wikipedia.org"
    };

    const char *files[] = {
        "google.txt", "mepcoeng.txt", "yahoo.txt", "github.txt", "wikipedia.txt"
    };

    int i;
    size_t hlen, dlen;

    hlen = strlen(hostname);

    for (i = 0; i < 5; i++) {
        dlen = strlen(domains[i]);

        if (hlen >= dlen &&
            strcasecmp(hostname + hlen - dlen, domains[i]) == 0 &&
            (hlen == dlen || hostname[hlen - dlen - 1] == '.')) {
            strcpy(filename, files[i]);
            return 1;
        }
    }

    return 0;
}

int main(void)
{
    int sockfd, n, i;
    struct sockaddr_in server_addr, client_addr;
    socklen_t len;
    char url[MAX_DATA];
    char hostname[MAX_DATA];
    char filename[30];
    char ipval[MAX_DATA];
    char *start, *end, *colon;
    size_t hlen;
    FILE *fp;
    char line[1200];
    char stored_url[MAX_DATA];
    char stored_ip[100];

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(IP);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    printf("Server waiting for URL...\n");

    while (1) {
        memset(url, 0, sizeof(url));
        memset(hostname, 0, sizeof(hostname));
        filename[0] = '\0';

        // Default to HOST NOT FOUND
        strcpy(ipval, "HOST NOT FOUND");

        len = sizeof(client_addr);
        n = recvfrom(sockfd, url, sizeof(url) - 1, 0, (struct sockaddr *)&client_addr, &len);

        if (n < 0) {
            perror("Receive failed");
            continue;
        }

        url[n] = '\0';
        url[strcspn(url, "\r\n")] = '\0';

        printf("\nReceived URL: %s\n", url);

        /* Extract hostname from URL. */
        start = strstr(url, "://");
        if (start != NULL) start += 3;
        else start = url;

        end = strpbrk(start, "/?#");
        if (end != NULL) hlen = (size_t)(end - start);
        else hlen = strlen(start);

        /* Remove optional port number. */
        colon = (char *)memchr(start, ':', hlen);
        if (colon != NULL) hlen = (size_t)(colon - start);

        if (hlen > 0 && hlen < sizeof(hostname)) {
            memcpy(hostname, start, hlen);
            hostname[hlen] = '\0';

            for (i = 0; hostname[i] != '\0'; i++) {
                hostname[i] = (char)tolower((unsigned char)hostname[i]);
            }
        }

        /* Select the appropriate file. */
        if (hostname[0] != '\0' && get_domain_file(hostname, filename)) {

            // Domain matched! So if we fail to find the URL in the file, it's an IP NOT FOUND error
            strcpy(ipval, "IP NOT FOUND");

            fp = fopen(filename, "r");
            if (fp != NULL) {
                while (fgets(line, sizeof(line), fp) != NULL) {
                    if (sscanf(line, "%1023s %99s", stored_url, stored_ip) != 2)
                        continue;

                    /* Match the complete URL. */
                    if (strcmp(url, stored_url) == 0) {
                        strncpy(ipval, stored_ip, sizeof(ipval) - 1);
                        ipval[sizeof(ipval) - 1] = '\0';
                        break;
                    }
                }
                fclose(fp);
            } else {
                perror("File opening failed");
            }
        }

        printf("Sending IP: %s\n", ipval);

        if (sendto(sockfd, ipval, strlen(ipval) + 1, 0, (struct sockaddr *)&client_addr, len) < 0) {
            perror("Send failed");
        }
    }

    close(sockfd);
    return 0;
}

[24bcs084@mepcolinux ex7]$ cat client.c
#include "echo.h"

int main(void)
{
    int sockfd, bytes_received, ch;
    struct sockaddr_in server_addr;
    socklen_t len;
    char url[MAX_DATA];
    char ipval[MAX_DATA];

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(IP);

    printf("Enter URL: ");

    if (fgets(url, sizeof(url), stdin) == NULL) {
        printf("Input failed!\n");
        close(sockfd);
        return 1;
    }

    if (strchr(url, '\n') == NULL && !feof(stdin)) {
        printf("URL is too long!\n");
        while ((ch = getchar()) != '\n' && ch != EOF);
        close(sockfd);
        return 1;
    }

    url[strcspn(url, "\r\n")] = '\0';

    if (url[0] == '\0') {
        printf("Invalid URL!\n");
        close(sockfd);
        return 1;
    }

    if (sendto(sockfd, url, strlen(url) + 1, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Send failed");
        close(sockfd);
        return 1;
    }

    len = sizeof(server_addr);

    bytes_received = recvfrom(sockfd, ipval, sizeof(ipval) - 1, 0, (struct sockaddr *)&server_addr, &len);

    if (bytes_received < 0) {
        perror("Receive failed");
        close(sockfd);
        return 1;
    }

    ipval[bytes_received] = '\0';

    // Print conditionally based on whether an error was returned
    if (strstr(ipval, "NOT FOUND") != NULL) {
        printf("Error: %s\n", ipval);
    } else {
        printf("IP Address: %s\n", ipval);
    }

    close(sockfd);
    return 0;
}
