#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 100
#define MAX_CLNT 256
#define NAME_SIZE 20

void *handle_clnt(void *arg);
void send_msg(char *msg, char *source, int len);
void error_handling(char *msg);
void send_msg_only_one(char *msg, char *source, int len);
void remove_first_word(const char *input, char *source, char *output);
int check_duplicate_name(char *name);

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
char clnt_name[NAME_SIZE];
char clnt_names[MAX_CLNT][NAME_SIZE];

pthread_mutex_t mutx;

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        if (clnt_sock == -1)
            error_handling("accept() error");

        int str_len = read(clnt_sock, clnt_name, NAME_SIZE - 1);

        if (str_len == -1)
            error_handling("read() error");

        clnt_name[str_len] = '\0';

        pthread_mutex_lock(&mutx);
        if (check_duplicate_name(clnt_name)) {
            write(clnt_sock, "이름 중복", strlen("이름 중복"));
            close(clnt_sock);
            pthread_mutex_unlock(&mutx);
            continue;
        }

        strcpy(clnt_names[clnt_cnt], clnt_name);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
        printf("========================\n");
    }
    close(serv_sock);
    return 0;
}

int check_duplicate_name(char *name) {
    for (int i = 0; i < clnt_cnt; i++) {
        if (strcmp(clnt_names[i], name) == 0) {
            return 1; // Duplicate found
        }
    }
    return 0; // No duplicate
}

void *handle_clnt(void *arg)
{
    int clnt_sock = *((int *)arg);
    int str_len = 0, i;
    char msg[BUF_SIZE];
    char source[NAME_SIZE];

    while ((str_len = read(clnt_sock, msg, sizeof(msg) - 1)) != 0) {
        msg[str_len] = '\0';

        char modified[BUF_SIZE] = {0};

        remove_first_word(msg, source, modified);

        if (modified[0] == '@') {
            send_msg_only_one(modified, source, strlen(modified));
        } else {
            send_msg(modified, source, strlen(modified));
        }
    }

    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++) {

        if (clnt_sock == clnt_socks[i]) {
            while (i < clnt_cnt - 1) {
                clnt_socks[i] = clnt_socks[i + 1];
                strcpy(clnt_names[i], clnt_names[i + 1]);
                i++;
            }
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    return NULL;
}

void send_msg(char *msg, char *source, int len)
{
    int i;
    char everyone_msg[BUF_SIZE];
    snprintf(everyone_msg, sizeof(everyone_msg), "[%s] %s", source, msg);

    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], everyone_msg, strlen(everyone_msg));
    pthread_mutex_unlock(&mutx);
}

void send_msg_only_one(char *msg, char *source, int len)
{
    int i;
    char target_name[NAME_SIZE];
    char *msg_body;
    sscanf(msg, "@%s", target_name);
    msg_body = strchr(msg, ' ');

    if (msg_body != NULL) {
        msg_body++;
    } else {
        return;
    }

    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg), "%s >> %s", source, msg_body);

    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++) {
        if (!strcmp(clnt_names[i], target_name)) {
            write(clnt_socks[i], specific_msg, strlen(specific_msg));
            break;
        }
    }
    pthread_mutex_unlock(&mutx);
}

void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

void remove_first_word(const char *input, char *source, char *output)
{
    int i = 0;
    int j = 0;

    while (input[i] != ' ' && input[i] != '\0') {
        source[i] = input[i];
        i++;
    }

    source[i] = '\0';

    if (input[i] == ' ') {
        i++;
    }

    while (input[i] != '\0') {
        output[j++] = input[i++];
    }
    output[j] = '\0';
}