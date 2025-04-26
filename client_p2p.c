#include "client.h"

// Shared variables needed for P2P client
UI ui;

// Forward declarations different from client.c
void init_ui();
void add_message(const char *msg);
int connect_to_peer(const char *ip, int port);
void* receive_messages(void *arg);

int main(int argc, char *argv[]) {
    
    if (argc == 5 && strcmp(argv[1], "server") == 0) {
        // SERVER MODE
        init_ui();
        strcpy(username, argv[2]);
        int p2p_port = atoi(argv[3]);
        char *server_ip = argv[4];

        // Debug message
        char debug_msg[100];
        snprintf(debug_msg, sizeof(debug_msg), "Starting P2P server (WSL) with: IP=%s, Port=%d", 
                 server_ip, p2p_port);
        add_message(debug_msg);

        // Create listener socket
        int listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener < 0) {
            add_message("Failed to create socket");
            perror("socket");
            endwin();
            return 1;
        }

        // Set socket options for WSL
        int opt = 1;
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            add_message("Socket options failed");
            perror("setsockopt");
            close(listener);
            endwin();
            return 1;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(p2p_port);
        
        // First try binding to INADDR_ANY (0.0.0.0)
        addr.sin_addr.s_addr = INADDR_ANY;

        // Debug message
        add_message("Attempting to bind to 0.0.0.0 first...");

        if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            // If INADDR_ANY fails, try specific IP
            add_message("INADDR_ANY bind failed, trying specific IP...");
            
            if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) {
                add_message("Invalid IP address format");
                perror("inet_pton");
                close(listener);
                endwin();
                return 1;
            }

            if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                char error_msg[100];
                snprintf(error_msg, sizeof(error_msg), "Bind failed on %s:%d", server_ip, p2p_port);
                add_message(error_msg);
                perror("bind");
                close(listener);
                endwin();
                return 1;
            }
        }

        // Add successful bind message
        char bound_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, bound_ip, INET_ADDRSTRLEN);
        char success_msg[100];
        snprintf(success_msg, sizeof(success_msg), "Successfully bound to %s:%d", bound_ip, p2p_port);
        add_message(success_msg);

        if (listen(listener, 5) < 0) {
            add_message("Listen failed");
            perror("listen");
            close(listener);
            endwin();
            return 1;
        }

        // Get actual bound address
        struct sockaddr_in bound_addr;
        socklen_t bound_len = sizeof(bound_addr);
        getsockname(listener, (struct sockaddr*)&bound_addr, &bound_len);
        
        snprintf(debug_msg, sizeof(debug_msg), "WSL P2P Server listening on port %d", 
                 ntohs(bound_addr.sin_port));
        add_message(debug_msg);
        add_message("Waiting for peer connection...");

        // Accept connection
        struct sockaddr_in peer_addr;
        socklen_t addr_len = sizeof(peer_addr);
        p2p_socket = accept(listener, (struct sockaddr*)&peer_addr, &addr_len);
        close(listener);  // Close listener after accepting

        if (p2p_socket < 0) {
            add_message("Accept failed!");
            endwin();
            return 1;
        }

        // Log successful connection
        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, INET_ADDRSTRLEN);
        char msg[100];
        snprintf(msg, sizeof(msg), "Connected to peer at %s:%d", 
                 peer_ip, ntohs(peer_addr.sin_port));
        add_message(msg);
    }

    else if (argc >= 4 && strcmp(argv[1], "client") == 0) {
        // CLIENT MODE
        init_ui();
        strcpy(username, argv[2]);
        char *peer_ip = argv[3];
        int peer_port = atoi(argv[4]);

        char msg[100];
        snprintf(msg, sizeof(msg), "Attempting to connect to %s:%d", peer_ip, peer_port);
        add_message(msg);
        
        sleep(1);  // Give server time to start
        p2p_socket = connect_to_peer(peer_ip, peer_port);
        
        if (p2p_socket < 0) {
            add_message("Failed to connect to peer");
            endwin();
            return 1;
        }
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
    if(sockfd < 0) {
        add_message("Socket creation failed");
        return -1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                   &opt, sizeof(opt)) < 0) {
        add_message("Socket options failed");
        close(sockfd);
        return -1;
    }

    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &peer_addr.sin_addr) <= 0) {
        add_message("Invalid peer IP address format");
        close(sockfd);
        return -1;
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "Connecting to %s:%d...", ip, port);
    add_message(msg);

    if (connect(sockfd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        add_message("Connection failed");
        perror("connect");
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
            add_message("Peer disconnected");
            p2p_socket = -1;  // Mark as disconnected
            break;
        }
        
        // Format messages like server does
        char formatted_msg[MAX_MSG];
        if(strncmp(buffer, username, strlen(username)) == 0) {
            // Own message
            snprintf(formatted_msg, sizeof(formatted_msg), "You: %s", 
                    buffer + strlen(username) + 2);
        } else {
            // Peer message
            strncpy(formatted_msg, buffer, sizeof(formatted_msg)-1);
        }
        add_message(formatted_msg);
    }
    return NULL;
}