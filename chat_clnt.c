#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 100
#define NAME_SIZE 20

void *send_msg(void *arg);
void *recv_msg(void *arg);
void error_handling(char *msg);
void welcome_message();

char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void *thread_return;

    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    if (!strcmp(argv[3], "all")) {
        printf("사용자의 이름이 all이면 안됩니다.\n");
        exit(1);
    }

    sprintf(name, "%s", argv[3]);
    sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    write(sock, name, strlen(name));

    pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);
    close(sock);
    return 0;
}

void *send_msg(void *arg)
{
    int sock = *((int *)arg);
    char name_msg[NAME_SIZE + BUF_SIZE];

    welcome_message();

    while (1) {
        fgets(msg, BUF_SIZE, stdin);
        if (!strcmp(msg, "@q\n") || !strcmp(msg, "@Q\n")) {
            close(sock);
            exit(0);
        }

        snprintf(name_msg, sizeof(name_msg), "%s %s", name, msg);
        write(sock, name_msg, strlen(name_msg));
    }
    return NULL;
}

void *recv_msg(void *arg)
{
    int sock = *((int *)arg);
    char name_msg[NAME_SIZE + BUF_SIZE];
    int str_len;

    while (1) {
        memset(name_msg, 0, sizeof(name_msg));
        str_len = read(sock, name_msg, sizeof(name_msg) - 1);
        if (str_len == -1)
            return (void *)-1;

        name_msg[str_len] = 0;
        if (strcmp(name_msg, "이름 중복") == 0) {
            printf("이름이 중복되었으니 새로운 이름으로 시도하세요.\n");
            close(sock);
            exit(1);
        }

        fputs(name_msg, stdout);
    }
    return NULL;
}

void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

void welcome_message()
{
    printf("=========Temporary Room==========\n");
    printf("\n 기능 리스트 (문자열)\n\n\n");
    printf("@m 방번호 : 방 만들기 \t ex) m 1\n");
    printf("@e 방번호 : 해당 방번호 접속하기 \t ex) e 1\n");
    printf("@o : 방 나가기\n");
    printf("@q : 채팅웹 종료\n");
    printf("@a 사용자명: 해당 사용자에게 귓속말하기 \t ex) @동윤 hello\n");
    printf("@all : 모든 사용자에게 채팅 전달하기\n");
    printf("\n================================\n\n\n");
}