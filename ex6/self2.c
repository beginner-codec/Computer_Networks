k_v_nirupa@LenovoLap:~/sem5/cn/ex6$ cat palindrom_client.c

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main()
{
    int s, n;
    char str[1024], result[100];
    struct sockaddr_in server;

    s = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Connection failed");
        return 1;
    }

    printf("Enter a string: ");
    fgets(str, sizeof(str), stdin);

    send(s, str, strlen(str), 0);

    n = recv(s, result, sizeof(result) - 1, 0);

    if (n > 0)
    {
        result[n] = '\0';
        printf("Result: %s\n", result);
    }

    close(s);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex6$ cat palindrom_server.c

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

int main()
{
    int s, c, n, i, j, flag;
    char str[1024], msg[100];
    struct sockaddr_in server, client;
    socklen_t len = sizeof(client);

    s = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(8081);
    server.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr *)&server, sizeof(server));
    listen(s, 5);

    printf("Palindrome server waiting...\n");

    c = accept(s, (struct sockaddr *)&client, &len);

    n = recv(c, str, sizeof(str) - 1, 0);

    if (n > 0)
    {
        str[n] = '\0';

        while (n > 0 && (str[n - 1] == '\n' || str[n - 1] == '\r'))
            str[--n] = '\0';

        i = 0;
        j = n - 1;
        flag = 1;

        while (i < j)
        {
            if (tolower((unsigned char)str[i]) !=
                tolower((unsigned char)str[j]))
            {
                flag = 0;
                break;
            }
            i++;
            j--;
        }

        if (flag)
            strcpy(msg, "Palindrome");
        else
            strcpy(msg, "Not a palindrome");

        send(c, msg, strlen(msg) + 1, 0);
    }

    close(c);
    close(s);
    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex6$
