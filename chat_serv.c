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
void send_msg_only_one(char *msg, char *source, char *target,int len);
void send_msg_all(char *msg, char *source, int len);
void remove_first_word(const char *input, char *source, char *option, char *output);
int check_duplicate_name(char *name);
int check_annotation_option_user_name(char *option, char *target);

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
        } else {
            printf("접속한 사용자명: %s\n", clnt_name);
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
        if (!strcmp(clnt_names[i], name)) {
            return 1;
        }
    }
    return 0;
}

int check_option_equals_all(char *name) {
    return !strcmp(name, "all");
}

void *handle_clnt(void *arg)
{
    int clnt_sock = *((int *)arg);
    int str_len = 0, i;
    char msg[BUF_SIZE];
    char source[NAME_SIZE];
    char target[NAME_SIZE];

    while ((str_len = read(clnt_sock, msg, sizeof(msg) - 1)) != 0) {
        msg[str_len] = '\0';

        char modified[BUF_SIZE] = {0};
        char option[BUF_SIZE] = {0};

        remove_first_word(msg, source, option, modified);
        int opt=check_annotation_option_user_name(option,target);
        
        switch (opt)
        {
        case 1:
            send_msg_only_one(modified, source, target, strlen(modified));
            break;
        case 2:
            send_msg_all(modified, source, strlen(modified));
            break;
        default:
            send_msg(modified, source, strlen(modified));
            break;
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

void send_msg_all(char *msg, char *source, int len) {
    send_msg(msg, source, len);
}

void send_msg_only_one(char *msg, char *source, char *target,int len)
{
    int i;

    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg), "%s >> %s", source, msg);

    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++) {
        if (!strcmp(clnt_names[i], target)) {
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

int check_annotation_option_user_name(char *option, char *target) {
    
    sscanf(option, "@%s", target);

    if (check_duplicate_name(target)) {
        printf("1\n");
        return 1;
    } else if (check_option_equals_all(target)) {
        printf("2\n");
        return 2;
    }
    printf("0\n");
    return 0;
}

void remove_first_word(const char *input, char *source, char *option, char *output)
{
    int i = 0, j = 0, k = 0;

    while (input[i] != ' ' && input[i] != '\0') {
        source[i] = input[i];
        i++;
    }

    source[i] = '\0';

    if (input[i] == ' ') {
        i++;
    }

    if (input[i] == '@') {
        while (input[i] != ' ' && input[i] != '\0') {
            option[k++] = input[i++];
        }
        option[k] = '\0';
        if (input[i] == ' ') {
            i++;
        }
    }

    while (input[i] != '\0') {
        output[j++] = input[i++];
    }

    output[j] = '\0';

}