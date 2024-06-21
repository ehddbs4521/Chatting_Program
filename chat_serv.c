#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 500  // 버프 크기
#define MAX_CLNT 256  // 최대 사용자 수
#define NAME_SIZE 20  // 최대 이름 길이
#define MAX_ROOM_KEY_LENGTH 4 // 최대 방 키 길이
#define MAX_ROOM_COUNT 100 // 최대 채팅방 갯수
#define DEFAULT_MONEY 10000  // 접속시 기본으로 생성되는 금액

#define DUPLICATED_NAME_ERROR "이름 중복\n" // 이름 중복 메시지
#define DUPLICATED_ROOM_ERROR "방 번호 중복\n" // 방번호 중복 메시지
#define NOT_EXIST_ROOM "해당 방 번호 부재\n" // 방번호 부재 메시지
#define WRONG_KEY "키 오류\n" // 키 오류 메시지
#define NOT_EXIST_CLIENT "해당 이름의 사용자 부재\n" // 사용자 부재 메시지
#define NOT_ENOUGH_CACHE "잔액 부족\n" // 잔액 부족 메시지

#define CHATTING_ROOM_NAMES "=============채팅방 접속 인원 이름=============\n" // 접속 인원 메시지
#define BAR "==========================================\n" // Bar 메시지

void *handle_clnt(void *arg); // 사용자의 메시지에 따른 로직
void send_msg(char *msg, char *source, int len, int clnt_sock); // 임시방에있는 사용자끼리 채팅하는 로직
void error_handling(char *msg); // 연결 에러 핸들링 로직
void send_msg_only_one(char *msg, char *source, char *target, int len, int clnt_sock); // 귓속말 로직
void send_msg_all(char *msg, char *source, int clnt_sock); // 모든 사람에게 메시지 전달하는 로직
void remove_first_word(char *input, char *source, char *option, char *output); // 사용자의 메시지를 전송자, 타겟, 옵션, 내용 등을 분할하는 로직
void check_annotation_option(char *option, char *source, char *target, char *modified, int room[], int clnt_sock); // 옵션에 따른 로직
int check_duplicate_name(char *name); // 접속 시 사용자의 이름이 현재 접속해있는 사용자들의 이름과 충돌이 일어나는지 검증하는 로직
void room_message(int clnt_sock); // 채팅방에 접속했을 시 나타나는 메시지 로직
void make_room(char *msg, int room[], int clnt_sock); // 채팅방 만드는 로직
int check_duplicate_room(char *msg, int clnt_sock); // 채팅방을 만들때 방의 이름이 다른 채팅방의 이름과 충돌이 일어나는지 검증하는 로직
void send_every_clear_msg(char *msg, int len); // 모든 사용자의 화면을 clear하는 로직
void send_every_welcome_msg(); // 모든 사용자에게 웰컴 메시지를 전달하는 로직
void send_welcome_msg(int clnt_sock); // 임시방에있는 사용자에게 웰컴 메시지를 전달하는 로직
int check_exists_room(char *msg, int room[]); // 사용자가 채팅방에 접속을 할 때 입력한 채팅방 번호가 존재하는지 검증하는 로직
void enter_room(char *msg, int clnt_sock); // 채팅방에 접속하는 로직
int exist_clnt_in_room(int room_clnt_socks[], int clnt_sock);  // 사용자가 현재 채팅방에 접속해있는 사용자인지 검증하는 로직
void send_msg_only_room(int clnt_sock, int room_num, char *msg); // 채팅방에 접속해있는 사용자들끼리 채팅하는 로직
void send_clear_msg(int clnt_sock); // 임시방 or 채팅방을 접속 할 때 화면을 clear하는 로직
void arrange_array(int data[]); // 사용자가 방을 나갔을 시 채팅방에 접속한 인원에 대한 정보 삭제하는 로직
void show_members(int clnt_sock); // 현재 채팅방에 접속해있는 사용자들의 이름을 보여주는 로작
void dutch_pay(int clnt_sock, char *msg); // 카카오톡 더치패이 송금 기능처럼 인당 금액을 공지하는 로직
void pay_money(int clnt_sock, char *msg, char *source); // 카카오톡 송금 기능처럼 송금하는 로직

int clnt_cnt = 0; // 접속한 사용자 수
int clnt_socks[MAX_CLNT]; // 사용자의 소켓 번호
char clnt_name[NAME_SIZE]; // 사용자의 이름
char clnt_names[MAX_CLNT][NAME_SIZE]; // 사용자의 수와 이름 배열
char clnt_sock_names[MAX_CLNT][NAME_SIZE]; // 사용자의 소켓 번호와 이름 배열
int room_key[MAX_ROOM_COUNT][MAX_ROOM_KEY_LENGTH] = {0}; // 방 번호와 해당 키
int room_num_start = 0; // 방 번호 시작
int room_num_end = 0; // 방 번호 끝
int room_names[MAX_ROOM_COUNT][MAX_CLNT]; // 방 번호와 방 이름 배열
int clnt_money[MAX_CLNT][1] = {0}; // 사용자의 소켓 번호와 잔고 배열
pthread_mutex_t mutx;
int room_clnt_socks[MAX_CLNT];  // 채팅방에 접속해있는 사용자의 소켓 번호 배열
int room[MAX_ROOM_COUNT] = {0}; // 방 번호 배열
int room_num = 0; // 현재 접속한 방 번호
int chatting_room_total = 0; // 채팅방에 접속해있는 사용자 수
int money = 0; // 지불해야 할 금액
int key = 0; // 방 키

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    if (argc != 2)
    {
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

    while (1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        if (clnt_sock == -1)
            error_handling("accept() error");

        int str_len = read(clnt_sock, clnt_name, NAME_SIZE - 1);

        if (str_len == -1)
            error_handling("read() error");

        clnt_name[str_len] = '\0';

        pthread_mutex_lock(&mutx);
        if (check_duplicate_name(clnt_name)) // 접속하려는 이름과 접속중인 이름의 충돌 판단
        {
            write(clnt_sock, DUPLICATED_NAME_ERROR, strlen(DUPLICATED_NAME_ERROR));
            close(clnt_sock);
            pthread_mutex_unlock(&mutx);
            continue;
        }
        else
        {
            printf("접속한 사용자명: %s\n", clnt_name);
        }

        strcpy(clnt_names[clnt_cnt], clnt_name);       // 사용자 이름 저장
        strcpy(clnt_sock_names[clnt_sock], clnt_name); // 사용자 소켓 번호와 이름 저장
        clnt_socks[clnt_cnt++] = clnt_sock;            // 사용자 소켓 번호 저장
        clnt_money[clnt_sock][0] = DEFAULT_MONEY;      // 접속시 디폴트 금액 설정
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
        pthread_detach(t_id);
        printf("Connected client IP: %s port: %d\n", inet_ntoa(clnt_adr.sin_addr), clnt_sock);
        printf("========================\n");
    }
    close(serv_sock);
    return 0;
}

int check_duplicate_room(char *msg, int clnt_sock)
{

    char *token;
    char *msg_copy = strdup(msg);
    room_num = 0;

    token = strtok(msg_copy, " ");

    if (token != NULL)
    {
        room_num = atoi(token);
    }

    int start = 0;

    for (int i = room_num_start; i < room_num_end; i++)
    {
        if (room[i] == room_num)
        {
            return 1; // room배열에서 사용자가 입력한 방번호가 있으면 1 반환
        }
    }
    room_names[room_num][0] = clnt_sock; // 없으면 입력한 방 배열에 방 생성자의 소켓 번호 세팅

    while (1)
    {
        if (room_clnt_socks[start] == 0)
        {
            break;
        }
        else
        {
            start++;
        }
    }
    room_clnt_socks[start] = clnt_sock; // 채팅방에 접속중인 사용자 리스트에 채팅방 생성자의 소켓 번호 할당

    return 0;
}

int check_duplicate_name(char *name)
{
    for (int i = 0; i < clnt_cnt; i++)
    {
        if (!strcmp(clnt_names[i], name)) // 사용자 이름이 담긴 배열에 사용자가 입력한 이름이있는지 검증
        {
            return 1;
        }
    }
    return 0;
}

int check_exists_room(char *msg, int room[])
{

    char *msg_copy = strdup(msg);
    char *token;
    int typed_room_num = 0;

    token = strtok(msg_copy, " ");

    if (token != NULL)
    {
        typed_room_num = atoi(token);
    }

    for (int i = 0; i < MAX_ROOM_COUNT; i++)
    {
        if (room[i] == typed_room_num) // 사용자가 입력한 방번호가 방번호들이 담긴 배열에있는지 검증
        {
            return 1;
        }
    }

    return 0;
}

int exist_clnt_in_room(int room_clnt_socks[], int clnt_sock)
{

    for (int i = 0; i < clnt_cnt; i++)
    {
        if (clnt_sock == room_clnt_socks[i]) // 사용자의 소켓 번호가 방에 접속중인 소켓 번호인지 검증
        {
            return 1;
        }
    }
    return 0;
}

int check_room_key(char *msg)
{

    char *token;
    char *msg_copy = strdup(msg);

    int typed_room = 0;
    int typed_key = 0;

    token = strtok(msg_copy, " ");

    if (token != NULL)
    {
        typed_room = atoi(token);
    }

    token = strtok(NULL, " ");
    if (token != NULL)
    {
        typed_key = atoi(token);
    }

    if (typed_key != room_key[typed_room][0]) // 해당 방번호에 설정해놓은 방 키와 사용자가 입력한 방 키가 같은지 검증
    {
        return 0;
    }

    return 1;
}

void make_room(char *msg, int room[], int clnt_sock)
{

    char *msg_copy = strdup(msg);
    char *token;

    token = strtok(msg_copy, " ");
    if (token != NULL)
    {
        room_num = atoi(token);
    }

    token = strtok(NULL, " ");
    if (token != NULL)
    {
        key = atoi(token);
    }

    room[room_num_end++] = room_num; // 방 번호가 담긴 배열에 사용자가 생성한 방 번호 할당

    room_key[room_num][0] = key; // 방 키가 담긴 배열에 사용자가 생성한 방 키 할당

    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg),
             "\t대화방 목록 (입장은 방 번호를 입력해주세요.)\n"
             "방 현황 [%d / %d]\n"
             "===================================================\n",
             room_num_end, MAX_ROOM_COUNT);

    pthread_mutex_lock(&mutx);
    for (int i = 0; i < clnt_cnt; i++)
    {
        if (clnt_socks[i] == clnt_sock)
        {
            room_message(clnt_sock); // 방 생성자에게 방에 접속했다는 메시지 전송
        }
        else
        {
            write(clnt_socks[i], specific_msg, strlen(specific_msg)); // 방 생성자를 제외한 모든 사용자에게 방 생성시 생성 방 현황에 대해 메시지 전송
        }
    }
    chatting_room_total++; // 현재 채팅방에 접속해있는 인원 수 증가
    pthread_mutex_unlock(&mutx);
}

void enter_room(char *msg, int clnt_sock)
{

    send_clear_msg(clnt_sock); // 방 입장 시 터미널 창을 clear

    if (!check_room_key(msg)) // 방 키가 틀렸을 시 오류 메시지 전송
    {
        write(clnt_sock, WRONG_KEY, strlen(WRONG_KEY));
        return;
    }
    else
    {
        room_message(clnt_sock);
        int room_num = atoi(msg);
        int start = 0;

        while (room_names[room_num][start] != 0)
        {
            start++;
        }

        room_names[room_num][start] = clnt_sock; // 입력한 방 번호 배열에서 start번째에 입장한 사용자의 소켓 번호를 할당

        start = 0;

        while (1)
        {
            if (room_clnt_socks[start] == clnt_sock)
            {
                break;
            }
            start++;
            if (room_clnt_socks[start] == 0)
            {
                room_clnt_socks[start] = clnt_sock; // 채팅방에 접속한 사용자 소켓 번호 배열에서 start번째에 입장한 사용자의 소켓 번호를 할당
                break;
            }
        }

        chatting_room_total++; // 현재 채팅방에 접속해있는 인원 수 증가
    }
}

void exit_room(int clnt_sock)
{

    send_clear_msg(clnt_sock);   // 해당 사용자의 터미널을 clear
    send_welcome_msg(clnt_sock); // 임시 방 접속시 welcome 메시지 전송

    for (int i = 0; i < MAX_CLNT; i++)
    {
        if (room_clnt_socks[i] == clnt_sock)
        {
            room_clnt_socks[i] = 0; // 현재 채팅방에 접속한 사용자 소켓 배열에서 기존 인덱스의 값을 0으로 초기화
        }

        if (room_names[room_num][i] == clnt_sock)
        {
            room_names[room_num][i] = 0; // 접속했던 채팅방에 사용자 소켓 배열에서 기존 인덱스의 값을 0으로 초기화
        }
    }

    chatting_room_total--; // 현재 채팅방에 접속해있는 인원 수 감소

    arrange_array(room_clnt_socks);      // 기존 사용자의 소켓 번호 인덱스 값을 0으로 세팅해주었기에 뒤의 0이 아닌 값들을 모두 앞으로 당김
    arrange_array(room_names[room_num]); // 기존 방 번호 인덱스 값을 0으로 세팅해주었기에 뒤의 0이 아닌 값들을 모두 앞으로 당김
}

void arrange_array(int data[])
{

    for (int i = 0; i < MAX_CLNT + 1; i++)
    {
        if (data[i] == 0)
        {
            data[i] = data[i + 1];
        }
    }
}

void room_message(int clnt_sock)
{
    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg),
             "============새로운 방이 생성되었습니다.============\n\n"
             "방을 나가려면 @q를 누르세요.\n"
             "===================================================\n");
    write(clnt_sock, specific_msg, strlen(specific_msg));
}

void *handle_clnt(void *arg)
{
    int clnt_sock = *((int *)arg);
    int str_len = 0, i;
    char msg[BUF_SIZE];
    char source[NAME_SIZE];
    char target[NAME_SIZE];

    while ((str_len = read(clnt_sock, msg, sizeof(msg) - 1)) != 0)
    {
        msg[str_len] = '\0';

        char modified[BUF_SIZE] = {0};
        char option[BUF_SIZE] = {0};
        remove_first_word(msg, source, option, modified);                           // 사용자가 입력한 문자들을 알맞는 배열에 할당
        check_annotation_option(option, source, target, modified, room, clnt_sock); // 옵션에 따른 로직
    }

    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
    {
        if (clnt_sock == clnt_socks[i])
        {
            while (i < clnt_cnt - 1)
            {
                clnt_socks[i] = clnt_socks[i + 1]; // 사용자가 프로그램을 종료 할 시 뒤의 소켓 번호를 모두 앞으로 한칸씩 당김
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
void send_clear_msg(int clnt_sock)
{
    pthread_mutex_lock(&mutx);
    write(clnt_sock, "\033[H\033[J\n", strlen("\033[H\033[J\n")); // clear하는 코드
    pthread_mutex_unlock(&mutx);
}

void send_every_clear_msg(char *msg, int len)
{

    pthread_mutex_lock(&mutx);
    for (int i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], "\033[H\033[J\n", strlen("\033[H\033[J\n")); // clear하는 코드
    pthread_mutex_unlock(&mutx);
}

void send_every_welcome_msg()
{
    int i;
    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg),
             "=========방 기능==========\n"
             "\n 기능 리스트 (문자열)\n\n\n"
             "@m 방번호 : 방 만들기 \t ex) m 1\n"
             "@e 방번호 : 해당 방번호 접속하기 \t ex) e 1\n"
             "@o : 방 나가기\n"
             "@q : 채팅웹 종료\n"
             "@a 사용자명: 해당 사용자에게 귓속말하기 \t ex) @동윤 hello\n"
             "@all : 모든 사용자에게 채팅 전달하기\n"
             "\n================================\n\n\n");
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], specific_msg, strlen(specific_msg));
    pthread_mutex_unlock(&mutx);
}

void send_welcome_msg(int clnt_sock)
{
    int i;
    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg),
             "=========방 기능==========\n"
             "\n 기능 리스트 (문자열)\n\n\n"
             "@m 방번호 : 방 만들기 \t ex) m 1\n"
             "@e 방번호 : 해당 방번호 접속하기 \t ex) e 1\n"
             "@o : 방 나가기\n"
             "@q : 채팅웹 종료\n"
             "@a 사용자명: 해당 사용자에게 귓속말하기 \t ex) @동윤 hello\n"
             "@all : 모든 사용자에게 채팅 전달하기\n"
             "\n================================\n\n\n");
    pthread_mutex_lock(&mutx);
    write(clnt_sock, specific_msg, strlen(specific_msg));
    pthread_mutex_unlock(&mutx);
}

void send_msg(char *msg, char *source, int len, int clnt_sock)
{
    int i;
    int flag = 1;
    char everyone_msg[BUF_SIZE];
    snprintf(everyone_msg, sizeof(everyone_msg), "[%s] %s", source, msg);

    pthread_mutex_lock(&mutx);

    flag = exist_clnt_in_room(room_clnt_socks, clnt_sock); // 해당 사용자가 채팅방에 접속중인 사용자인지 flag에 값 할당

    if (flag)
    {
        send_msg_only_room(clnt_sock, room_num, everyone_msg); // 채팅방에 접속 중이기에 채팅방에있는 사용자들에게만 메시지 전송
    }
    else
    {
        for (i = 0; i < clnt_cnt; i++)
        {
            if (!exist_clnt_in_room(room_clnt_socks, clnt_socks[i])) // 해당 사용자 제외 모든 사용자들이 채팅방에 접속중인 사용자인지 검증
            {
                write(clnt_socks[i], everyone_msg, strlen(everyone_msg)); // 임시방에있는 사용자들에게만 메시지 전송
            }
        }
    }

    pthread_mutex_unlock(&mutx);
}

void send_msg_all(char *msg, char *source, int clnt_sock)
{

    char everyone_msg[BUF_SIZE];
    snprintf(everyone_msg, sizeof(everyone_msg), "[%s] %s", source, msg);

    pthread_mutex_lock(&mutx);
    for (int i = 0; i < clnt_cnt; i++)
    {
        if (clnt_socks[i] != clnt_sock)
        {
            write(clnt_socks[i], everyone_msg, strlen(everyone_msg)); // 모든 사용자들에게 메시지 전송
        }
    }
    pthread_mutex_unlock(&mutx);
}

void send_msg_only_one(char *source, char *target, char *modified, int len, int clnt_sock)
{
    int i;
    int flag = 0;

    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg), "%s >> %s", source, modified);

    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
    {
        if (!strcmp(clnt_names[i], target)) // 사용자가 입력한 타겟이 사용자 이름에 담긴 배열에 저장되있는지 검증
        {
            write(clnt_socks[i], specific_msg, strlen(specific_msg)); // 저장되있으면 해당 타겟에게 메시지 전송
            flag = 1;
            break;
        }
    }

    if (!flag)
    {
        write(clnt_sock, NOT_EXIST_CLIENT, strlen(NOT_EXIST_CLIENT));
    }
    pthread_mutex_unlock(&mutx);
}

void send_msg_only_room(int clnt_sock, int room_num, char *msg)
{

    for (int i = 0; i < chatting_room_total; i++) // 채팅방에 접속해있는 인원수만큼 반복문 시행
    {
        if (room_names[room_num][i] != clnt_sock) // 사용자가 입력한 방 번호 배열에 저장된 사용자 소켓들에서 본인의 소켓 번호를 제외한 사용자에게 메시지 전송
        {
            write(room_names[room_num][i], msg, strlen(msg));
        }
    }
}

void show_members(int clnt_sock)
{
    write(clnt_sock, CHATTING_ROOM_NAMES, strlen(CHATTING_ROOM_NAMES));
    for (int i = 0; i < chatting_room_total; i++) // 현재 채팅방에 접속 중인 사용자만큼 반복문 시행
    {
        char buffer[256];
        int len = snprintf(buffer, sizeof(buffer), "%s\n", clnt_sock_names[room_clnt_socks[i]]); // 해당 방 번호의 소켓 번호들을 이름 배열이 담긴 배열에서 추출

        write(clnt_sock, buffer, len);
    }
    write(clnt_sock, BAR, strlen(BAR));
}

void dutch_pay(int clnt_sock, char *msg)
{
    money = atoi(msg);

    char everyone_msg[BUF_SIZE];
    snprintf(everyone_msg, sizeof(everyone_msg), "총 금액 [%d]원, 1인당 지불 금액 [%d]원\n", money, money / chatting_room_total); // 입력한 금액을 현재 채팅방에 접속한 인원 수 만큼 더치패이

    for (int i = 0; i < chatting_room_total; i++) // 채팅방에 접속해있는 인원수만큼 반복문 시행
    {
        write(room_names[room_num][i], everyone_msg, strlen(everyone_msg));
    }
}

void pay_money(int clnt_sock, char *msg, char *source)
{

    char *msg_copy = strdup(msg);
    char *token;
    char *target;
    char everyone_msg[BUF_SIZE];

    int withdraw_money = 0;
    int target_sock = 0;
    int flag = 0;

    token = strtok(msg_copy, " ");
    if (token != NULL)
    {
        target = token;
    }

    for (int i = 0; i < MAX_CLNT; i++)
    {
        if (!strcmp(clnt_sock_names[i], target)) // 돈을 송금 받을 사용자의 이름이 현재 프로그램에 접속중인 사용자인지 검증
        {
            target_sock = i;
            flag = 1; // 사용자의 이름이 현재 프로그램에 접속중인 사용자라면 flag를 1로 세팅
            break;
        }
    }

    if (!flag) // 사용자의 이름이 현재 프로그램에 접속중인 사용자가 아니면 오류 메시지 전송
    {
        write(clnt_sock, NOT_EXIST_CLIENT, strlen(NOT_EXIST_CLIENT));
        return;
    }

    token = strtok(NULL, " ");

    if (token != NULL)
    {
        withdraw_money = atoi(token);
    }

    if (clnt_money[clnt_sock][0] - withdraw_money < 0) // 이체 금액이 잔고보다 크면 오류 메시지 전송
    {
        write(clnt_sock, NOT_ENOUGH_CACHE, strlen(NOT_ENOUGH_CACHE));
        return;
    }

    clnt_money[target_sock][0] += withdraw_money; // 상대방의 계좌는 이체금액만큼 증가
    clnt_money[clnt_sock][0] -= withdraw_money;   // 본인의 계좌는 이체금액만큼 감소

    snprintf(everyone_msg, sizeof(everyone_msg), BAR "%s의 현재 남은 잔액: %d원\n" BAR, source, clnt_money[clnt_sock][0]);
    write(clnt_sock, everyone_msg, strlen(everyone_msg));

    snprintf(everyone_msg, sizeof(everyone_msg), "");

    snprintf(everyone_msg, sizeof(everyone_msg), BAR "%s님으로부터 %d원 입금\n"
                                                     "%s의 현재 남은 잔액: %d원\n" BAR,
             source, withdraw_money, target, clnt_money[target_sock][0]);

    write(target_sock, everyone_msg, strlen(everyone_msg));
}

void error_handling(char *msg) // 연결 관련 에러 메시지 전송
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

void check_annotation_option(char *option, char *source, char *target, char *modified, int room[], int clnt_sock)
{
    if (option[0] == '\0') // option이 없을 시 채팅
    {
        send_msg(modified, source, strlen(modified), clnt_sock);
    }
    else if (option[1] == 'm') // option이 'm'일 시 방 만들기
    {
        if (!check_duplicate_room(modified, clnt_sock)) // 생성할 방 번호가 기존의 방 번호들과 충돌나는지 검증
        {
            send_every_clear_msg(modified, strlen(modified));
            send_every_welcome_msg();
            make_room(modified, room, clnt_sock); // 방 만들기
        }
        else
        {
            write(clnt_sock, DUPLICATED_ROOM_ERROR, strlen(DUPLICATED_ROOM_ERROR)); // 생성할 방 번호가 기존의 방 번호들과 충돌이 난다면 오류 메시지 전송
        }
    }
    else if (option[1] == 'e') // option이 'e'일 시 해당 방 번호로 입장
    {
        if (check_exists_room(modified, room)) // 해당 방 번호가 존재하는 방인지 검증
        {
            enter_room(modified, clnt_sock); // 방 입장
        }
        else
        {
            write(clnt_sock, NOT_EXIST_ROOM, strlen(NOT_EXIST_ROOM)); // 존재하지 않는 방 번호라면 오류 메시지 전송
        }
    }
    else if (!strcmp(option, "@all")) // option이 '@all'일 시 모든 사용자에게 메시지 전송
    {
        send_msg_all(modified, source, clnt_sock);
    }
    else if (option[1] == 'o') // option이 'o'일 시 해당 방에서 퇴장
    {
        exit_room(clnt_sock);
    }
    else if (option[1] == 'l') // option이 'l'일 시 접속 중인 채팅방 인원 목록 보기
    {
        show_members(clnt_sock);
    }
    else if (strlen(option) == 2 && option[1] == 'd') // option이 'd'일 시 접속 중인 채팅방 인원들에게 입력한 금액과 더치페이 한 금액 공지
    {
        dutch_pay(clnt_sock, modified);
    }
    else if (option[1] == 'p') // option이 'p'일 시 송금 받을 상대방에게 입력한 금액만큼 이체
    {
        pay_money(clnt_sock, modified, source);
    }
    else
    {
        strcpy(target, option + 1);
        send_msg_only_one(source, target, modified, strlen(modified), clnt_sock); // 귓속말
    }
}

void remove_first_word(char *input, char *source, char *option, char *output)
{
    /*

        ex) dy가 son에게 5000원을 이체하고싶어서 dy는 터미널로 @d son 3000을 입력

        => input: dy @d son 3000이 됨
    */

    int i = 0, j = 0, k = 0;

    while (input[i] != ' ' && input[i] != '\0')
    {
        source[i] = input[i]; // 본인의 이름을 source 배열에 저장
        i++;
    }

    source[i] = '\0';

    if (input[i] == ' ')
    {
        i++;
    }

    if (input[i] == '@')
    {
        while (input[i] != ' ' && input[i] != '\0')
        {
            option[k++] = input[i++]; // @부터 빈칸이 나올때까지 option 배열에 저장
        }
        option[k] = '\0';
        if (input[i] == ' ')
        {
            i++;
        }
    }

    while (input[i] != '\0')
    {
        output[j++] = input[i++]; // option 다음 문자들은 output 배열에 저장
    }

    output[j] = '\0';
}

