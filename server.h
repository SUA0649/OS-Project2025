#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/queue.h>
#include<signal.h>


int server_socket;
#define LOCK_FILE "/tmp/server.lock"
#define MAX_CLIENTS 50
#define MAX_NAME_LEN 32
#define MAX_MSG_LEN 512
#define PORT 8080
#define MAX_WELCOME_MSG (MAX_MSG_LEN + MAX_NAME_LEN + 20)
#define QUEUE_SIZE 10

void broadcast_user_list();
void remove_client(int index);
void *thread_worker(void *arg);
void add_client(const char *name, const char *ip, int port, int socket);
void remove_client(int index);
void broadcast_user_list();
void handle_client(int client_sock, struct sockaddr_in client_addr);

// SIGINT handler
void handle_sigint(int sig) {
    printf("\n🔴 Received SIGINT (Ctrl+C)\n");
    // execlp("rm","rm", "-f","/tmp/server.lock",NULL);
    unlink(LOCK_FILE);
    if (server_socket >= 0) {
                close(server_socket);
                printf("🚪 Server socket closed\n");
            }
    exit(0);
}