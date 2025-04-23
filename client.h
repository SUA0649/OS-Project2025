#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include<signal.h>
#include "Utils/cJSON.h"
#include <stdbool.h>

#define SERVER_IP "192.168.56.1"
#define SERVER_PORT 8080
#define MAX_NAME_LEN 32
#define MAX_MSG 512
#define MAX_USERS 50
#define USER_LIST_WIDTH 30
#define MAX_FILEPATH 300

typedef enum {
    CONN_SERVER,
    CONN_P2P
} ConnectionType;

typedef struct {
    char name[MAX_MSG];
    char ip[INET_ADDRSTRLEN];
    int port;
    int socket;
} PeerInfo;

typedef struct {
    WINDOW *chat_win;
    WINDOW *users_win;
    WINDOW *input_win;
} UI;

typedef struct {
    int sockfd;
    UI *ui;
} ThreadData;

UI ui;
PeerInfo peers[MAX_USERS];
int peer_count = 0;
char username[MAX_MSG] = {0};
ConnectionType current_conn_type = CONN_SERVER;
int current_socket = -1;
int p2p_socket = -1;
int p2p_listener = -1;


void init_ui();
void update_user_list();
void add_message(const char* msg);
void get_username();
int connect_to_server();
int connect_to_peer(const char *ip, int port);
void * receive_messages(void* arg);
void chat_loop();
void display_previous_chat();
void update_log(const char* msg);

char* read_file(const char *filename){
    FILE *fp = fopen(filename, "r");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *data = (char *)malloc(size + 1);
    fread(data, 1, size, fp);
    data[size] = '\0';
    fclose(fp);
    return data;
}

void write_file(const char *filename, cJSON *json) {
    char *json_str = cJSON_Print(json);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("❌ Failed to write file.\n");
        return;
    }
    fprintf(fp, "%s", json_str);
    fclose(fp);
    free(json_str);
}

void handle_sigint(int sig) {
    /*printf("\n🔴 Received SIGINT (Ctrl+C)\n");
    execlp("rm","rm", "-f","/tmp/server.lock",NULL);
    unlink(LOCK_FILE);
    if (server_socket >= 0) {
                close(server_socket);
                printf("🚪 Server socket closed\n");
            }
    exit(0);*/
}