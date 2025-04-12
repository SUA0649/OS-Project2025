
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define THREAD_POOL_SIZE 30
#define MAX_QUEUE 30

//For socketing
int server_fd;
struct sockaddr_in server_addr;

typedef struct Client
{
    int socket_fd;
    char username[32];
    struct sockaddr_in client_addr;
} Client;

Client active_clients[MAX_CLIENTS];
int num_clients = 0;

//For threading
pthread_t worker_thread[THREAD_POOL_SIZE];
pthread_mutex_t clients_mutex;
pthread_cond_t new_task_cond;

typedef struct ChatMessage
{
    char message[BUFFER_SIZE];
    char send[32];
    char target[32];
} ChatMessage;

ChatMessage message_queue[MAX_QUEUE];
int queue_front = 0, queue_rear = 0;
pthread_mutex_t queue_mutex;
pthread_cond_t queue_not_empty;

void add_client(int socket_fd, const char *username)
{
    pthread_mutex_lock(&clients_mutex);
    if (num_clients < MAX_CLIENTS)
    {
        active_clients[num_clients].socket_fd = socket_fd;
        strcpy(active_clients[num_clients].username, username);
        num_clients++;
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int socket_fd)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < num_clients; i++)
    {
        if (active_clients[i].socket_fd == socket_fd)
        {

            memmove(&active_clients[i], &active_clients[i + 1],
                    (num_clients - i - 1) * sizeof(Client));
            num_clients--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void enqueue_message(ChatMessage msg)
{
    pthread_mutex_lock(&queue_mutex);
    message_queue[queue_rear] = msg;
    queue_rear = (queue_rear + 1) % MAX_QUEUE;
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
}

ChatMessage dequeue_message()
{
    pthread_mutex_lock(&queue_mutex);
    while (queue_front == queue_rear)
    {
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    }
    ChatMessage msg = message_queue[queue_front];
    queue_front = (queue_front + 1) % MAX_QUEUE;
    pthread_mutex_unlock(&queue_mutex);
    return msg;
}

void *worker_thread(void *arg)
{
    while (1)
    {
        ChatMessage msg = dequeue_message();

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < num_clients; i++)
        {

            if (strcmp(msg.target, "ALL") == 0 ||
                strcmp(active_clients[i].username, msg.target) == 0)
            {
                send(active_clients[i].socket_fd, msg.message, strlen(msg.message), 0);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }
    return NULL;
}

int main()
{

    pthread_mutex_init(&clients_mutex, NULL);
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&queue_not_empty, NULL);

    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        pthread_create(&worker_threads[i], NULL, worker_thread, NULL);
    }
}