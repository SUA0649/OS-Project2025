#include "client.h"

char LOCAL_IP[INET_ADDRSTRLEN];
int local_port = -1;
int current_socket = -1;
int p2p_listener_port = -1;
char p2p_response = 0;


void display_previous_chat(){
    char file[MAX_FILEPATH];
    snprintf(file,MAX_FILEPATH,"Logs/%s.json",username);
    char *json_data = read_file(file);

    cJSON *root=NULL;

    if(!json_data){
        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root,"username",username);
        cJSON_AddItemToObject(root,"messages",cJSON_CreateArray()); 
        return;
    }
    else{
        root = cJSON_Parse(json_data);
        free(json_data);
        if(!root){
            fprintf(stderr,"Failed to parse chat log.");
            return ;
        }
        cJSON *messages = cJSON_GetObjectItem(root,"messages");
        if(!cJSON_IsArray(messages)){
            fprintf(stderr,"Wrong format stored in the JSON file.");
            cJSON_Delete(root);
            return ;
        }
        cJSON *msg=NULL;
        cJSON_ArrayForEach(msg,messages){
            if(cJSON_IsString){
                add_message(msg->valuestring);
            }
        }
    }
    cJSON_Delete(root);
}
    
void update_log(const char *message) {
    char file[MAX_FILEPATH];
    snprintf(file,MAX_FILEPATH,"Logs/%s.json", username);

    char *json_data = read_file(file);
    cJSON *root = NULL;

    if (json_data) {
        root = cJSON_Parse(json_data);
        free(json_data);
    }

    if (!root) {
        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "username", username);
        cJSON_AddItemToObject(root, "messages", cJSON_CreateArray());
    }

    cJSON *messages = cJSON_GetObjectItem(root, "messages");
    if (!cJSON_IsArray(messages)) {
        messages = cJSON_CreateArray();
        cJSON_ReplaceItemInObject(root, "messages", messages);
    }
    cJSON_AddItemToArray(messages, cJSON_CreateString(message));

    // Save back to file
    write_file(file, root);
    cJSON_Delete(root);
}


int find_available_p2p_port() {
    for (int port = P2P_PORT_START; port <= P2P_PORT_END; port++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
    
        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr))) {
            close(sock);
            continue;
        }
        local_port = port;

        close(sock);
        return port;
    }
    return -1;
}

void start_p2p_listener() {
    p2p_listener_port = find_available_p2p_port();
    if (p2p_listener_port == -1) {
        add_message("Failed to find available P2P port");
        return;
    }
    
    // Tell server our P2P port
    char port_msg[32];
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (fd < 0) {
        endwin();
        fprintf(stderr, "Socket creation failed\n");
        return;
    }
    
    
    snprintf(port_msg, sizeof(port_msg), "P2P_PORT:%d", p2p_listener_port);
    send_to_current(port_msg);
    
}

void launch_p2p_server() {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - launch in pure server mode
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", local_port);
        execlp("x-terminal-emulator", "x-terminal-emulator",
            "-bg", "black", "-fg", "white", "-e",  
              "./client_p2p.exe", "server", username, port_str, LOCAL_IP, NULL);
        exit(1);
    }
}

void launch_p2p_client(const char *peer_ip, int peer_port) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - launch in pure client mode
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", peer_port);   
        execlp("x-terminal-emulator", "x-terminal-emulator"
              ,"-bg", "black", "-fg", "white", "-e", "./client_p2p.exe", "client", username, peer_ip, port_str, NULL);
        exit(1);
    }
}

void init_ui() {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        refresh();
    
        ui.chat_win = newwin(LINES-3, COLS-USER_LIST_WIDTH-1, 0, 0);
        scrollok(ui.chat_win, TRUE);
        box(ui.chat_win, 0, 0);
        mvwprintw(ui.chat_win, 0, 2, " Chat ");
        ui.chat_line = 1;  // Initialize at line 1 (inside the box)
        wrefresh(ui.chat_win);
    
        ui.users_win = newwin(LINES-3, USER_LIST_WIDTH, 0, COLS-USER_LIST_WIDTH);
        box(ui.users_win, 0, 0);
        mvwprintw(ui.users_win, 0, 2, " Online Users ");
        wrefresh(ui.users_win);
    
        ui.input_win = newwin(3, COLS-USER_LIST_WIDTH-1, LINES-3, 0);
        mvwprintw(ui.input_win, 1, 0, "> ");
        wrefresh(ui.input_win);
    
}

void update_user_list() {
    // Debug print
    // Clear and redraw the window properly
    werase(ui.users_win);
    box(ui.users_win, 0, 0);
    mvwprintw(ui.users_win, 0, 2, " Online Users ");
    
    // Calculate available space (leave 2 lines for border + header)
    int max_rows = LINES - 5;
    int rows_available = max_rows > 0 ? max_rows : 1;
    
    // Print all visible users
    int users_to_show = peer_count < rows_available ? peer_count : rows_available;
    for(int i = 0; i < users_to_show; i++) {
        mvwprintw(ui.users_win, i+1, 1, "%-28.28s", peers[i].name);
    }
    
    // Show "+X more" if needed
    if(peer_count > rows_available) {
        mvwprintw(ui.users_win, rows_available+1, 1, "+%d more", peer_count-rows_available);
    }
    
    // Force refresh
    touchwin(ui.users_win);
    wrefresh(ui.users_win);
    
    // Debug: print current window content
}

void add_message(const char *msg) {
    int maxy, maxx;
    getmaxyx(ui.chat_win, maxy, maxx);
    int available_height = maxy - 2;
    int available_width = maxx - 2;
    
    if (available_height <= 0) return;

    char buffer[MAX_MSG];
    strncpy(buffer, msg, MAX_MSG-1);
    buffer[MAX_MSG-1] = '\0';

    char *line = buffer;
    while (*line) {
        // Handle scrolling if needed
        if (ui.chat_line >= available_height) {
            for (int i = 1; i < available_height; i++) {
                char line_content[available_width + 1];
                mvwinstr(ui.chat_win, i + 1, 1, line_content);
                mvwprintw(ui.chat_win, i, 1, "%-*s", available_width, line_content);
            }
            wmove(ui.chat_win, available_height - 1, 1);
            wclrtoeol(ui.chat_win);
            ui.chat_line = available_height - 1;
        }

        // Word wrapping logic
        int remaining = available_width;
        if (strlen(line) > remaining) {
            // Find last space within width limit
            int cut = remaining;
            while (cut > 0 && line[cut] != ' ' && line[cut] != '\0') {
                cut--;
            }
            if (cut == 0) cut = remaining; // No spaces found
            
            char saved = line[cut];
            line[cut] = '\0';
            mvwprintw(ui.chat_win, ui.chat_line, 1, "%s", line);
            line[cut] = saved;
            line += cut + (saved == ' ' ? 1 : 0);
            ui.chat_line++;
        } else {
            mvwprintw(ui.chat_win, ui.chat_line, 1, "%s", line);
            ui.chat_line++;
            break;
        }
    }

    box(ui.chat_win, 0, 0);
    mvwprintw(ui.chat_win, 0, 2, " Chat ");
    wrefresh(ui.chat_win);
}

void get_username() {
    initscr();
    WINDOW *uname_win = newwin(5, 40, LINES/2-3, COLS/2-20);
    box(uname_win, 0, 0);
    mvwprintw(uname_win, 1, 1, "Enter your username: ");
    wrefresh(uname_win);
    
    echo();
    mvwgetnstr(uname_win, 2, 1, username, MAX_MSG-1);
    noecho();
    
    delwin(uname_win);
    clear();
    refresh();

    if(strlen(username) == 0) {
        strcpy(username, "Anonymous");
    }
}

int connect_to_server() {
    int sockfd;
    struct sockaddr_in serv_addr;
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    
    if(inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        close(sockfd);
        return -1;
    }
    
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    // Send username and local IP to server in format "username:local_ip"
    char init_msg[MAX_MSG];
    snprintf(init_msg, sizeof(init_msg), "%s:%s", username, LOCAL_IP);
    if(send(sockfd, init_msg, strlen(init_msg), 0) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}


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
    
    display_previous_chat();

    while(1) {
        memset(buffer, 0, sizeof(buffer));
        int valread = recv(data->sockfd, buffer, MAX_MSG, 0);
        
        if(valread <= 0) {
            add_message("Disconnected from server");
            current_socket = -1;
            peer_count = 0;
            update_user_list();
            pthread_exit(NULL);
            break;
        }

        // Handle user list updates
        else if (strncmp(buffer, "USERS:", 6) == 0) {
            peer_count = 0;
            char list_copy[MAX_MSG];
            strncpy(list_copy, buffer + 6, MAX_MSG - 1);
            list_copy[MAX_MSG - 1] = '\0';

            char *token = strtok(list_copy, ",");
            while (token && peer_count < MAX_USERS) {
                char name[MAX_NAME_LEN];
                char ip[INET_ADDRSTRLEN];
                int port;
        
                if (sscanf(token, "%[^:]:%[^:]:%d", name, ip, &port) == 3) {
                    strncpy(peers[peer_count].name, name, MAX_NAME_LEN - 1);
                    strncpy(peers[peer_count].ip, ip, INET_ADDRSTRLEN - 1);
                    peers[peer_count].port = port;
                    peers[peer_count].socket = -1;
                    peer_count++;
                }
                token = strtok(NULL, ",");
            }
            update_user_list();
            continue;
        }
        // Handle P2P invitations
        else if (strncmp(buffer, "P2P_INVITE:", 11) == 0) {
            char *inviter_name = strtok(buffer+11, ":");
            char *inviter_ip = strtok(NULL, ":");
            char *inviter_port_str = strtok(NULL, ":");
            
            if (inviter_name && inviter_ip && inviter_port_str) {
                int inviter_port = atoi(inviter_port_str);
                
                pthread_mutex_lock(&input_mutex);
                input_mode = INPUT_P2P_PROMPT;
                add_message("P2P invitation received. Accept? (y/n): ");
                wrefresh(ui.chat_win);
                
                while (input_mode == INPUT_P2P_PROMPT) {
                    pthread_cond_wait(&input_cond, &input_mutex);
                }
                
                if (p2p_response == 'y' || p2p_response == 'Y') {
                    add_message("Launching Client");
                    launch_p2p_client(inviter_ip, inviter_port);
                }
                pthread_mutex_unlock(&input_mutex);
            }
            continue;
        }
        // Handle P2P connection requests
        else if (strncmp(buffer, "P2P_CONNECT:", 12) == 0) {
            char *ip = strtok(buffer+12, ":");
            char *port_str = strtok(NULL, ":");
            
            if(ip && port_str) {
                int port = atoi(port_str);
                add_message("launching p2p server");
                launch_p2p_server();  // Become server first
                sleep(1);  // Give server time to start
                current_socket = connect_to_peer(ip, port);  // Then connect as client
                
                if(current_socket >= 0) {
                    add_message("P2P connection established!");
                    current_conn_type = CONN_P2P;
                } else {
                    add_message("P2P connection failed");
                }
            }
            continue;
        }
        else if(strncmp(buffer,"charUSERS",9) != 0){
                // Handle regular messages
                update_log(buffer);
                add_message(buffer);
    
        }

    }
    return NULL;
}

void send_to_current(const char *msg) {
    if(current_socket != -1) {
        send(current_socket, msg, strlen(msg), 0);
    }
}

void chat_loop() {
    char input[MAX_MSG] = {0};
    int ch, pos = 0;
    pthread_t recv_thread;
    
    ThreadData data = {current_socket, &ui};
    if(current_conn_type == CONN_SERVER) {
        pthread_create(&recv_thread, NULL, receive_messages, &data);
    }
    
    start_p2p_listener();

    while(1) {
        ch = getch();
        
        pthread_mutex_lock(&input_mutex);
        if (input_mode == INPUT_P2P_PROMPT) {
            // Only accept 'y' or 'n' in this mode
            if (ch == 'y' || ch == 'Y' || ch == 'n' || ch == 'N') {
                p2p_response = ch;
                input_mode = INPUT_NORMAL;
                pthread_cond_signal(&input_cond);
            }
            pthread_mutex_unlock(&input_mutex);
            continue;
        }
        pthread_mutex_unlock(&input_mutex);
        
        // Ignore special keys (keypad keys) except backspace and enter
        if (ch >= KEY_MIN) {  // KEY_MIN is the minimum value for special keys
            if (ch != KEY_BACKSPACE && ch != '\n') {
                continue;  // Skip all other special keys
            }
        }
        
        if (ch == '`') {
            if (current_conn_type == CONN_SERVER) {
                send_to_current("/quit");
                break;
            }
            
            if (current_socket != -1) {
                close(current_socket);
                current_socket = -1;
            }
            break;
        }
        else if (ch == '\n' && pos > 0) {
            if (strncmp(input, "/connect ", 9) == 0) {                
                launch_p2p_server();
                send_to_current(input);
            }
            else {
                send_to_current(input);
            }
            
            pos = 0;
            memset(input, 0, sizeof(input));
            wmove(ui.input_win, 1, 2);
            wclrtobot(ui.input_win);
            mvwprintw(ui.input_win, 1, 0, "> ");
            wrefresh(ui.input_win);
        }
        else if ((ch == 127 || ch == KEY_BACKSPACE) && pos > 0) {
            input[--pos] = '\0';
            mvwprintw(ui.input_win, 1, 2+pos, " ");
            wmove(ui.input_win, 1, 2+pos);
            wrefresh(ui.input_win);
        }
        else if (isprint(ch) && pos < MAX_MSG-1) {  // Only accept printable characters
            input[pos++] = ch;
            wprintw(ui.input_win, "%c", ch);
            wrefresh(ui.input_win);
        }
    }
    
}

void handle_sigint(int sig) {
    
    if (current_socket != -1) {
        send_to_current("/quit");
    }
    endwin();
    exit(0);
}

int main() {
    signal(SIGINT,handle_sigint);
    get_username();
    // Get valid port
    init_ui();

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8"); // Google DNS
    serv.sin_port = htons(53); // DNS port
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0 || connect(fd, (struct sockaddr *)&serv, sizeof(serv))) {
        close(fd);
        endwin();
        fprintf(stderr, "Connection failed\n");
        return 1;
    }
    
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    getsockname(fd, (struct sockaddr *)&name, &namelen);
    inet_ntop(AF_INET, &name.sin_addr, LOCAL_IP, INET_ADDRSTRLEN);
    close(fd);


    current_socket = connect_to_server();
    if(current_socket < 0) {
        endwin();
        fprintf(stderr, "Connection failed\n");
        return 1;
    }

    chat_loop();

    if(p2p_socket != -1) close(p2p_socket);
    if(current_socket != -1) close(current_socket);
    endwin();
    return 0;
}