#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "Utils/cJSON.h"
#include <stdbool.h>
#include <ctype.h>

#define MAX_FILEPATH 300
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_NAME_LEN 32
#define MAX_MSG 512
#define MAX_USERS 50
#define USER_LIST_WIDTH 30
#define P2P_PORT_START 9000
#define P2P_PORT_END 9100


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

typedef enum {
    INPUT_NORMAL,
    INPUT_P2P_PROMPT
} InputMode;

volatile InputMode input_mode = INPUT_NORMAL;
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t input_cond = PTHREAD_COND_INITIALIZER;

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
    int chat_line;
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
int p2p_socket = -1;


void launch_p2p_chat(const char *peer_ip, int peer_port);
int find_available_p2p_port();
void add_message(const char *msg);
void send_to_current(const char *msg);
void launch_p2p_server();  // For the peer that accepts connection
void launch_p2p_client(const char *peer_ip, int peer_port);  // For the peer that initiates