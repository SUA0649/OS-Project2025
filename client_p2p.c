#include "client.h"

// Add at the top after includes
typedef struct peer_list {
    int socket;
    char name[MAX_NAME_LEN];
    struct peer_list *next;
} PeerList;

PeerList *peerss = NULL;
pthread_mutex_t peers_mutex = PTHREAD_MUTEX_INITIALIZER;

// Shared variables needed for P2P client
UI ui;

// Forward declarations different from client.c
void init_ui();
void add_message(const char *msg);
int connect_to_peer(const char *ip, int port);
void* receive_messages(void *arg);
void handle_p2p_client(int client_sock, struct sockaddr_in client_addr);
void broadcast_message(const char *msg, int sender_sock);
void add_peer(int sock, const char *name);
void remove_peer(int sock);

void handle_p2p_client(int client_sock, struct sockaddr_in client_addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    char msg[MAX_MSG];
    snprintf(msg, sizeof(msg), "New peer connected from %s:%d", 
             client_ip, ntohs(client_addr.sin_port));
    add_message(msg);

    // Add peer to list
    add_peer(client_sock, "Anonymous");  // Can be enhanced with username exchange
    
    // Start a thread to handle this client
    ThreadData *data = malloc(sizeof(ThreadData));
    data->sockfd = client_sock;
    data->ui = &ui;
    
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, data);
    pthread_detach(recv_thread);
}

void broadcast_message(const char *msg, int sender_sock) {
    pthread_mutex_lock(&peers_mutex);
    PeerList *current = peerss;
    while (current != NULL) {
        if (current->socket != sender_sock) {
            send(current->socket, msg, strlen(msg), 0);
        }
        current = current->next;
    }
    pthread_mutex_unlock(&peers_mutex);
}

void add_peer(int sock, const char *name) {
    PeerList *new_peer = malloc(sizeof(PeerList));
    new_peer->socket = sock;
    strncpy(new_peer->name, name, MAX_NAME_LEN-1);
    new_peer->next = NULL;

    pthread_mutex_lock(&peers_mutex);
    if (peerss == NULL) {
        peerss = new_peer;
    } else {
        PeerList *current = peerss;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_peer;
    }
    pthread_mutex_unlock(&peers_mutex);
}

void remove_peer(int sock) {
    pthread_mutex_lock(&peers_mutex);
    PeerList *current = peerss;
    PeerList *prev = NULL;

    while (current != NULL) {
        if (current->socket == sock) {
            if (prev == NULL) {
                peerss = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }
    pthread_mutex_unlock(&peers_mutex);
}

int main(int argc, char *argv[]) {
    
    if (argc == 5 && strcmp(argv[1], "server") == 0) {
        // SERVER MODE
        init_ui();
        strcpy(username, argv[2]);
        int p2p_port = atoi(argv[3]);
        
        // Create listener socket
        int listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener < 0) {
            add_message("Failed to create socket");
            endwin();
            return 1;
        }

        // Set socket options for WSL
        int opt = 1;
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                       &opt, sizeof(opt)) < 0) {
            add_message("Socket options failed");
            endwin();
            return 1;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(p2p_port);
        addr.sin_addr.s_addr = INADDR_ANY;  // Bind to all interfaces for WSL

        if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            add_message("Bind failed - port may be in use");
            perror("bind");
            endwin();
            return 1;
        }

        if (listen(listener, 5) < 0) {
            add_message("Listen failed");
            endwin();
            return 1;
        }

        char msg[100];
        snprintf(msg, sizeof(msg), "P2P Server listening on port %d", p2p_port);
        add_message(msg);

        // Accept connections in a loop
        while (1) {
            struct sockaddr_in peer_addr;
            socklen_t addr_len = sizeof(peer_addr);
            int client_sock = accept(listener, (struct sockaddr*)&peer_addr, &addr_len);
            
            if (client_sock < 0) {
                add_message("Accept failed!");
                continue;
            }

            handle_p2p_client(client_sock, peer_addr);
        }
    }

    else if (argc >= 4 && strcmp(argv[1], "client") == 0) {
        // CLIENT MODE - Connect to existing server
        strcpy(username, argv[2]);
        char *peer_ip = argv[3];
        int peer_port = atoi(argv[4]);
        init_ui();
        char temp[MAX_MSG];
        snprintf(temp, sizeof(temp), "Connecting to Server Port: %d : Server IP: %s", peer_port, peer_ip);
        add_message(temp);
        sleep(1);
        p2p_socket = connect_to_peer(peer_ip, peer_port);
    }
    else {
        fprintf(stderr, "Usage (server): %s server\n", argv[0]);
        fprintf(stderr, "Usage (client): %s client <username> <ip> <port>\n", argv[0]);
        return 1;
    }

    // Common chat loop for both modes
    if (p2p_socket < 0) {
        add_message("Connection failed!");
        endwin();
        return 1;
    }
    add_message("Connected to peer!");
    add_message("Type your messages below. Press ` to quit.");

    // Start message receiver thread
    pthread_t recv_thread;
    ThreadData data = {p2p_socket, &ui};
    pthread_create(&recv_thread, NULL, receive_messages, &data);

    // Main input loop
    char input[MAX_MSG] = {0};
    int ch, pos = 0;
    
    while(1) {
        ch = getch();
        
        if(ch == '`') {
            break;
        }
        else if(ch == '\n' && pos > 0) {
            char formatted_msg[MAX_MSG + 20];
            snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", username, input);
            send(p2p_socket, formatted_msg, strlen(formatted_msg), 0);
            pos = 0;
            memset(input, 0, sizeof(input));
            wmove(ui.input_win, 1, 2);
            wclrtobot(ui.input_win);
            mvwprintw(ui.input_win, 1, 0, "> ");
            wrefresh(ui.input_win);
        }
        else if((ch == 127 || ch == KEY_BACKSPACE) && pos > 0) {
            input[--pos] = '\0';
            mvwprintw(ui.input_win, 1, 2+pos, " ");
            wmove(ui.input_win, 1, 2+pos);
            wrefresh(ui.input_win);
        }
        else if(ch != ERR && pos < MAX_MSG-1) {
            input[pos++] = ch;
            wprintw(ui.input_win, "%c", ch);
            wrefresh(ui.input_win);
        }
    }

    // Cleanup
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);
    close(p2p_socket);p2p_socket = -1;
    endwin();
    return 0;
}

// Implement UI functions
void init_ui() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    refresh();

    ui.chat_win = newwin(LINES-3, COLS-1, 0, 0);
    scrollok(ui.chat_win, TRUE);
    box(ui.chat_win, 0, 0);
    mvwprintw(ui.chat_win, 0, 2, " P2P Chat ");
    wrefresh(ui.chat_win);

    ui.input_win = newwin(3, COLS-1, LINES-3, 0);
    mvwprintw(ui.input_win, 1, 0, "> ");
    wrefresh(ui.input_win);

    ui.users_win = NULL;
}

void add_message(const char *msg) {
    static int line = 1;
    int maxy, maxx;
    getmaxyx(ui.chat_win, maxy, maxx);
    
    if(line >= maxy-1) {
        scroll(ui.chat_win);
        line = maxy-2;
    }
    
    // time_t now = time(NULL);
    // struct tm *t = localtime(&now);
    // char timestamp[9];
    // strftime(timestamp, sizeof(timestamp), "%H:%M:%S", t);
    
    mvwprintw(ui.chat_win, line++, 1, "%-*.*s", maxx-2, maxx-2, msg);
    wrefresh(ui.chat_win);
}

// Implement network functions
int connect_to_peer(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) return -1;

    struct sockaddr_in peer_addr;
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &peer_addr.sin_addr);

    if(connect(sockfd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void* receive_messages(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    char buffer[MAX_MSG];
    
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        int valread = recv(data->sockfd, buffer, MAX_MSG, 0);
        
        if(valread <= 0) {
            char msg[100];
            snprintf(msg, sizeof(msg), "Peer disconnected");
            add_message(msg);
            remove_peer(data->sockfd);
            close(data->sockfd);
            free(data);
            break;
        }
        
        add_message(buffer);
        broadcast_message(buffer, data->sockfd);
    }
    return NULL;
}