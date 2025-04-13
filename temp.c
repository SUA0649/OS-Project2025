#include <ncurses.h>
#include <string.h>
#include <stdlib.h> // For malloc()

#define INPUT_HEIGHT 3
#define MAX_MESSAGES 100

typedef struct {
    char text[256];
} Message;

Message messages[MAX_MESSAGES];
int msg_count = 2;

void update_chat(WINDOW *win) {
    werase(win);
    box(win, 0, 0);
    mvwprintw(win,1,1,"Chat messages appear here...\n");
    // Display last 10 messages (or all if less than 10)
    int start = (msg_count > 10) ? msg_count - 10 : 1;
    for (int i = start; i < msg_count; i++) {
        mvwprintw(win, i - start + 1, 1, "%s", messages[i].text);
    }
    
    wrefresh(win);
}

int main() {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    // Colors
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);  //Chat text
    init_pair(2, COLOR_WHITE, COLOR_BLUE);  //Input box

    //Create windows
    WINDOW *chat_win = newwin(LINES - INPUT_HEIGHT, COLS, 0, 0);
    WINDOW *input_win = newwin(INPUT_HEIGHT, COLS,  LINES - INPUT_HEIGHT, 0);
    scrollok(chat_win, TRUE);

    // Initial display
    update_chat(chat_win);

    //Main loop
    while (1){

        
        //Draw input box
        werase(input_win);
        wattron(input_win, COLOR_PAIR(2));
        box(input_win, 0 , 0);
        mvwprintw(input_win, 1, 1, "> ");
        wattroff(input_win, COLOR_PAIR(2));
        wrefresh(input_win);

        // Get input
        char msg[256];
        echo();
        mvwgetnstr(input_win, 1, 3, msg, sizeof(msg) - 1);
        noecho();

        if (strcmp(msg, "quit") == 0) break;

        // Store and display new message
        if (msg_count < MAX_MESSAGES) {
            snprintf(messages[msg_count].text, sizeof(messages[msg_count].text), 
                    "User: %s", msg);
            msg_count++;
        }
        update_chat(chat_win); // <-- THIS WAS MISSING IN ORIGINAL
    }

    endwin();
    return 0;
}