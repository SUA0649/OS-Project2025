#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// HARDCODED SERVER IP - CHANGE THIS TO YOUR PUBLIC IP
#define SERVER_IP "202.47.36.20"  // Replace with your actual public IP

typedef struct {
    int socket;
    struct sockaddr_in address;
} client_t;

client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        int bytes_received = recv(client->socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;
        
        buffer[bytes_received] = '\0';
        printf("Received from %s:%d - %s\n", 
               inet_ntoa(client->address.sin_addr),
               ntohs(client->address.sin_port),
               buffer);
        
        // Broadcast to all clients
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != 0 && clients[i].socket != client->socket) {
                send(clients[i].socket, buffer, strlen(buffer), 0);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }
    
    // Client disconnected
    close(client->socket);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client->socket) {
            clients[i].socket = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    free(client);
    pthread_exit(NULL);
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // HARDCODED IP BINDING - Change SERVER_IP at top of file
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to all interfaces

    
    // Bind and listen
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on %s:%d\n", SERVER_IP, PORT);
    
    while (1) {
        client_t *client = malloc(sizeof(client_t));
        socklen_t addrlen = sizeof(client->address);
        
        if ((client->socket = accept(server_fd, (struct sockaddr *)&client->address, &addrlen)) < 0) {
            perror("accept");
            free(client);
            continue;
        }
        
        printf("New connection from %s:%d\n", 
               inet_ntoa(client->address.sin_addr),
               ntohs(client->address.sin_port));
        
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == 0) {
                clients[i] = *client;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, (void *)client);
    }
    
    return 0;
}