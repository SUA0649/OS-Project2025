#pragma once
#include "client.h"

#define CUSTOM_COLOR_ID 10
#define CUSTOM_PAIR_ID 20
#define USER_LIST_WIDTH 30


//Forward Declaration.
void init_ui_p2p();
void add_messages(const char *msg);
void* receives_messages(void *arg);
void display_previous_chat_p2p(const char *target_user);
void update_log(const char *target_user, const char *message);

// Shared variables needed for P2P client
UI ui_p2p;
char peer_username[300] = {0};

// Implement UI functions
void init_ui_p2p() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, false);
    start_color();
    
    if (can_change_color()) {
        // RGB values for your desired background (203, 166, 247)
        // ncurses uses 0-1000 scale for colors
        init_color(CUSTOM_COLOR_ID, 102, 102, 180); // Converted from 255-scale
        init_pair(CUSTOM_PAIR_ID, COLOR_WHITE, CUSTOM_COLOR_ID);
        bkgd(COLOR_PAIR(CUSTOM_PAIR_ID));
    } else {
        // Fallback to black background if custom colors not supported
        init_pair(CUSTOM_PAIR_ID, COLOR_WHITE, COLOR_BLACK);
        bkgd(COLOR_PAIR(CUSTOM_PAIR_ID));
    }

    clear();
    refresh();

    ui_p2p.chat_win = newwin(LINES-3, COLS-1, 0, 0);
    ui_p2p.input_win = newwin(3, COLS-1, LINES-3, 0);
    scrollok(ui_p2p.chat_win, TRUE);

    box(ui_p2p.chat_win, 0, 0);
    mvwprintw(ui_p2p.chat_win, 0, 2, " P2P Chat ");
    
    ui_p2p.chat_line = 1;
    
    wbkgd(ui_p2p.chat_win, COLOR_PAIR(CUSTOM_PAIR_ID));
    wbkgd(ui_p2p.input_win, COLOR_PAIR(CUSTOM_PAIR_ID));
    
    wrefresh(ui_p2p.chat_win);
    
    box(ui_p2p.input_win, 0, 0);
    mvwprintw(ui_p2p.input_win, 1, 0, "> ");
    wrefresh(ui_p2p.input_win);

    ui_p2p.users_win = NULL;
}


//* Implement message adding functions
void add_messages(const char *msg) {
    int maxy, maxx;
    getmaxyx(ui_p2p.chat_win, maxy, maxx);
    int available_height = maxy - 2;  // Account for borders
    int available_width = maxx - 2;   // Account for borders
    
    if (available_height <= 0) return;

    char buffer[MAX_MSG];
    strncpy(buffer, msg, MAX_MSG-1);
    buffer[MAX_MSG-1] = '\0';

    char *line = buffer;
    while (*line) {
        // Handle scrolling if needed
        if (ui_p2p.chat_line >= available_height) {
            for (int i = 1; i < available_height; i++) {
                char line_content[available_width + 1];
                mvwinstr(ui_p2p.chat_win, i + 1, 1, line_content);
                mvwprintw(ui_p2p.chat_win, i, 1, "%-*s", available_width, line_content);
            }

            wmove(ui_p2p.chat_win, available_height - 1, 1);
            wclrtoeol(ui_p2p.chat_win);
            ui_p2p.chat_line = available_height - 1;
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
            mvwprintw(ui_p2p.chat_win, ui_p2p.chat_line, 1, "%s", line);
            line[cut] = saved;
            line += cut + (saved == ' ' ? 1 : 0);
            ui_p2p.chat_line++;
        } else {
            mvwprintw(ui_p2p.chat_win, ui_p2p.chat_line, 1, "%s", line);
            ui_p2p.chat_line++;
            break;
        }
    }

    box(ui_p2p.chat_win, 0, 0);
    mvwprintw(ui_p2p.chat_win, 0, 2, " P2P Chat ");
    wrefresh(ui_p2p.chat_win);
}

void* receives_messages(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    char buffer[MAX_MSG];
    
    memset(buffer, 0, sizeof(buffer));
    int valread = recv(data->sockfd, buffer, MAX_MSG, 0);
    strcpy(peer_username,buffer);
    display_previous_chat_p2p(buffer);
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        valread = recv(data->sockfd, buffer, MAX_MSG, 0);
        
        if(valread <= 0) {
            add_messages("Peer disconnected");
            break;
        }
        // char *buf_cpy[50];
        // strcpy(buf_cpy,buffer);
        add_messages(buffer);
        update_log(peer_username,buffer);
    }
    return NULL;
}

//* Used for storing the logs in the LOGS FOLDER.
void display_previous_chat_p2p(const char *target_user) {
    char file[MAX_FILEPATH];
    snprintf(file, MAX_FILEPATH, "Logs/%s.json", username);

    char *json_data = read_file(file);
    if (!json_data) {
        add_messages("No previous chat found.\n");
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
        add_messages("No messages found.\n");
        cJSON_Delete(root);
        return;
    }

    cJSON *chat_array = cJSON_GetObjectItem(messages, target_user);
    if (!chat_array || !cJSON_IsArray(chat_array)) {
        add_messages("No previous messages with user.\n");
        cJSON_Delete(root);
        return;
    }

    cJSON *msg = NULL;
    cJSON_ArrayForEach(msg, chat_array) {
        if (cJSON_IsString(msg)) {
            add_messages(msg->valuestring);
        }
    }

    cJSON_Delete(root);
}

