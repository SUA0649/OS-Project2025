#include "client_p2p.h"

int main(int argc, char *argv[]) {
    if (argc == 5 && strcmp(argv[1], "server") == 0) {
        // SERVER MODE - Full listener implementation
        init_ui_p2p();
        strcpy(username, argv[2]);
        int p2p_port = atoi(argv[3]);
        char temp[MAX_MSG];
        snprintf(temp, sizeof(temp), "P2P Server Port: %s : Server IP: %s", argv[3], argv[4]);
        add_messages(temp);
        add_messages("P2P Server started. Waiting for connections...");
        
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
            add_messages("Bind failed!");
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
            add_messages("Accept failed!");
            endwin();
            return 1;
        }
        
        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, INET_ADDRSTRLEN);
        char msg[MAX_MSG];
        snprintf(msg, sizeof(msg), "Connected to %s:%d", 
                peer_ip, ntohs(peer_addr.sin_port));
        add_messages(msg);
    }

    else if (argc >= 4 && strcmp(argv[1], "client") == 0) {
        // CLIENT MODE - Connect to existing server
        strcpy(username, argv[2]);
        char *peer_ip = argv[3];
        int peer_port = atoi(argv[4]);
        init_ui_p2p();
        char temp[MAX_MSG];
        snprintf(temp, sizeof(temp), "Connecting to Server Port: %d : Server IP: %s", peer_port, peer_ip);
        add_messages(temp);
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
        add_messages("Connection failed!");
        endwin();
        return 1;
    }
    add_messages("Connected to peer!");
    add_messages("Type your messages below. Press ` to quit.");

    // Start message receiver thread
    pthread_t recv_thread;
    ThreadData data = {p2p_socket, &ui_p2p};
    pthread_create(&recv_thread, NULL, receives_messages, &data);

    // Main input loop
    char input[200] = {0};
    int ch, pos = 0;
    send(p2p_socket,username,strlen(username),0);
    while(1) {
        ch = getch();
        
        if(ch == '`') {
            break;
        }
        else if(ch == '\n' && pos > 0) {
            char formatted_msg[700 + 20];
            snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", username, input);
            add_messages(formatted_msg);
            update_log(peer_username,formatted_msg);
            send(p2p_socket, formatted_msg, strlen(formatted_msg), 0);
            pos = 0;
            memset(input, 0, sizeof(input));
            wmove(ui_p2p.input_win, 1, 2);
            wclrtobot(ui_p2p.input_win);
            box(ui_p2p.input_win, 0, 0);
            mvwprintw(ui_p2p.input_win, 1, 0, "> ");
            wrefresh(ui_p2p.input_win);
        }
        else if((ch == 127 || ch == KEY_BACKSPACE) && pos > 0) {
            input[--pos] = '\0';
            mvwprintw(ui_p2p.input_win, 1, 2+pos, " ");
            wmove(ui_p2p.input_win, 1, 2+pos);
            wrefresh(ui_p2p.input_win);
        }
        else if(ch != ERR && pos < MAX_MSG-1) {
            input[pos++] = ch;
            wprintw(ui_p2p.input_win, "%c", ch);
            wrefresh(ui_p2p.input_win);
        }
    }

    // Cleanup
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);
    close(p2p_socket);p2p_socket = -1;
    endwin();
    return 0;
}
