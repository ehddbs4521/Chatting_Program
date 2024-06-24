#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 500 // 버프 크기
#define NAME_SIZE 20 // 최대 사용자 수

#define DUPLICATED_NAME_ERROR "이름 중복\n" // 이름 중복 메시지
#define DUPLICATED_ROOM_ERROR "방 번호 중복\n" // 방번호 중복 메시지
#define NOT_EXIST_ROOM "해당 방 번호 부재\n" // 방번호 부재 메시지
#define WRONG_KEY "키 오류\n" // 키 오류 메시지
#define NOT_EXIST_CLIENT "해당 이름의 사용자 부재\n" // 사용자 부재 메시지
#define NOT_ENOUGH_CACHE "잔액 부족\n" // 잔액 부족 메시지

void *send_msg(void *arg); // 메시지를 전송하는 로직
void *recv_msg(void *arg); // 메시지를 수신하는 로직
void error_handling(char *msg); // 연결 에러 핸들링 로직
void welcome_message(); // 사용자가 프로그램 접속 시 받는 메시지 로직
char name[NAME_SIZE] = "[DEFAULT]"; // 사용자 이름이 담긴 배열
char msg[BUF_SIZE]; // 사용자가 입력한 메시지가 담긴 배열

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void *thread_return;

    if (argc != 4)
    {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    
    if (!strcmp(argv[3], "all") || !strcmp(argv[3], "m") || !strcmp(argv[3], "l") || !strcmp(argv[3], "d") || !strcmp(argv[3], "p") || !strcmp(argv[3], "o") || !strcmp(argv[3], "e")) // 사용자 이름이 명령어와 동일하지 않도록 확인
    {
        printf("사용자의 이름이 명령어와 같으면 안됩니다.\n");
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
    while (1)
    {
        fgets(msg, BUF_SIZE, stdin);
        if (!strcmp(msg, "@q\n") || !strcmp(msg, "@Q\n")) // 사용자가 '@q'나 'Q' 입력시 프로그램 종료
        {
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
    char msg[NAME_SIZE + BUF_SIZE];
    int str_len;
    while (1)
    {
        memset(msg, 0, sizeof(msg));
        str_len = read(sock, msg, sizeof(msg) - 1);
        if (str_len == -1)
            return (void *)-1;

        msg[str_len] = 0;
        // 서버에서 보낸 오류 메시지들을 parsing하여 해당 메시지와 같으면 오류 출력
        if (!strcmp(msg, DUPLICATED_NAME_ERROR))
        {
            printf("이름이 중복되었으니 새로운 이름으로 시도하세요.\n");
            close(sock);
            exit(1);
        }
        else if (!strcmp(msg, DUPLICATED_ROOM_ERROR))
        {
            printf("방 번호 중복되었으니 새로운 방 번호로 시도하세요.\n");
        }
        else if (!strcmp(msg, NOT_EXIST_ROOM))
        {
            printf("입력한 방번호가 존재하지 않습니다. 다시 시도하세요.\n");
        }
        else if (!strcmp(msg, WRONG_KEY))
        {
            printf("키가 틀렸습니다. 다시 시도하세요.\n");
        }
        else if (!strcmp(msg, NOT_EXIST_CLIENT))
        {
            printf("해당 사용자가 존재하지 않습니다. 다시 시도하세요.\n");
        }
        else if (!strcmp(msg, NOT_ENOUGH_CACHE))
        {
            printf("잔액이 부족합니다. 다시 시도하세요.\n");
        }
        else
        {
            fputs(msg, stdout);
        }
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
    printf("=========방 기능==========\n");
    printf("\n 기능 리스트 (문자열)\n\n\n");
    printf("@m 방번호 : 방 만들기 \t ex) m 1\n");
    printf("@e 방번호 : 해당 방번호 접속하기 (주의: 방번호는 0부터 100까지) \t ex) e 1\n");
    printf("@o : 방 나가기\n");
    printf("@q : 채팅웹 종료\n");
    printf("@a 사용자명: 해당 사용자에게 귓속말하기 \t ex) @동윤 hello\n");
    printf("@all : 모든 사용자에게 채팅 전달하기\n");
    printf("\n================================\n\n\n");
}

