#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <fcntl.h>
#include <signal.h>

#define LOCK_FILE "/tmp/server.lock"
#define MAX_CLIENTS 50
#define MAX_NAME_LEN 32
#define MAX_MSG_LEN 512
#define PORT 8080
#define MAX_WELCOME_MSG (MAX_MSG_LEN + MAX_NAME_LEN + 20)
#define QUEUE_SIZE 10

int server_fd = -1;
typedef struct {
    char name[MAX_NAME_LEN];
    char ip[INET_ADDRSTRLEN];
    int port;
    int socket;
    int client_index;
    int p2p_port;
} ClientInfo;

void handle_p2p_request(int client_sock, const char* target_name);
void send_p2p_invitation(const char* requester_name, const char* requester_ip, int requester_port, const char* target_name);

// Queue for incoming connectionstypedef struct connection_queue {
    typedef struct connection_queue {
        int client_sock;
        struct sockaddr_in client_addr;
        char client_ip[INET_ADDRSTRLEN];  // Add this line
        STAILQ_ENTRY(connection_queue) entries;
    } connection_queue_t;

// SIGINT handler
void handle_sigint(int sig) {
    printf("\n🔴 Received SIGINT (Ctrl+C)\n");
    unlink(LOCK_FILE);
    if (server_fd >= 0) {
                close(server_fd);
                printf("🚪 Server socket closed\n");
            }
    exit(0);
}

STAILQ_HEAD(queue_head, connection_queue);