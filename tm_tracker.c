#include <curses.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

enum {
    name_max_length = 30,
    max_categories = 5,
    max_intervals = 1000,
    key_escape = 27,
    key_enter = 10,
    default_timeout = 1000
};

enum {
    CMD_START = 's',
    CMD_FINISH = 'f',
    CMD_QUIT = 'q'
};

typedef struct Interval {
    char category[name_max_length];
    time_t start;
    time_t end;
} Interval;

static void print_time(Interval *interval, int start_y, int start_x)
{
    erase();
    time_t current = time(NULL);
    time_t passed = current - interval->start;

    int minutes = passed / 60;
    int seconds = passed % 60;

    mvprintw(start_y - 1, start_x, "%s", interval->category);
    mvprintw(start_y, start_x, "%02d:%02d", minutes, seconds);
    refresh();
}

static bool get_text_input(WINDOW *win, char *buffer, int max_len) {
    int char_count = strlen(buffer);
    int ch;
    curs_set(1);
    
    // If editing existing text, print it first
    mvwprintw(win, 1, 1, "%s", buffer); 
    wrefresh(win);

    while(1) {
        ch = wgetch(win);
        if(ch == key_escape) {
            curs_set(0);
            return false;
        }
        else if(ch == key_enter) {
            break;
        }
        else if(ch == KEY_BACKSPACE || ch == 127) { // Handle 127 for Mac/some terms
            if(char_count > 0) {
                char_count--;
                buffer[char_count] = '\0';
                mvwaddch(win, 1, 1 + char_count, ' '); // Clear visual
                wmove(win, 1, 1 + char_count);         // Move cursor back
            }
        }
        else if(ch >= 32 && ch <= 126 && char_count < max_len - 1) {
            buffer[char_count] = (char)ch;
            char_count++;
            buffer[char_count] = '\0';
            waddch(win, ch);
        }
        wrefresh(win);
    }
    curs_set(0);
    return true;
}

static void start_interval(Interval *intervals, int *total)
{
    clear();
    refresh();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    char message[100]; // Length of message below
    snprintf(message, sizeof(message), "Cannot have more than %d intervals. Press any key to return.", max_intervals);
    if(*total >= max_intervals) {
        mvprintw(rows / 2, (cols - strlen(message)) / 2, "%s", message);
        getch();
        return;
    }

    int height = 3;
    int width = name_max_length + 25; // 25 chars for query text & <-Esc
    int start_y = (rows - height) / 2;
    int start_x = (cols - width) / 2;

    Interval *current = &intervals[*total];

    WINDOW *win = newwin(height, width, start_y, start_x);
    keypad(win, TRUE);
    
    box(win, 0, 0); 

//    int attr;
//    dimmed_attr(&attr);
//    wattron(win, attr); 
//    mvwprintw(win, 1, width - esc_hint_length - 1, ESC_HINT);
//    wattroff(win, attron(attr)); 

    char temp_name[name_max_length] = {0};
    
    do {
        if(!get_text_input(win, temp_name, name_max_length)) {
            delwin(0);
            return;
        }
    } while(strlen(temp_name) <= 0);
    strncpy(current->category, temp_name, name_max_length - 1);
    current->category[name_max_length - 1] = '\0';
   
    current->start = time(NULL);
    current->end = 0;
    (*total)++;

    delwin(win);
}

static void main_screen(Interval *intervals, int intervals_total)
{
    timeout(default_timeout);

    int r, c;
    getmaxyx(stdscr, r, c);

    int start_y = r / 2;
    int start_x = c / 2;

    bool active = false;

    while(1) {
        if(active){
            print_time(&intervals[intervals_total - 1], start_y, start_x);
        }
        else
            mvprintw(start_y, start_x, "PRESS [s] TO BEGIN");

        int key = getch();
        switch(key) {
            case CMD_START:
                start_interval(intervals, &intervals_total);
                active = true;
                break;
            case CMD_FINISH:
                erase();
                refresh();
                intervals[intervals_total].end = time(NULL);
                intervals_total--;
                active = false;
                break;
            case CMD_QUIT:
            case key_escape:
                endwin();
                exit(0);
        }
    }
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, 1);
    curs_set(0);

    int intervals_total = 0;
    Interval my_intervals[max_intervals];

    main_screen(my_intervals, intervals_total);

    endwin();
    return 0;
}
