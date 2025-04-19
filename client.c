#include "client.h"

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
    static int line = 1;
    int maxy, maxx;
    getmaxyx(ui.chat_win, maxy, maxx);
    
    if(line >= maxy-1) {
        scroll(ui.chat_win);
        line = maxy-2;
    }
    
    mvwprintw(ui.chat_win, line++, 1, "%s", msg);
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
    
    // Send username to server
    if(send(sockfd, username, strlen(username), 0) < 0) {
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

		if (strncmp(buffer, "USERS:", 6) == 0) {
			// Debug: print the raw user list
            printf("\t %s    ",buffer);
			// Clear existing peers
			peer_count = 0;
		
			// Make a copy for safe tokenization
			char list_copy[MAX_MSG];
			strncpy(list_copy, buffer + 6, MAX_MSG - 1);
			list_copy[MAX_MSG - 1] = '\0';
            printf("%s  ",buffer);

			// Tokenize and parse each user entry: "name:ip:port"
			char *token = strtok(list_copy, ",");
			while (token && peer_count < MAX_USERS) {
				char name[MAX_NAME_LEN];
				char ip[INET_ADDRSTRLEN];
				int port;
		
				if (sscanf(token, "%[^:]:%[^:]:%d", name, ip, &port) == 3) {
					strncpy(peers[peer_count].name, name, MAX_NAME_LEN - 1);
					peers[peer_count].name[MAX_NAME_LEN - 1] = '\0';
		
					strncpy(peers[peer_count].ip, ip, INET_ADDRSTRLEN - 1);
					peers[peer_count].ip[INET_ADDRSTRLEN - 1] = '\0';
		
					peers[peer_count].port = port;
					peers[peer_count].socket = -1;
		
					peer_count++;
				}
		
				token = strtok(NULL, ",");
			}
		
			// Refresh the user interface
			update_user_list();
		}
        else if(strncmp(buffer, "P2P:", 4) == 0) {
            // Server is giving us P2P connection info
            char *ip = strtok(buffer+4, ":");
            char *port_str = strtok(NULL, ":");
            
            if(ip && port_str) {
                int port = atoi(port_str);
                p2p_socket = connect_to_peer(ip, port);
                if(p2p_socket >= 0) {
                    current_conn_type = CONN_P2P;
                    current_socket = p2p_socket;
                    add_message("P2P connection established!");
                } else {
                    add_message("P2P connection failed");
                }
            }
        }
        else if(strncmp(buffer, "P2P_REQUEST:", 12) == 0) {
            // Another client wants to connect to us
            char *ip = strtok(buffer+12, ":");
            char *port_str = strtok(NULL, ":");
            
            if(ip && port_str) {
                char msg[MAX_MSG];
                snprintf(msg, sizeof(msg), "P2P connection request from %s. Accept? (y/n): ", ip);
                add_message(msg);
                
                int ch = getch();
                if(ch == 'y' || ch == 'Y') {
                    int port = atoi(port_str);
                    p2p_socket = connect_to_peer(ip, port);
                    if(p2p_socket >= 0) {
                        current_conn_type = CONN_P2P;
                        current_socket = p2p_socket;
                        add_message("P2P connection established!");
                    } else {
                        add_message("P2P connection failed");
                    }
                }
            }
        }
        else {
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
    
    while(1) {
        ch = getch();
        
        if(ch == '`') {
            if(current_conn_type == CONN_SERVER) {
                send_to_current("/quit");
                break;
            }
            
        // Close all sockets
        if(p2p_socket != -1) {
            close(p2p_socket);
            p2p_socket = -1;
        }
        if(current_socket != -1) {
            close(current_socket);
            current_socket = -1;
        }
            break;
        }
        else if(ch == '\n' && pos > 0) {
            if(strncmp(input, "/connect ", 9) == 0) {
                char *target = input + 9;
                if(strcasecmp(target, "server") == 0 && current_conn_type == CONN_P2P) {
                    // Switch back to server connection
                    close(p2p_socket);
                    p2p_socket = -1;
                    current_conn_type = CONN_SERVER;
                    current_socket = connect_to_server();
                    if(current_socket >= 0) {
                        pthread_create(&recv_thread, NULL, receive_messages, &data);
                        add_message("Switched back to server connection");
                    }
                }
                else {
                    // Request P2P connection
                    send_to_current(input);
                }
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

    int disc = recv(current_socket, NULL, 0, 0);
    if(disc <= 0) {
        add_message("Disconnected from server");
    } else {
        add_message("Server closed connection");
    }
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);
    if (p2p_socket != -1) {
        close(p2p_socket);
        p2p_socket = -1;
    }
    if (current_socket != -1) {
        close(current_socket);
        current_socket = -1;
    }
    
}

int main() {
    signal(SIGINT,handle_sigint);
    get_username();
    init_ui();
    
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