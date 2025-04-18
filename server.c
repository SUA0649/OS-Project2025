#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>


#define MAX_CLIENTS 50
#define MAX_NAME_LEN 32
#define MAX_MSG_LEN 512
#define PORT 8080
#define MAX_WELCOME_MSG (MAX_MSG_LEN + MAX_NAME_LEN + 20)

typedef struct {
    char name[MAX_NAME_LEN];
    char ip[INET_ADDRSTRLEN];
    int port;
    int socket;
	int client_index;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_user_list();
void remove_client(int index);
void *handle_client(void *arg);

void add_client(const char *name, const char *ip, int port, int socket) {
    pthread_mutex_lock(&clients_mutex);
    
    if (client_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
	memset(&clients[client_count].name, 0, sizeof(clients[client_count].name));
	memset(&clients[client_count].ip, 0, sizeof(clients[client_count].ip));

    strncpy(clients[client_count].name, name, MAX_NAME_LEN - 1);
    strncpy(clients[client_count].ip, ip, INET_ADDRSTRLEN - 1);
    clients[client_count].port = port;
    clients[client_count].socket = socket;
    client_count++;

    pthread_mutex_unlock(&clients_mutex);
    broadcast_user_list();
}

void remove_client(int index) {
    pthread_mutex_lock(&clients_mutex);
    close(clients[index].socket);
    
    for (int i = index; i < client_count - 1; i++) {
        clients[i] = clients[i + 1];
    }
    client_count--;
    
    pthread_mutex_unlock(&clients_mutex);
    broadcast_user_list();
}

void broadcast_user_list() {
    // Calculate required buffer size
    size_t needed_size = 6; // "USERS:"
    
    pthread_mutex_lock(&clients_mutex);
    
    // First pass: calculate total size needed
    for (int i = 0; i < client_count; i++) {
        needed_size += strlen(clients[i].name) + 1; // name + :
        needed_size += strlen(clients[i].ip) + 1;   // ip + :
        needed_size += 10; // port (max 5 digits) + comma/null
    }
    
    // Allocate buffer
    char *user_list = malloc(needed_size);
    if (!user_list) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    
    strcpy(user_list, "USERS:");
    size_t pos = 6;
    
    // Second pass: build the string
    for (int i = 0; i < client_count; i++) {
        if (i > 0) {
            user_list[pos++] = ',';
        }
        
        int written = snprintf(user_list + pos, needed_size - pos, "%s:%s:%d",
                             clients[i].name,
                             clients[i].ip,
                             clients[i].port);
        if (written < 0 || (size_t)written >= needed_size - pos) {
            break; // Buffer full
        }
        pos += written;
    }
    
    // Send to all clients
    for (int i = 0; i < client_count; i++) {
        send(clients[i].socket, user_list, strlen(user_list) + 1, 0);
    }
    
    free(user_list);
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) { int client_sock = *((int *)arg);
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[MAX_MSG_LEN] = {0};
    char client_ip[INET_ADDRSTRLEN];

    // Get client IP and port
    getpeername(client_sock, (struct sockaddr *)&client_addr, &addr_len);
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

	//client port
	int client_port = ntohs(client_addr.sin_port);

    // Get username
    memset(buffer, 0, sizeof(buffer));
	
	int bytes = recv(client_sock, buffer, MAX_MSG_LEN - 1, 0);
    if (bytes < 0) {
        close(client_sock);
        return NULL;
    }

	// Check if username exists
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].name, buffer) == 0) {
            send(client_sock, "ERROR: Username taken", 21, 0);
            close(client_sock);
            pthread_mutex_unlock(&clients_mutex);
            return NULL;
        }
    }
    pthread_mutex_unlock(&clients_mutex);


    // Add client
    add_client(buffer, client_ip, client_port, client_sock);
	
    // Welcome message
    char welcome_msg[MAX_WELCOME_MSG];
    snprintf(welcome_msg, sizeof(welcome_msg), "SERVER: %s has joined the chat", buffer);
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != client_sock) {
            send(clients[i].socket, welcome_msg, strlen(welcome_msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    bool p2p = false;
    int p2p_target = -1;

    // Chat loop
    while (1) {
        memset(buffer, 0, sizeof(buffer));
    
		bytes = recv(client_sock, buffer, MAX_MSG_LEN - 1, 0);
        if (bytes <= 0) break;

        if (strcmp(buffer, "/quit") == 0) break;
        
        if (!p2p) {
            if (strncmp(buffer, "/connect ", 9) == 0) {
                char *target_name = buffer + 9;
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < client_count; i++) {
                    if (strcmp(clients[i].name, target_name) == 0) {
                        // Send connection info to requesting client
                        char conn_info[MAX_MSG_LEN];
                        snprintf(conn_info, sizeof(conn_info), "P2P:%s:%d", 
                                clients[i].ip, clients[i].port);
                        send(client_sock, conn_info, strlen(conn_info), 0);
                        
                        // Notify target client
                        char notify_msg[MAX_MSG_LEN];
                        snprintf(notify_msg, sizeof(notify_msg), 
                                "P2P_REQUEST:%s:%d", client_ip, client_port);
                        send(clients[i].socket, notify_msg, strlen(notify_msg), 0);
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
            }
			else {
				// Broadcast message with correct sender info
				pthread_mutex_lock(&clients_mutex);
				
				int sender_index = -1;
				for (int i = 0; i < client_count; i++) {
					if (clients[i].socket == client_sock) {
						sender_index = i;
						break;
					}
				}
				
				if (sender_index != -1) {
					char formatted_msg[MAX_MSG_LEN + MAX_NAME_LEN + 3];
					char sender_msg[MAX_MSG_LEN + 10];
					
					snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", 
							clients[sender_index].name, buffer);
					snprintf(sender_msg, sizeof(sender_msg), "You: %s", buffer);
					
					for (int i = 0; i < client_count; i++) {
						if (i == sender_index) {
							send(clients[i].socket, sender_msg, strlen(sender_msg), 0);
						} else {
							send(clients[i].socket, formatted_msg, strlen(formatted_msg), 0);
						}
					}
				}
				
				pthread_mutex_unlock(&clients_mutex);
			}
        }
        else {
            // In P2P mode, just forward the message
            if (p2p_target >= 0 && p2p_target < client_count) {
                send(clients[p2p_target].socket, buffer, strlen(buffer), 0);
            }
        }
    }

    // Client disconnected
    char leave_msg[MAX_WELCOME_MSG];
    snprintf(leave_msg, sizeof(leave_msg), "SERVER: %s has left the chat", buffer);
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != client_sock) {
            send(clients[i].socket, leave_msg, strlen(leave_msg), 0);
        }
    }
    
    // Find and remove client
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client_sock) {
            remove_client(i);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(client_sock);
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        int client_sock;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        if ((client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("accept");
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, &client_sock) != 0) {
            perror("pthread_create");
            close(client_sock);
        }
        pthread_detach(thread_id);
    }

    return 0;
}