#include "server.h"

struct queue_head connection_queue;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
bool server_running = true;

void broadcast_user_list();
void remove_client(int index);
void *thread_worker(void *arg);

void handle_p2p_request(int client_sock, const char* target_name) {
    pthread_mutex_lock(&clients_mutex);
    //printf("Hello world\n");    
    // Find requester info
    ClientInfo requester = {0};
    int requester_found = 0;
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client_sock) {
            requester = clients[i];
            requester_found = 1;
            break;
        }
    }
    
    if (!requester_found) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    
    // Find target client
    int target_found = 0;
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].name, target_name) == 0) {
            target_found = 1;
            // Send invitation to target
            pthread_mutex_unlock(&clients_mutex);
            send_p2p_invitation(requester.name, requester.ip, requester.p2p_port, target_name);
            pthread_mutex_lock(&clients_mutex);
            break;
        }
    }
    
    if (!target_found) {
        char msg[MAX_MSG_LEN];
        snprintf(msg, sizeof(msg), "SERVER: User '%s' not found", target_name);
        send(client_sock, msg, strlen(msg), 0);
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

void send_p2p_invitation(const char* requester_name, const char* requester_ip, 
    int requester_port, const char* target_name) {
    char invitation[MAX_MSG_LEN + MAX_NAME_LEN * 2 + 50];
    snprintf(invitation, sizeof(invitation), 
             "P2P_INVITE:%s:%s:%d", 
             requester_name, requester_ip, requester_port);
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].name, target_name) == 0) {
            //printf("\n\n%s %s %d %s\n",requester_name, requester_ip, requester_port, target_name);
            send(clients[i].socket, invitation, strlen(invitation), 0);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}





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
    
    if (index < 0 || index >= client_count) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    close(clients[index].socket);
    
    // Shift remaining clients
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
    //fflush(stdout);
    //printf("%s   ",user_list);
    //fflush(stdout);
    
    free(user_list);
    pthread_mutex_unlock(&clients_mutex);
}

void handle_client(int client_sock, struct sockaddr_in client_addr) { 
    char buffer[MAX_MSG_LEN] = {0};
    char client_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);

    // Get username
    int bytes = recv(client_sock, buffer, MAX_MSG_LEN - 1, 0);
    if (bytes <= 0) {
        close(client_sock);
        return;
    }


    // Check if username exists
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].name, buffer) == 0) {
            send(client_sock, "ERROR: Username taken", 21, 0);
            close(client_sock);
            pthread_mutex_unlock(&clients_mutex);
            return;
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

    // Chat loop
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        bytes = recv(client_sock, buffer, MAX_MSG_LEN - 1, 0);
        //printf("%s\n",buffer);
        if (bytes <= 0) break;

        if (strcmp(buffer, "/quit") == 0){
            break;
        }
         else if (strncmp(buffer, "/connect ", 9) == 0) {
            // Handle P2P connection request
            char* target_name = buffer + 9;
            handle_p2p_request(client_sock, target_name);
            continue;
        }
       
        else if (strncmp(buffer, "P2P_PORT:", 9) == 0) {
            // Client is telling us their P2P listening port
            int p2p_port = atoi(buffer + 9);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < client_count; i++) {
                if (clients[i].socket == client_sock) {
                    printf("\n\np2p_port: %d\n", p2p_port);
                    clients[i].p2p_port = p2p_port;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        
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
    
    int found_index = -1;
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client_sock) {
            found_index = i;
            break;
        }
    }
    
    pthread_mutex_lock(&clients_mutex);
    if (found_index != -1) {
        char leave_msg[MAX_WELCOME_MSG];
        snprintf(leave_msg, sizeof(leave_msg), "SERVER: %s has left the chat", 
        clients[found_index].name);
        
        // Send leave notification
        for (int i = 0; i < client_count; i++) {
            if (i != found_index) {
                send(clients[i].socket, leave_msg, strlen(leave_msg), 0);
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        remove_client(found_index); 
    }
    send(clients[found_index].socket, "SERVER: You have been disconnected", 34, 0);
    close(client_sock);
}

void *thread_worker(void *arg) {
    while(server_running) {
        connection_queue_t *item = NULL;
        
        pthread_mutex_lock(&queue_mutex);
        while (STAILQ_EMPTY(&connection_queue) && server_running) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        
        if (!server_running) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        
        item = STAILQ_FIRST(&connection_queue);
        if (item != NULL) {
            STAILQ_REMOVE_HEAD(&connection_queue, entries);
        }
        pthread_mutex_unlock(&queue_mutex);
        
        if (item != NULL) {
            handle_client(item->client_sock, item->client_addr);
            free(item);
        }
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    pthread_t worker_threads[MAX_CLIENTS];

    // Initialize connection queue
    STAILQ_INIT(&connection_queue);

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

    if (listen(server_fd, QUEUE_SIZE) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Create worker threads
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_create(&worker_threads[i], NULL, thread_worker, NULL);
    }

    // Main accept loop
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        // Add to connection queue
        connection_queue_t *item = malloc(sizeof(connection_queue_t));
        if (item == NULL) {
            close(client_sock);
            continue;
        }
        
        item->client_sock = client_sock;
        memcpy(&item->client_addr, &client_addr, sizeof(client_addr));
        
        pthread_mutex_lock(&queue_mutex);
        STAILQ_INSERT_TAIL(&connection_queue, item, entries);
        pthread_cond_signal(&queue_cond);
        pthread_mutex_unlock(&queue_mutex);
    }

    // Cleanup
    server_running = false;
    pthread_cond_broadcast(&queue_cond);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    
    // Close all client sockets
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        close(clients[i].socket);
    }
    pthread_mutex_unlock(&clients_mutex);
    
    close(server_fd);
    return 0;
}