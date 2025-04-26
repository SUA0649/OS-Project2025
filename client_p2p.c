#include "client.h"
#include <ifaddrs.h>  // Add this for getifaddrs and struct ifaddrs

// Shared variables needed for P2P client
UI ui;
char current_username[MAX_MSG] = {0};

// Forward declarations different from client.c
void init_ui();
void add_message(const char *msg);
int connect_to_peer(const char *ip, int port);
void* receive_messages(void *arg);

int main(int argc, char *argv[]) {
    
    if (argc == 3 && strcmp(argv[1], "server") == 0) {
        // SERVER MODE - Full listener implementation
        strcpy(current_username, argv[2]);
        init_ui();
        add_message("P2P Server started. Waiting for connections...");
        
        // Get local IP address
        char local_ip[INET_ADDRSTRLEN] = "192.168.1.12";  // Default to localhost
        struct ifaddrs *ifaddr, *ifa;
        if (getifaddrs(&ifaddr) == -1) {
            perror("getifaddrs");
            add_message("Warning: Could not get local IP, using localhost");
        } else {
            // Find the first non-loopback IPv4 address
            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL) continue;
                if (ifa->ifa_addr->sa_family == AF_INET) {
                    struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                    if (strcmp(inet_ntoa(sa->sin_addr), "192.168.1.12") != 0) {
                        strcpy(local_ip, inet_ntoa(sa->sin_addr));
                        break;
                    }
                }
            }
            freeifaddrs(ifaddr);
        }

        char debug_msg[MAX_MSG];
        snprintf(debug_msg, sizeof(debug_msg), "Server IP: %s, Port: %d", local_ip, P2P_PORT_START);
        add_message(debug_msg);

        int listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener < 0) {
            add_message("Socket creation failed!");
            endwin();
            return 1;
        }
        
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = INADDR_ANY,
            .sin_port = htons(P2P_PORT_START)
        };
        
        // Set socket options
        int opt = 1;
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            add_message("Setsockopt failed!");
            close(listener);
            endwin();
            return 1;
        }
        
        if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            snprintf(debug_msg, sizeof(debug_msg), "Bind failed! Error: %s", strerror(errno));
            add_message(debug_msg);
            close(listener);
            endwin();
            return 1;
        }
        
        if (listen(listener, 5) < 0) {
            add_message("Listen failed!");
            close(listener);
            endwin();
            return 1;
        }
        
        add_message("Server is listening for connections...");
        
        // Accept connection
        struct sockaddr_in peer_addr;
        socklen_t addr_len = sizeof(peer_addr);
        p2p_socket = accept(listener, (struct sockaddr*)&peer_addr, &addr_len);
        
        if (p2p_socket < 0) {
            snprintf(debug_msg, sizeof(debug_msg), "Accept failed! Error: %s", strerror(errno));
            add_message(debug_msg);
            close(listener);
            endwin();
            return 1;
        }
        
        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, INET_ADDRSTRLEN);
        
        // Send local IP to the connecting peer
        char ip_msg[MAX_MSG];
        snprintf(ip_msg, sizeof(ip_msg), "SERVER_IP:%s", local_ip);
        if (send(p2p_socket, ip_msg, strlen(ip_msg), 0) < 0) {
            add_message("Failed to send server IP!");
        }
        
        snprintf(debug_msg, sizeof(debug_msg), "Connected to peer %s:%d", 
                peer_ip, ntohs(peer_addr.sin_port));
        add_message(debug_msg);
        
        close(listener);  // Close listener after accepting one connection
    }

    else if (argc >= 4 && strcmp(argv[1], "client") == 0) {
        // CLIENT MODE - Connect to existing server
        strcpy(current_username, argv[2]);
        char *peer_ip = argv[3];
        int peer_port = atoi(argv[4]);
        
        init_ui();
        char debug_msg[MAX_MSG];
        snprintf(debug_msg, sizeof(debug_msg), "Attempting to connect to %s:%d", peer_ip, peer_port);
        add_message(debug_msg);
        
        p2p_socket = connect_to_peer(peer_ip, peer_port);
        
        if (p2p_socket >= 0) {
            add_message("Connection established, waiting for server IP...");
            
            // Wait for server's IP message
            char buffer[MAX_MSG] = {0};
            int bytes = recv(p2p_socket, buffer, MAX_MSG - 1, 0);
            if (bytes > 0) {
                if (strncmp(buffer, "SERVER_IP:", 10) == 0) {
                    char server_ip[INET_ADDRSTRLEN];
                    strncpy(server_ip, buffer + 10, INET_ADDRSTRLEN - 1);
                    snprintf(debug_msg, sizeof(debug_msg), "Connected to server at %s", server_ip);
                    add_message(debug_msg);
                } else {
                    snprintf(debug_msg, sizeof(debug_msg), "Received unexpected message: %s", buffer);
                    add_message(debug_msg);
                }
            } else if (bytes == 0) {
                add_message("Server closed the connection");
                close(p2p_socket);
                p2p_socket = -1;
            } else {
                snprintf(debug_msg, sizeof(debug_msg), "Error receiving server IP: %s", strerror(errno));
                add_message(debug_msg);
                close(p2p_socket);
                p2p_socket = -1;
            }
        } else {
            snprintf(debug_msg, sizeof(debug_msg), "Connection failed: %s", strerror(errno));
            add_message(debug_msg);
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
            char formatted_msg[MAX_MSG + MAX_NAME_LEN + 2];
            snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", current_username, input);
            send(p2p_socket, formatted_msg, strlen(formatted_msg), 0);
            add_message(formatted_msg);
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
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[9];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", t);
    
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
        int bytes = recv(data->sockfd, buffer, MAX_MSG - 1, 0);
        
        if(bytes <= 0) {
            add_message("Connection closed");
            break;
        }
        
        // Handle server IP message
        if (strncmp(buffer, "SERVER_IP:", 10) == 0) {
            char server_ip[INET_ADDRSTRLEN];
            strncpy(server_ip, buffer + 10, INET_ADDRSTRLEN - 1);
            char msg[MAX_MSG];
            snprintf(msg, sizeof(msg), "Connected to server at %s", server_ip);
            add_message(msg);
            continue;
        }
        
        // Handle regular messages
        char formatted_msg[MAX_MSG + MAX_NAME_LEN + 2];
        snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", 
                current_username, buffer);
        add_message(formatted_msg);
    }
    
    return NULL;
}