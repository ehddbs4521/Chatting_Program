#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 500
#define MAX_CLNT 256
#define NAME_SIZE 20
#define MAX_ROOM_COUNT 100

#define DUPLICATED_NAME_ERROR "이름 중복"
#define DUPLICATED_ROOM_ERROR "방 번호 중복"
#define NOT_EXIST_ROOM "해당 방 번호가 비존재"


void *handle_clnt(void *arg);
void send_msg(char *msg, char *source, int len, int clnt_sock);
void error_handling(char *msg);
void send_msg_only_one(char *msg, char *source, char *target,int len);
void send_msg_all(char *msg, char *source, int clnt_sock);
void remove_first_word(const char *input, char *source, char *option, char *output);
void check_annotation_option(char *option, char * source, char *target, char *modified, int room[], int clnt_sock);
int check_duplicate_name(const char *name);
void room_message(int clnt_sock);
void make_room(char *msg, int room[], int clnt_sock);
int check_duplicate_room(char *msg, int clnt_sock);
void send_every_clear_msg(char *msg, int len);
void send_every_welcome_msg();
void send_welcome_msg(int clnt_sock);
int check_exists_room(char *msg, int room[]);
void enter_room(char *msg, int clnt_sock);
int exist_clnt_in_room(int room_clnt_socks[], int clnt_sock);
void send_msg_only_room(int clnt_sock, int room_num, char *msg);
void send_clear_msg(int clnt_sock);
void arrange_array(int data[]);

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
char clnt_name[NAME_SIZE];
char clnt_names[MAX_CLNT][NAME_SIZE];
int room_num_start = 0;
int room_num_end = 0;
int room_names[MAX_ROOM_COUNT][MAX_CLNT];
pthread_mutex_t mutx;
int room_clnt_socks[MAX_CLNT];
int room[MAX_ROOM_COUNT]={0};
int room_num = 0;

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
            write(clnt_sock, DUPLICATED_NAME_ERROR, strlen(DUPLICATED_NAME_ERROR));
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
        printf("Connected client IP: %s %d\n", inet_ntoa(clnt_adr.sin_addr),clnt_sock);
        printf("========================\n");
    }
    close(serv_sock);
    return 0;
}

int check_duplicate_room(char *msg, int clnt_sock) {

    room_num=atoi(msg);
    int start=0;

    for (int i = room_num_start; i < room_num_end; i++) {
        if(room[i] == room_num){
            return 1;
        }
    }
    room_names[room_num][0]=clnt_sock;
    

    while(1){
        if(room_clnt_socks[start]==0){
            break;
        }
        else{
            start++;
        }
    }
    room_clnt_socks[start]=clnt_sock;

    return 0;
}

int check_duplicate_name(const char *name) {
    for (int i = 0; i < clnt_cnt; i++) {
        if (!strcmp(clnt_names[i], name)) {
            return 1;
        }
    }
    return 0;
}

int check_exists_room(char *msg, int room[]){
    room_num=atoi(msg);

    for (int i = 0; i < MAX_ROOM_COUNT; i++)
    {   
        if(room[i]==room_num){
            return 1;
        }
    }
    
    return 0;
    
}

int exist_clnt_in_room(int room_clnt_socks[], int clnt_sock){
    
    for (int i = 0; i < clnt_cnt; i++)
    {   
        if(clnt_sock==room_clnt_socks[i]){
            return 1;
        }
    }
    return 0;
    
}
void make_room(char *msg, int room[], int clnt_sock) {
    room[room_num_end++] = atoi(msg);

    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg), 
             "\t대화방 목록 (입장은 방 번호를 입력해주세요.)\n"
             "방 현황 [%d / %d]\n"
             "===================================================\n",
             room_num_end, MAX_ROOM_COUNT);

    pthread_mutex_lock(&mutx);
    for (int i = 0; i < clnt_cnt; i++) {
        if (clnt_socks[i] == clnt_sock) {
            room_message(clnt_sock);
        } else {
            write(clnt_socks[i], specific_msg, strlen(specific_msg));
        }
    }
    pthread_mutex_unlock(&mutx);
}

void enter_room(char *msg, int clnt_sock){

    send_clear_msg(clnt_sock);
    room_message(clnt_sock);

    int room_num=atoi(msg);
    int start=0;

    while(room_names[room_num][start]!=0){
        start++;
    }
    
    room_names[room_num][start]=clnt_sock;

    start=0;

    while(1){
        if(room_clnt_socks[start]==clnt_sock){
            break;
        }
        start++;
        if(room_clnt_socks[start]==0){
            room_clnt_socks[start]=clnt_sock;
            break;
        }
    }

}



void exit_room(int clnt_sock){

    send_clear_msg(clnt_sock);
    send_welcome_msg(clnt_sock);

    for (int i = 0; i < MAX_CLNT; i++)
    {
        if(room_clnt_socks[i]==clnt_sock){
            room_clnt_socks[i]=0;
        }

        if(room_names[room_num][i]==clnt_sock){
            room_names[room_num][i]=0;
        }
    }
    arrange_array(room_clnt_socks);
    arrange_array(room_names[room_num]);
}

void arrange_array(int data[]){

    for (int i = 0; i < MAX_CLNT+1; i++)
    {
        if(data[i]==0){
            data[i]=data[i+1];
        }
    }
    
}

void room_message(int clnt_sock) {
    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg),
        "============새로운 방이 생성되었습니다.============\n\n"
        "방을 나가려면 @q를 누르세요.\n"
        "===================================================\n");
    write(clnt_sock,specific_msg,strlen(specific_msg));
    
}

void *handle_clnt(void *arg) {
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
        if(!strcmp(option, "@m")){
            send_every_clear_msg("\033[H\033[J",strlen("\033[H\033[J"));
            send_every_welcome_msg();
        }

        check_annotation_option(option, source, target, modified, room, clnt_sock);
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
void send_clear_msg(int clnt_sock){
    int i;
    pthread_mutex_lock(&mutx);
    write(clnt_sock, "\033[H\033[J\n", strlen("\033[H\033[J\n"));
    pthread_mutex_unlock(&mutx);
}

void send_every_clear_msg(char *msg, int len){
    int i;
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], "\033[H\033[J\n", strlen("\033[H\033[J\n"));
    pthread_mutex_unlock(&mutx);
}

void send_every_welcome_msg(){
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
        "\n================================\n\n\n"
        );
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], specific_msg, strlen(specific_msg));
    pthread_mutex_unlock(&mutx);
}

void send_welcome_msg(int clnt_sock){
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
        "\n================================\n\n\n"
        );
    pthread_mutex_lock(&mutx);
    write(clnt_sock, specific_msg, strlen(specific_msg));
    pthread_mutex_unlock(&mutx);
}

void send_msg(char *msg, char *source, int len, int clnt_sock) {
    int i;
    int flag = 1;
    char everyone_msg[BUF_SIZE];
    snprintf(everyone_msg, sizeof(everyone_msg), "[%s] %s", source, msg);

    pthread_mutex_lock(&mutx);

    flag=exist_clnt_in_room(room_clnt_socks, clnt_sock);

    for (i = 0; i < clnt_cnt; i++){

        if(!flag){
            if(!exist_clnt_in_room(room_clnt_socks, clnt_socks[i])){
                write(clnt_socks[i], everyone_msg, strlen(everyone_msg));
            }
        }else{
            send_msg_only_room(clnt_sock,room_num,everyone_msg);
            break;
        }
    }
    
    pthread_mutex_unlock(&mutx);
}

void send_msg_all(char *msg, char *source, int clnt_sock) {

    char everyone_msg[BUF_SIZE];
    snprintf(everyone_msg, sizeof(everyone_msg), "[%s] %s", source, msg);

    pthread_mutex_lock(&mutx);
        for (int i = 0; i < clnt_cnt; i++) {
            if (clnt_socks[i]!=clnt_sock) {
                write(clnt_socks[i], everyone_msg, strlen(everyone_msg));
            }
        }
    pthread_mutex_unlock(&mutx);

}

void send_msg_only_one(char *source, char *target, char *modified, int len) {
    int i;

    char specific_msg[BUF_SIZE];
    snprintf(specific_msg, sizeof(specific_msg), "%s >> %s", source, modified);

    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++) {
        if (!strcmp(clnt_names[i], target)) {
            write(clnt_socks[i], specific_msg, strlen(specific_msg));
            break;
        }
    }
    pthread_mutex_unlock(&mutx);
}

void send_msg_only_room(int clnt_sock, int room_num, char *msg){
    
    int total=0;

    for (int i = 0; i < MAX_CLNT; i++)
    {
        if(room_names[room_num][i]!=0){
            total++;
        }
    }
    

    for (int i = 0; i < total; i++)
    {   
        if(room_names[room_num][i]!=clnt_sock){
            write(room_names[room_num][i],msg,strlen(msg));
        }
        
    }
    
}

void error_handling(char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

void check_annotation_option(char *option, char * source, char *target, char *modified, int room[], int clnt_sock) {
    
    
    if(option[0] == '\0'){
        send_msg(modified, source, strlen(modified),clnt_sock);
    }
    else if(option[1] == 'm'){
        if(!check_duplicate_room(modified, clnt_sock)){
            make_room(modified, room, clnt_sock);

            return;
        }
        else{
            write(clnt_sock, DUPLICATED_ROOM_ERROR, strlen(DUPLICATED_ROOM_ERROR));
            return;
        }
    }
    else if(option[1] == 'e'){
        if(check_exists_room(modified, room)){
            enter_room(modified, clnt_sock);
            return;
        }
        else{
            write(clnt_sock, NOT_EXIST_ROOM, strlen(NOT_EXIST_ROOM));
            return;
        }
    }
    else if(!strcmp(option,"@all")){
        send_msg_all(modified,source,clnt_sock);
    }
    else if(option[1] == 'o'){
        exit_room(clnt_sock);
    }
    else{
        strcpy(target, option + 1);
        send_msg_only_one(source, target, modified, strlen(modified));
        return;
    }
}

void remove_first_word(const char *input, char *source, char *option, char *output) {
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