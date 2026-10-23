k_v_nirupa@LenovoLap:~/sem5/cn/ex6$ cat mlt_server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/utsname.h>

int main()
{
    int s, c, n, choice;
    char buffer[2048], user[100], pass[100];
    char filename[200], command[20];
    struct sockaddr_in server, client;
    struct utsname info;
    socklen_t len = sizeof(client);
    FILE *fp;
    time_t now;

    s = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(8082);
    server.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr *)&server, sizeof(server));
    listen(s, 5);

    printf("Multi-service server waiting...\n");

    c = accept(s, (struct sockaddr *)&client, &len);

    send(c, "Username:", 10, 0);
    n = recv(c, user, sizeof(user) - 1, 0);
    if (n <= 0) goto end;
    user[n] = '\0';
    user[strcspn(user, "\r\n")] = '\0';

    send(c, "Password:", 9, 0);
    n = recv(c, pass, sizeof(pass) - 1, 0);
    if (n <= 0) goto end;
    pass[n] = '\0';
    pass[strcspn(pass, "\r\n")] = '\0';

    if (strcmp(user, "admin") != 0 ||
        strcmp(pass, "1234") != 0)
    {
        send(c, "Login failed", 13, 0);
        goto end;
    }

    send(c, "Login successful", 17, 0);

    while (1)
    {
        n = recv(c, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';

        if (strcmp(buffer, "TIME") == 0)
        {
            now = time(NULL);
            snprintf(buffer, sizeof(buffer), "%s", ctime(&now));
            send(c, buffer, strlen(buffer) + 1, 0);
        }
        else if (strcmp(buffer, "INFO") == 0)
        {
            uname(&info);
            snprintf(buffer, sizeof(buffer),
                     "System: %s\nNode: %s\nRelease: %s\nMachine: %s",
                     info.sysname, info.nodename,
                     info.release, info.machine);
            send(c, buffer, strlen(buffer) + 1, 0);
        }
        else if (strcmp(buffer, "CLIENT") == 0)
        {
            snprintf(buffer, sizeof(buffer),
                     "Client IP: %s\nClient port: %d",
                     inet_ntoa(client.sin_addr),
                     ntohs(client.sin_port));
            send(c, buffer, strlen(buffer) + 1, 0);
        }
        else if (strncmp(buffer, "UPLOAD ", 7) == 0)
        {
            strcpy(filename, buffer + 7);
            fp = fopen(filename, "w");

            if (fp == NULL)
            {
                send(c, "Cannot create file", 19, 0);
                continue;
            }

            send(c, "Send file content:", 19, 0);
            n = recv(c, buffer, sizeof(buffer) - 1, 0);

            if (n > 0)
            {
                fwrite(buffer, 1, n, fp);
                fclose(fp);
                send(c, "Upload successful", 18, 0);
            }
            else
            {
                fclose(fp);
                send(c, "Upload failed", 14, 0);
            }
        }
        else if (strncmp(buffer, "DOWNLOAD ", 9) == 0)
        {
            strcpy(filename, buffer + 9);
            fp = fopen(filename, "rb");

            if (fp == NULL)
            {
                send(c, "File not found", 15, 0);
                continue;
            }

            n = fread(buffer, 1, sizeof(buffer) - 1, fp);
            fclose(fp);
            buffer[n] = '\0';
            send(c, buffer, n + 1, 0);
        }
        else if (strcmp(buffer, "QUIT") == 0)
        {
            send(c, "Goodbye", 8, 0);
            break;
        }
        else
        {
            send(c, "Invalid request", 16, 0);
        }
    }

end:
    close(c);
    close(s);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex6$ cat mlt_client.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main()
{
    int s, n, choice;
    char buffer[2048], filename[200];
    struct sockaddr_in server;
    FILE *fp;

    s = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(8082);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Connection failed");
        return 1;
    }

    n = recv(s, buffer, sizeof(buffer) - 1, 0);
    buffer[n] = '\0';
    printf("%s ", buffer);
    fgets(buffer, sizeof(buffer), stdin);
    send(s, buffer, strlen(buffer), 0);

    n = recv(s, buffer, sizeof(buffer) - 1, 0);
    buffer[n] = '\0';
    printf("%s ", buffer);
    fgets(buffer, sizeof(buffer), stdin);
    send(s, buffer, strlen(buffer), 0);

    n = recv(s, buffer, sizeof(buffer) - 1, 0);
    buffer[n] = '\0';
    printf("%s\n", buffer);

    if (strncmp(buffer, "Login successful", 16) != 0)
    {
        close(s);
        return 0;
    }

    while (1)
    {
        printf("\n1.Date/Time\n2.System Info\n3.Client Info\n");
        printf("4.Upload file\n5.Download file\n6.Exit\nChoice: ");
        scanf("%d", &choice);
        getchar();

        if (choice == 1)
            strcpy(buffer, "TIME");
        else if (choice == 2)
            strcpy(buffer, "INFO");
        else if (choice == 3)
            strcpy(buffer, "CLIENT");
        else if (choice == 4)
        {
            printf("Enter local filename: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\r\n")] = '\0';

            fp = fopen(filename, "rb");
            if (fp == NULL)
            {
                printf("Cannot open file\n");
                continue;
            }

            snprintf(buffer, sizeof(buffer), "UPLOAD %s", filename);
            send(s, buffer, strlen(buffer) + 1, 0);

            n = recv(s, buffer, sizeof(buffer) - 1, 0);
            buffer[n] = '\0';
            printf("%s\n", buffer);

            n = fread(buffer, 1, sizeof(buffer) - 1, fp);
            fclose(fp);
            send(s, buffer, n, 0);

            n = recv(s, buffer, sizeof(buffer) - 1, 0);
            buffer[n] = '\0';
            printf("%s\n", buffer);
            continue;
        }
        else if (choice == 5)
        {
            printf("Enter server filename: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\r\n")] = '\0';

            snprintf(buffer, sizeof(buffer), "DOWNLOAD %s", filename);
            send(s, buffer, strlen(buffer) + 1, 0);

            n = recv(s, buffer, sizeof(buffer) - 1, 0);
            if (n > 0)
            {
                buffer[n] = '\0';

                if (strcmp(buffer, "File not found") == 0)
                    printf("%s\n", buffer);
                else
                {
                    fp = fopen("downloaded_file.txt", "wb");
                    if (fp != NULL)
                    {
                        fwrite(buffer, 1, strlen(buffer), fp);
                        fclose(fp);
                        printf("Saved as downloaded_file.txt\n");
                    }
                }
            }
            continue;
        }
        else if (choice == 6)
        {
            strcpy(buffer, "QUIT");
            send(s, buffer, strlen(buffer) + 1, 0);
            n = recv(s, buffer, sizeof(buffer) - 1, 0);
            if (n > 0)
            {
                buffer[n] = '\0';
                printf("%s\n", buffer);
            }
            break;
        }
        else
        {
            printf("Invalid choice\n");
            continue;
        }

        send(s, buffer, strlen(buffer) + 1, 0);
        n = recv(s, buffer, sizeof(buffer) - 1, 0);

        if (n <= 0) break;
        buffer[n] = '\0';
        printf("%s\n", buffer);
    }

    close(s);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex6$
