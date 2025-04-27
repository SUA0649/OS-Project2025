#include "client.h"
#define CUSTOM_COLOR_ID 10
#define CUSTOM_PAIR_ID 20
#define USER_LIST_WIDTH 30


// Shared variables needed for P2P client
UI ui;
char peer_username[MAX_MSG] = {0};

// Forward declarations different from client.c
void init_ui();
void add_message(const char *msg);
int connect_to_peer(const char *ip, int port);
void* receive_messages(void *arg);
void display_previous_chat(const char *target_user);
void update_log(const char *target_user, const char *message);

int main(int argc, char *argv[]) {
    
    if (argc == 5 && strcmp(argv[1], "server") == 0) {
        // SERVER MODE - Full listener implementation
        init_ui();
        strcpy(username, argv[2]);
        int p2p_port = atoi(argv[3]);
        char temp[MAX_MSG];
        snprintf(temp, sizeof(temp), "P2P Server Port: %s : Server IP: %s", argv[3], argv[4]);
        add_message(temp);
        add_message("P2P Server started. Waiting for connections...");
        
        int listener = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = INADDR_ANY,
            .sin_port = htons(p2p_port)
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
    send(p2p_socket,username,strlen(username),0);
    while(1) {
        ch = getch();
        
        if(ch == '`') {
            break;
        }
        else if(ch == '\n' && pos > 0) {
            char formatted_msg[MAX_MSG + 20];
            snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", username, input);
            add_message(formatted_msg);
            update_log(peer_username,formatted_msg);
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
    keypad(stdscr, false);
    start_color();
    
    if (can_change_color()) {
        // RGB values for your desired background (203, 166, 247)
        // ncurses uses 0-1000 scale for colors
        init_color(CUSTOM_COLOR_ID, 800, 650, 970); // Converted from 255-scale
        init_pair(CUSTOM_PAIR_ID, COLOR_WHITE, CUSTOM_COLOR_ID);
        bkgd(COLOR_PAIR(CUSTOM_PAIR_ID));
    } else {
        // Fallback to black background if custom colors not supported
        init_pair(CUSTOM_PAIR_ID, COLOR_WHITE, COLOR_BLACK);
        bkgd(COLOR_PAIR(CUSTOM_PAIR_ID));
    }

    clear();
    refresh();

    ui.chat_win = newwin(LINES-3, COLS-1, 0, 0);
    scrollok(ui.chat_win, TRUE);

    box(ui.chat_win, 0, 0);
    mvwprintw(ui.chat_win, 0, 2, " P2P Chat ");
    
    wbkgd(ui.chat_win, COLOR_PAIR(CUSTOM_PAIR_ID));
    wbkgd(ui.input_win, COLOR_PAIR(CUSTOM_PAIR_ID));

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


void display_previous_chat(const char *target_user) {
    char file[MAX_FILEPATH];
    snprintf(file, MAX_FILEPATH, "Logs/%s.json", username);

    char *json_data = read_file(file);
    if (!json_data) {
        printf("No previous chat found.\n");
        return;
    }

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root) {
        fprintf(stderr, "Failed to parse chat log.\n");
        return;
    }

    cJSON *messages = cJSON_GetObjectItem(root, "messages");
    if (!messages || !cJSON_IsObject(messages)) {
        printf("No messages found.\n");
        cJSON_Delete(root);
        return;
    }

    cJSON *chat_array = cJSON_GetObjectItem(messages, target_user);
    if (!chat_array || !cJSON_IsArray(chat_array)) {
        printf("No previous messages with %s.\n", target_user);
        cJSON_Delete(root);
        return;
    }

    cJSON *msg = NULL;
    cJSON_ArrayForEach(msg, chat_array) {
        if (cJSON_IsString(msg)) {
            add_message(msg->valuestring);
        }
    }

    cJSON_Delete(root);
}

    
void update_log(const char *target_user, const char *message) {
    char file[MAX_FILEPATH];
    snprintf(file, MAX_FILEPATH, "Logs/%s.json", username);

    char *json_data = read_file(file);
    cJSON *root = NULL;

    if (json_data) {
        root = cJSON_Parse(json_data);
        free(json_data);
    }

    if (!root) {
        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "username", username);
        cJSON_AddItemToObject(root, "messages", cJSON_CreateObject());
    }

    cJSON *messages = cJSON_GetObjectItem(root, "messages");
    if (!messages || !cJSON_IsObject(messages)) {
        messages = cJSON_CreateObject();
        cJSON_ReplaceItemInObject(root, "messages", messages);
    }

    // target_user could be "global" or "Faizan" or "Anne" etc.
    cJSON *chat_array = cJSON_GetObjectItem(messages, target_user);
    if (!chat_array || !cJSON_IsArray(chat_array)) {
        chat_array = cJSON_CreateArray();
        cJSON_AddItemToObject(messages, target_user, chat_array);
    }

    cJSON_AddItemToArray(chat_array, cJSON_CreateString(message));

    write_file(file, root);
    cJSON_Delete(root);
}


void* receive_messages(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    char buffer[MAX_MSG];
    
    int count=0;
    memset(buffer, 0, sizeof(buffer));
    int valread = recv(data->sockfd, buffer, MAX_MSG, 0);
    strcpy(peer_username,buffer);
    display_previous_chat(buffer);
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        valread = recv(data->sockfd, buffer, MAX_MSG, 0);
        
        if(valread <= 0) {
            add_message("Peer disconnected");
            break;
        }
        // char *buf_cpy[50];
        // strcpy(buf_cpy,buffer);
        add_message(buffer);
        update_log(peer_username,buffer);
    }
    return NULL;
}