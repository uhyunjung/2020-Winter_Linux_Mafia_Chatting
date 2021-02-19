#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<pthread.h>
#include<time.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include "/usr/include/mysql/mysql.h"
#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS "abc!1234"
#define database "select * from mafia.info"
#define MAXLINE  511
#define MAX_SOCK 1024 // 솔라리스의 경우 64
#define BUF_SIZE 100
#define MAX_CLNT 6
#define MAX_IP 30

char* mafia_victory = "\n마피아의 승리입니다!\n";
char* citizen_victory = "\n시민의 승리입니다!\n";
char* doctor_victory = "\n<<의사가 마피아의 살인을 막았습니다!>>\n";
char* doctor_fail = "\n<<의사가 마피아의 살인을 막지 못했습니다!>>\n";
char* game_end = "\n마피아  게임이 종료되었습니다.\n";
char* vote = "------------------------------------------------------\n\t\t투표를 시작합니다.\n\t마피아라고 생각하는 사용자의 번호를 입력하세요\n";
char* order = "투표할 사용자의 닉네임을 입력해주세요\n";
char* wait_so = "wait..\n";
char* same = "동점자가 나왔으므로 다시 시작합니다\n";
char* wrong = "잘못입력하셨습니다\n";
char* EXIT_STRING = "exit";     // 클라이언트의 종료요청 문자열
char* to_night = "\n----------밤으로 넘어갑니다.---------\n";
char* msg_mafia = "\n----------당신은 마피아입니다.----------\n";
char* msg_doc = "\n----------당신은 의사입니다.----------\n\n";
char* msg_pol = "\n----------당신은 경찰입니다.----------\n\n";
char* msg_citizen = "\n----------당신은 시민입니다.----------\n\n";
char* to_dead = "\n-------당신은 죽었습니다. 관전모드로 전환합니다.\n 대화에 참여하실 수 없지만 대화를 들을 수는 있습니다.\n";
char* nightmsg = "\n누구를 죽일 것인지 결정해주세요:\n";
char* dtmsg = "\n누구를 살릴 것인지 결정해주세요:\n";
char* plcmsg = "\n누가 마피아인지 결정해주세요:\n";
char* policevictory = "\n<<경찰이 선택한 사람은 마피아가 맞습니다!>>\n";
char* policefail = "\n<<경찰이 선택한 사람은 마피아가 아닙니다.>>\n";
char* mafia_chat_wait = "\n마피아끼리의 대화 시간을 갖습니다. 모두 대기해주세요.\n";
char* retrymsg = "\n그 사람은 이미 죽었습니다, 다른 사람을 선택해주세요.";
char* startGame = "\n\n-------Start Mafia Game-------\n\n서로 자기소개를 하는 시간입니다. 채팅에 참여해주세요.\n";   // 새로운 채팅 참가자 처리
char* timeone = "------------1분남았습니다.---------------\n";

char vvote[MAXLINE + 1];
int flag[6] = { 1,1,1,1,1,1 };
int alive[6] = { 1,1,1,1,1,1 };
int job[4];
int nvote[6] = { 0,0,0,0,0,0 };
char buf[MAXLINE + 1];
int k, i, max;
char nickname[6][100];
int num_user = 0;
int clisock_list[MAX_CLNT];
int mafiachat[6] = { 1,1,1,1,1,1 };
int alive_num = 6;
typedef struct people{
    int tmp;
    int win;
    int lose;
    int rank;
    char name[100];
}people;

people p[6];

pthread_mutex_t mutx;
pthread_t t_id;
pthread_t a_thread;

void* mysqlf();
void* handle_clnt(void* arg);
void send_msg(char* msg, int len);
void error_handling(char* msg);
char* serverState(int count);
void menu(char port[]);
void jobSet();
void job_notice();
void timer(int second);
void* votefunc();
void dead_player(int num);
void start_mafia_chat();
void print_all_message(char message[]);
int Choice(int who, char emssage[], char buf[]);
void* NightChoice();
void* thread_function(void* arg);
int main(int argc, char* argv[]);
void* handle_clnt(void* Arg);
void send_msg(char* msg, int len);
void error_handling(char* msg);
char* serverState(int count);
void menu(char port[]);
int end_condition();
/****************************/


void jobSet() {//복붙
    int tmp = num_user;
    int user[6] = { 0,1,2,3,4,5 };
    srand(time(NULL));
    for (int i = 0; i < 4; i++) {
        int n = rand() % tmp;
        job[i] = user[n];
        for (int j = n; j < tmp - 1; j++)//중복안되게 정렬
            user[j] = user[j + 1];
        tmp--;
    }
}
void job_notice() {//직업 알림 복붙
    for (int i = 0; i < num_user; i++) {
        if (i == job[0] || i == job[1])
            send(clisock_list[i], msg_mafia, strlen(msg_mafia), 0);
        else if (i == job[2])
            send(clisock_list[i], msg_pol, strlen(msg_pol), 0);
        else if (i == job[3])
            send(clisock_list[i], msg_doc, strlen(msg_doc), 0);
        else
            send(clisock_list[i], msg_citizen, strlen(msg_citizen), 0);
    }
}
void timer(int second) {                 //sleep으로 대체 가능할지 보기
    int end = 0;
    int start = (int)(time(NULL));
    int i, n = 0; //while문에서 한번만 출력되기 위함
    while (1) {
        end = (int)(time(NULL));
        if (end - start == second - 60 && !n) {//1분남았습니다 출력
            print_all_message(timeone);
            n = 1;
        }
        else if (end - start == second)
            break;
    }
    for (i = 0; i < num_user; i++)
        flag[i] = 0;
}  // timer 끝
void* votefunc() {//투표
    int tryagain;
    int j = 0;
    int t = 0;
    pthread_mutex_lock(&mutx);

    while (1) {
        t++;
        tryagain = 1;
        if (t != 1) //동점자가 나왔을 때
            print_all_message(same);
        for (int i = 0; i < num_user; i++)
            nvote[i] = 0;//초기화 시킴 nvote는 투표용지같은 개념
        print_all_message(vote);

        printf("\n<투표시작>\n");
        for (int index = 0; index < 6; index++) {
            if (alive[index] == 0)continue;//죽은애 투표못해
            flag[index] = 1;
            for (int k = 0; k < num_user; k++) {
                if (k != index && alive[k] == 1) {
                    flag[k] = 0;
                    send(clisock_list[k], wait_so, strlen(wait_so), 0);//wait..출력
                }
            }
            int vote = Choice(index, order, buf);
            ++nvote[vote];
            for (i = 0; i < num_user; i++) {
                printf("%d  ", nvote[i]);//서버에게 실시간으로 보여줌
            } printf("\n");
        }//투표끝

        max = 0;//가장 많이 투표수 받은 사람
        for (int i = 0; i < num_user; i++) {
            if (max < nvote[i]) {
                max = nvote[i];
                k = i;
            }
        }
        //동점자 체크
        for (i = 0; i < num_user; i++) {
            if (max == nvote[i] && i != k) {//동점자 있을시
                t++;
                tryagain = 0;
                break;
            }
        }
        if (tryagain != 0)
            break;
    }
    pthread_mutex_unlock(&mutx);
    return (void*)k;
}
void dead_player(int player_num) {
    char dead_person[200];
    flag[player_num] = 1;//받을 수 있고
    alive[player_num] = 0;//말할 수 없음
    alive_num--;
    sprintf(dead_person, "%s 님이 사망하셨습니다.", nickname[player_num]);
    print_all_message(dead_person);
    send(clisock_list[player_num], to_dead, strlen(to_dead), 0); //너 죽었어
}
void start_mafia_chat() {//귓속말 복붙
    for (int i = 0; i < num_user; i++) {
        if (i == job[0] || i == job[1]){//마피아라면
            flag[i] = 1;//메시지를 받을 수 있음
        }
        else {
            mafiachat[i] = 0;//말할 수 없고
            flag[i] = 0;//받을 수도 없음
            if (alive[i] == 0) //죽은 사람이면
                flag[i] = 1;//받을 순 있음.
        }
    }
}
void print_all_message(char message[]) {
    for (int i = 0; i < num_user; i++)
        send(clisock_list[i], message, strlen(message), 0);
}
int Choice(int who, char message[], char buf[]) {
    int choice;
    send(clisock_list[who], message, strlen(message), 0);  // 선택하라고 메시지 줌
    while (1) {
        memset(buf, 0, strlen(buf));
        int nbyte = recv(clisock_list[who], buf, MAXLINE, 0); // 메시지 청취
        if (nbyte <= 0 || errno == EWOULDBLOCK)//예외처리
            continue;
        char* ptr = strtok(buf, " ");
        ptr = strtok(NULL, "\n");
        printf("%s\n", ptr);
        int tmp = 0;
        choice = -1;
        for (int i = 0; i < num_user; i++) {
            if (!strncmp(ptr, nickname[i], strlen(nickname[i]))) {
                tmp = 1;
                choice = i;
                break;
            }
        }
        if (!alive[choice]) {//선택한 애가 죽었을 때
            send(clisock_list[who], retrymsg, strlen(retrymsg), 0);  // 다시 선택하라고 메시지 줌
            continue;
        }
        if (!tmp) {
            send(clisock_list[who], wrong, strlen(wrong), 0);  // 다시 선택하라고 메시지 줌
            continue;
        }
        else return choice;
    } // while 중괄호 끝
}
void* NightChoice() {
    char* end_mafia_talk = "\t마피아 채팅이 끝났습니다. 마피아 중 한 명은 선택을 하게 됩니다.\n";
    char* doctor_talking = "의사가 선택을 하고 있습니다.\n";
    char* police_talking = "경찰이 선택을 하고 있습니다.\n";
    int dt_choice = -1;
    int pol_choice = -1;
    int mf_choice = -1;
    char tmp_msg[200];
    pthread_mutex_lock(&mutx);
    // 마피아 채팅 종료 알림
    for (int i = 0; i < num_user; i++)
        mafiachat[i] = 1;
    print_all_message(end_mafia_talk);

    //마피아 차례
    int tmp = job[0];
    if (alive[tmp] == 0)
        tmp = job[1];
    mf_choice = Choice(tmp, nightmsg, buf);

    //doctor
    for (int i = 0; i < num_user; i++)
        if (i != job[3])
            send(clisock_list[i], doctor_talking, strlen(doctor_talking), 0);//doctor 말하는중

    if (alive[job[3]] == 1)  // doctor 살아 있을 때
        dt_choice = Choice(job[3], dtmsg, buf);
    else timer(10);

    //경찰차례
    for (int i = 0; i < num_user; i++)
        if (i != job[2])
            send(clisock_list[i], police_talking, strlen(police_talking), 0);

    if (alive[job[2]] == 1) // police이고 살아 있을 때
        pol_choice = Choice(job[2], plcmsg, buf);
    else timer(10);

    //의사와 마피아 선택 비교
    if (mf_choice != dt_choice) {    // 마피아 지목 != 의사 지목일 경우
        dead_player(mf_choice);//마피아가 선택한 사람 죽음
        print_all_message(tmp_msg);
        print_all_message(doctor_fail);
    }
    else
        print_all_message(doctor_victory);

    //경찰선택 비교
    if (pol_choice == job[0] || pol_choice == job[1]) //마피아라면
        print_all_message(policevictory);
    else //마피아가 아니면
        print_all_message(policefail);

    pthread_mutex_unlock(&mutx);
}    // 함수 끝
int end_condition() {
    int alive_mafia = 0;
    int alive_citizen = 0;
    for (int i = 0; i < num_user; i++) {
        if (alive[i]) {
            if (i == job[0] || i == job[1]) alive_mafia++;
            else alive_citizen++;
        }
    }
    if (!alive_mafia)return 1; //시민의 승리
    else if (alive_mafia >= alive_citizen)return -1;//마피아 승리
    return 0;
}
void* mysqlf(){
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char query[255];  // insert문 임시 저장*/
    people pp[1000];
    int cnt = 0;
    people t;
        // MYSQL 연결 확인
    if(!(conn=mysql_init((MYSQL*)NULL))){
        printf("mysql init 실패\n");
        exit(1);
    }

    if(!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, "mafia", 3306, NULL ,0)){
        printf("mysql connect 에러\n");
        exit(1);
    }

    if(mysql_query(conn, database)){
        printf("query 실패\n");
        exit(1);
    }
    res = mysql_store_result(conn);
    for(int i =0; i<num_user;i++){
        while(row=mysql_fetch_row(res)){
            if(!strcmp(row[0],nickname[i])){
                if(p[i].tmp)//이겼으면
                    sprintf(query,"update info set win='%d' where name='%s';",atoi(row[1])+1,row[0]);
                else
                    sprintf(query,"update info set lose='%d' where name='%s';",atoi(row[2])+1,row[0]);
                mysql_query(conn, query);
                break;
            }
        }
    }
        
    if(mysql_query(conn, database))
    {
        printf("query 실패\n");
        exit(1);
    }
    res = mysql_store_result(conn);
    cnt = 0;
    while(row=mysql_fetch_row(res)){
        printf("%s, %s, %s, %s, %s\n", row[0], row[1], row[2] ,row[3] ,row[4]);
        pp[cnt].win = atoi(row[1]);
        pp[cnt].lose = atoi(row[2]);
        strcpy(pp[cnt].name,row[0]);
        cnt++;
    }
    for(int i=0; i<cnt; i++){
        t = pp[i];
        int j;
        for(j = i-1; j>=0; j--){
            if(t.win>pp[j].win)
                pp[j+1] = pp[j];
            else if(t.win==pp[j].win&&t.lose<pp[j].lose)
                pp[j+1] = pp[j];
            else break;
        }
        pp[j+1] = t;
    }
    for(int i =0; i<cnt; i++){
        memset(query, 0, strlen(query));
        pp[i].rank = i+1;
        sprintf(query,"update info set ranking='%d' where name='%s';",pp[i].rank,pp[i].name);
        mysql_query(conn, query);
    }
}
void* thread_function(void* arg) { //명령어를 처리할 스레드
    int i;
    int j;
    char alive_say[100];
    pthread_t vote;
    pthread_t night;
    pthread_t sql;
    char* m = "마피아끼리 채팅중입니다.\n";
    char* day_say = "\n아침이 밝았습니다. 마피아일것같은 사람을 토론을 통해 추리해주세요\n";
    char* stay = "\n---------------------------------다음게임을 준비중입니다.----------------------------------\n";
    if (num_user == 6) {    //명령어 처리
        pthread_mutex_lock(&mutx);
        jobSet();//직업 설정
        job_notice();//직업 알림
        pthread_mutex_unlock(&mutx);
        int tmp = 0;
        for (int k = 1; k < 7; k++) {
            printf("%d",tmp);
            pthread_mutex_lock(&mutx);
            if (end_condition()!=0)break;
            sprintf(alive_say, "<<%d번째 날. 현재 생존자수는 %d명입니다.>>", k, alive_num);
            print_all_message(alive_say);
            for (int i = 0; i < num_user; i++)
                flag[i] = 1;
            if (k == 1)print_all_message(startGame);
            else print_all_message(day_say);
            pthread_mutex_unlock(&mutx);

            timer(5);//2분 시간줌
            pthread_create(&vote, NULL, votefunc, (void*)NULL);
            pthread_join(vote, (void**)&j);
            dead_player(j);

            if (end_condition()!=0)break;
            print_all_message(to_night);//밤으로 넘어갑니다
            start_mafia_chat();//마피아 채팅 시작
            print_all_message(m);//마피아끼리 채팅중
            timer(5);
            pthread_create(&night, NULL, NightChoice, (void*)NULL);
            pthread_join(night, NULL);

            timer(5);
            tmp = 1;
        }
        print_all_message(game_end);
        if (end_condition() == 1) {//시민의 승리
            print_all_message(citizen_victory);
            for(int i =0; i<6; i++){
                if(i==job[0]||i==job[1])p[i].tmp = 0;
                else p[i].tmp = 1;
            }
        }else {//마피아의 승리
            print_all_message(mafia_victory);
            for(int i =0; i<6; i++){
                if(i==job[0]||i==job[1])p[i].tmp = 1;
                else p[i].tmp = 0;
            }
        }
        pthread_create(&sql, NULL, mysqlf, (void*)NULL);
        pthread_join(sql, NULL);
    }       
}

int main(int argc, char* argv[])
{   
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    /** time log **/
    struct tm* t;
    time_t timer = time(NULL);
    t = localtime(&timer);

    if (argc != 2)
    {
        printf(" Usage : %s <port>\n", argv[0]);
        exit(1);
    }
    menu(argv[1]);

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");
    while (1)
    {
        t = localtime(&timer);
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        memset(&buf, 0, sizeof(buf));
        recv(clnt_sock, buf, sizeof(buf), 0);
        strcpy(nickname[num_user], buf);
        printf("nickname %s\n", nickname[num_user]);
        pthread_mutex_lock(&mutx);
        clisock_list[num_user++] = clnt_sock;
        pthread_mutex_unlock(&mutx);
        if(num_user==6){pthread_create(&a_thread, NULL, thread_function, (void*)NULL);pthread_detach(a_thread);}
        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
        pthread_detach(t_id);
        printf(" Connected client IP : %s ", inet_ntoa(clnt_adr.sin_addr));
        printf("(%d-%d-%d %d:%d)\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min);
        printf(" chatter (%d/6)\n", num_user);
    }
    close(serv_sock);
    return 0;
}

void* handle_clnt(void* arg)
{
    int clnt_sock = *((int*)arg);//clnt_sock의 주소, clnt_sock은 각 clnt를 의미
    int str_len = 0, i;
    char msg[BUF_SIZE];

    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) {
        int i;
        for (i = 0; i < num_user; i++) {
            if (clisock_list[i] == clnt_sock)break;
        }
        if (alive[i] == 0 || mafiachat[i] == 0)continue;
        send_msg(msg, str_len);
    }
    // remove disconnected client
    pthread_mutex_lock(&mutx);
    for (i = 0; i < num_user; i++)
    {
        if (clnt_sock == clisock_list[i])
        {
            while (i++ < num_user - 1)
                clisock_list[i] = clisock_list[i + 1];
            break;
        }
    }
    num_user--;
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    return NULL;
}

void send_msg(char* msg, int len)
{
    int i;
    pthread_mutex_lock(&mutx);//나만쓸꺼야
    for (i = 0; i < num_user; i++)
        if (flag[i] == 1)
            write(clisock_list[i], msg, len);
    pthread_mutex_unlock(&mutx);
}

void error_handling(char* msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

char* serverState(int count)
{
    char* stateMsg = malloc(sizeof(char) * 20);
    strcpy(stateMsg, "None");

    if (count < 5)
        strcpy(stateMsg, "Good");
    else
        strcpy(stateMsg, "Bad");

    return stateMsg;
}

void menu(char port[])
{
    system("clear");
    printf(" **** moon/sun chat server ****\n");
    printf(" server port    : %s\n", port);
    printf(" server state   : %s\n", serverState(num_user));
    printf(" max connection : %d\n", MAX_CLNT);
    printf(" ****          Log         ****\n\n");
}