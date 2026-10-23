k_v_nirupa@LenovoLap:~/sem5/cn/ex6$ cat echo_server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main()
{
    int s, c, n;
    char buffer[1024];
    struct sockaddr_in server, client;
    socklen_t len = sizeof(client);

    s = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr *)&server, sizeof(server));
    listen(s, 5);

    printf("Echo server waiting...\n");

    c = accept(s, (struct sockaddr *)&client, &len);

    while ((n = recv(c, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[n] = '\0';
        printf("Client: %s\n", buffer);
        send(c, buffer, n, 0);
    }

    close(c);
    close(s);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex6$ cat echo_client.c

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main()
{
    int s, n;
    char buffer[1024];
    struct sockaddr_in server;

    s = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Connection failed");
        return 1;
    }

    printf("Enter a message: ");
    fgets(buffer, sizeof(buffer), stdin);

    send(s, buffer, strlen(buffer), 0);

    n = recv(s, buffer, sizeof(buffer) - 1, 0);

    if (n > 0)
    {
        buffer[n] = '\0';
        printf("Echo from server: %s", buffer);
    }

    close(s);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex6$
