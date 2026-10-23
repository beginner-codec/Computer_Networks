k_v_nirupa@LenovoLap:~/sem5/cn/ex9$ cat self1server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>

#define PORT 8080
#define MAX_CLIENTS 50
#define MAX_LINE 1024

typedef struct
{
    char username[30];
    char password[30];
} User;

typedef struct
{
    int id;
    char question[200];
    char a[100], b[100], c[100], d[100];
    char correct;
} Question;

typedef struct
{
    int socket;
    int logged_in;
    int answered[10];
    int score;
    char username[30];
} Session;

User users[] = {
    {"student1", "pass123"},
    {"student2", "pass456"},
    {"student3", "pass789"}
};

Question questions[] = {
    {1, "Which protocol is connection-oriented?",
     "UDP", "TCP", "IP", "ARP", 'B'},
    {2, "Which layer routes packets?",
     "Physical", "Data Link", "Network", "Application", 'C'},
    {3, "Which protocol translates domain names to IP addresses?",
     "HTTP", "FTP", "DNS", "SMTP", 'C'}
};

int user_count = 3;
int question_count = 3;

pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_activity(const char *username, const char *activity)
{
    FILE *fp;
    time_t now;
    struct tm tm_info;
    char timestamp[30];

    now = time(NULL);

    if (localtime_r(&now, &tm_info) == NULL)
        return;

    strftime(timestamp, sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S", &tm_info);

    pthread_mutex_lock(&log_mutex);

    fp = fopen("server.log", "a");

    if (fp != NULL)
    {
        fprintf(fp, "[%s] User: %s | %s\n",
                timestamp, username, activity);
        fclose(fp);
    }

    pthread_mutex_unlock(&log_mutex);
}

int send_line(int sock, const char *message)
{
    char buffer[MAX_LINE];
    int len;
    int sent = 0;

    len = snprintf(buffer, sizeof(buffer), "%s\n", message);

    if (len < 0 || len >= MAX_LINE)
        return -1;

    while (sent < len)
    {
        int n = send(sock, buffer + sent, len - sent, 0);

        if (n <= 0)
            return -1;

        sent += n;
    }

    return 0;
}

int receive_line(int sock, char *buffer, size_t size)
{
    size_t i = 0;
    char ch;
    int n;

    if (size < 2)
        return -1;

    while (i < size - 1)
    {
        n = recv(sock, &ch, 1, 0);

        if (n <= 0)
            return -1;

        if (ch == '\n')
            break;

        if (ch != '\r')
            buffer[i++] = ch;
    }

    buffer[i] = '\0';

    return (int)i;
}

int authenticate(const char *username, const char *password)
{
    int i;
    int valid = 0;

    pthread_mutex_lock(&db_mutex);

    for (i = 0; i < user_count; i++)
    {
        if (strcmp(username, users[i].username) == 0 &&
            strcmp(password, users[i].password) == 0)
        {
            valid = 1;
            break;
        }
    }

    pthread_mutex_unlock(&db_mutex);

    return valid;
}

void show_questions(Session *s)
{
    int i;
    char buffer[MAX_LINE];
    int failed = 0;

    pthread_mutex_lock(&db_mutex);

    for (i = 0; i < question_count; i++)
    {
        snprintf(buffer, sizeof(buffer), "Q%d: %s",
                 questions[i].id, questions[i].question);

        if (send_line(s->socket, buffer) < 0)
        {
            failed = 1;
            break;
        }

        snprintf(buffer, sizeof(buffer),
                 "A. %s | B. %s | C. %s | D. %s",
                 questions[i].a, questions[i].b,
                 questions[i].c, questions[i].d);

        if (send_line(s->socket, buffer) < 0)
        {
            failed = 1;
            break;
        }
    }

    pthread_mutex_unlock(&db_mutex);

    if (!failed)
        send_line(s->socket, "END");
}

void submit_answer(Session *s, int qid, char answer)
{
    char message[200];
    int i;

    answer = (char)toupper((unsigned char)answer);

    if (qid < 1 || qid > question_count ||
        answer < 'A' || answer > 'D')
    {
        send_line(s->socket,
                  "Invalid question ID or answer.");
        send_line(s->socket, "END");
        log_activity(s->username, "Invalid answer request");
        return;
    }

    pthread_mutex_lock(&db_mutex);

    i = qid - 1;

    if (s->answered[i])
    {
        pthread_mutex_unlock(&db_mutex);

        send_line(s->socket,
                  "Answer already submitted for this question.");
        send_line(s->socket, "END");

        log_activity(s->username,
                     "Duplicate answer submission");
        return;
    }

    s->answered[i] = 1;

    if (answer == questions[i].correct)
    {
        s->score++;

        snprintf(message, sizeof(message),
                 "Correct answer! Current score: %d",
                 s->score);
    }
    else
    {
        snprintf(message, sizeof(message),
                 "Incorrect answer. Current score: %d",
                 s->score);
    }

    pthread_mutex_unlock(&db_mutex);

    send_line(s->socket, message);
    send_line(s->socket, "END");

    log_activity(s->username, "Submitted an answer");
}

void *handle_client(void *arg)
{
    Session *s = (Session *)arg;
    char buffer[MAX_LINE];
    char username[30];
    char password[30];
    char command[30];
    int qid;
    char answer;
    int i;

    s->logged_in = 0;
    s->score = 0;

    for (i = 0; i < 10; i++)
        s->answered[i] = 0;

    if (send_line(s->socket,
                  "Welcome to the Online Examination System.") < 0 ||
        send_line(s->socket,
                  "Login using: LOGIN username password") < 0 ||
        send_line(s->socket, "END") < 0)
    {
        close(s->socket);
        free(s);
        return NULL;
    }

    while (receive_line(s->socket, buffer,
                        sizeof(buffer)) >= 0)
    {
        if (sscanf(buffer, "%29s", command) != 1)
            continue;

        if (strcmp(command, "LOGIN") == 0)
        {
            if (sscanf(buffer, "%29s %29s %29s",
                       command, username, password) != 3)
            {
                send_line(s->socket,
                          "Usage: LOGIN username password");
                send_line(s->socket, "END");
                continue;
            }

            if (s->logged_in)
            {
                send_line(s->socket, "Already logged in.");
                send_line(s->socket, "END");
                continue;
            }

            if (authenticate(username, password))
            {
                s->logged_in = 1;

                strncpy(s->username, username,
                        sizeof(s->username) - 1);
                s->username[sizeof(s->username) - 1] = '\0';

                send_line(s->socket, "Login successful!");

                log_activity(s->username, "Logged in");
            }
            else
            {
                send_line(s->socket,
                          "Login failed. Invalid credentials.");

                log_activity(username,
                             "Failed login attempt");
            }

            send_line(s->socket, "END");
        }
        else if (strcmp(command, "QUIT") == 0)
        {
            send_line(s->socket, "Goodbye!");
            send_line(s->socket, "END");
            break;
        }
        else if (!s->logged_in)
        {
            send_line(s->socket,
                      "Please log in before using this command.");
            send_line(s->socket, "END");
        }
        else if (strcmp(command, "QUESTIONS") == 0)
        {
            log_activity(s->username, "Viewed questions");
            show_questions(s);
        }
        else if (strcmp(command, "ANSWER") == 0)
        {
            if (sscanf(buffer, "%29s %d %c",
                       command, &qid, &answer) != 3)
            {
                send_line(s->socket,
                          "Usage: ANSWER question_id option");
                send_line(s->socket, "END");

                log_activity(s->username,
                             "Invalid answer request");
                continue;
            }

            submit_answer(s, qid, answer);
        }
        else if (strcmp(command, "SCORE") == 0)
        {
            char result[100];

            snprintf(result, sizeof(result),
                     "Final score: %d/%d",
                     s->score, question_count);

            send_line(s->socket, result);
            send_line(s->socket, "END");

            log_activity(s->username, "Viewed score");
        }
        else
        {
            send_line(s->socket,
                      "Invalid command. Use QUESTIONS, ANSWER, SCORE or QUIT.");
            send_line(s->socket, "END");

            log_activity(s->username, "Invalid command");
        }
    }

    if (s->logged_in)
        log_activity(s->username, "Disconnected");

    close(s->socket);
    free(s);

    return NULL;
}

int main(void)
{
    int server_fd;
    int client_fd;
    int opt = 1;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    pthread_t thread_id;

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

    printf("Online Examination Server running on port %d...\n",
           PORT);

    while (1)
    {
        Session *s;

        addr_len = sizeof(client_addr);

        client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &addr_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        s = (Session *)calloc(1, sizeof(Session));

        if (s == NULL)
        {
            close(client_fd);
            continue;
        }

        s->socket = client_fd;

        if (pthread_create(&thread_id, NULL,
                           handle_client, s) != 0)
        {
            perror("pthread_create");
            close(client_fd);
            free(s);
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_fd);

    pthread_mutex_destroy(&db_mutex);
    pthread_mutex_destroy(&log_mutex);

    return 0;
}

k_v_nirupa@LenovoLap:~/sem5/cn/ex9$ cat self1client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define MAX_LINE 1024

int send_line(int sock, const char *message)
{
    char buffer[MAX_LINE];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", message);
    int sent = 0;

    if (len < 0 || len >= MAX_LINE)
        return -1;

    while (sent < len)
    {
        int n = send(sock, buffer + sent, len - sent, 0);
        if (n <= 0)
            return -1;
        sent += n;
    }

    return 0;
}

int receive_line(int sock, char *buffer, size_t size)
{
    size_t i = 0;
    char ch;
    int n;

    while (i < size - 1)
    {
        n = recv(sock, &ch, 1, 0);

        if (n <= 0)
            return -1;

        if (ch == '\n')
            break;

        if (ch != '\r')
            buffer[i++] = ch;
    }

    buffer[i] = '\0';
    return (int)i;
}

int receive_response(int sock)
{
    char buffer[MAX_LINE];

    while (receive_line(sock, buffer, sizeof(buffer)) >= 0)
    {
        if (strcmp(buffer, "END") == 0)
            return 0;

        printf("%s\n", buffer);
    }

    return -1;
}

int main(void)
{
    int sock;
    struct sockaddr_in server_addr;
    char command[MAX_LINE];

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

    if (receive_response(sock) < 0)
    {
        printf("Server disconnected.\n");
        close(sock);
        return 1;
    }

    printf("\nCommands:\n");
    printf("LOGIN username password\n");
    printf("QUESTIONS\n");
    printf("ANSWER question_id option\n");
    printf("SCORE\n");
    printf("QUIT\n\n");

    while (1)
    {
        printf("Enter command: ");

        if (fgets(command, sizeof(command), stdin) == NULL)
            break;

        command[strcspn(command, "\n")] = '\0';

        if (strlen(command) == 0)
            continue;

        if (send_line(sock, command) < 0)
        {
            printf("Failed to send request.\n");
            break;
        }

        if (receive_response(sock) < 0)
        {
            printf("Server disconnected.\n");
            break;
        }

        if (strncmp(command, "QUIT", 4) == 0 &&
            (command[4] == '\0' || command[4] == ' '))
            break;
    }

    close(sock);
    return 0;
}
k_v_nirupa@LenovoLap:~/sem5/cn/ex9$
