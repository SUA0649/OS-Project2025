#include "client.h"

// Shared variables needed for P2P client
UI ui;


// Forward declarations different from client.c
void init_ui();
void add_message(const char *msg);
int connect_to_peer(const char *ip, int port);
void* receive_messages(void *arg);

int main(int argc, char *argv[]) {
    
    if (argc == 2 && strcmp(argv[1], "server") == 0) {
        // SERVER MODE - Full listener implementation
        init_ui();
        add_message("P2P Server started. Waiting for connections...");
        
        int listener = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = INADDR_ANY,
            .sin_port = htons(P2P_PORT_START)
        };
        
        // Set socket options
        int opt = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        if (bind(listener, (struct sockaddr*)&addr, sizeof(addr))){
            add_message("Bind failed!");
            endwin();
            return 1;
        }
        
        listen(listener, 1);
        
        // Accept connection
        struct sockaddr_in peer_addr;
        socklen_t addr_len = sizeof(peer_addr);
        p2p_socket = accept(listener, (struct sockaddr*)&peer_addr, &addr_len);
        close(listener);
        
        if (p2p_socket < 0) {
            add_message("Accept failed!");
            endwin();
            return 1;
        }
        
        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, INET_ADDRSTRLEN);
        char msg[MAX_MSG];
        snprintf(msg, sizeof(msg), "Connected to %s:%d", 
                peer_ip, ntohs(peer_addr.sin_port));
        add_message(msg);
    }

    else if (argc >= 4 && strcmp(argv[1], "client") == 0) {
        // CLIENT MODE - Connect to existing server
        char *peer_ip = argv[3];
        int peer_port = atoi(argv[4]);
        
        init_ui();
        add_message("Connecting to peer...");
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
            send(p2p_socket, input, strlen(input), 0);
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
    
    char formatted_msg[MAX_MSG + 20];
    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s", timestamp, msg);
    
    mvwprintw(ui.chat_win, line++, 1, "%-*.*s", maxx-2, maxx-2, formatted_msg);
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
            add_message("Peer disconnected");
            break;
        }
        
        add_message(buffer);
    }
    return NULL;
}