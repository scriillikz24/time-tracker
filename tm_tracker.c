#include <curses.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

enum {
    name_max_length = 30,
    max_tasks = 10,
    max_tags = 3,
    max_intervals = 20,
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
    time_t start;
    time_t end;
} Interval;

typedef struct Task {
    char name[name_max_length];
    Interval interval;
    bool active;
} Task;

static void print_time(Task *task, int start_y, int start_x)
{
    erase();
    time_t current = time(NULL);
    time_t passed = current - task->interval.start;
    struct tm *t = localtime(&passed);
    mvprintw(start_y - 1, start_x, "%s", task->name);
    mvprintw(start_y, start_x, "%02d:%02d", t->tm_min, t->tm_sec);
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

static void add_task(Task *tasks, int *total)
{
    clear();
    refresh();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    char message[60]; // Length of message below
    snprintf(message, sizeof(message), "Cannot have more than %d tasks. Press any key to return.", max_tasks);
    if(*total >= max_tasks) {
        mvprintw(rows / 2, (cols - strlen(message)) / 2, "%s", message);
        getch();
        return;
    }

    int height = 3;
    int width = name_max_length + 25; // 25 chars for query text & <-Esc
    int start_y = (rows - height) / 2;
    int start_x = (cols - width) / 2;

    Task *current = &tasks[*total];

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
    strncpy(current->name, temp_name, name_max_length - 1);
    current->name[name_max_length - 1] = '\0';
   
    time_t start_time = time(NULL);
    current->interval.start = start_time;
    current->interval.end = 0;
    current->active = true;
    (*total)++;

    delwin(win);
}

static void main_screen(Task *tasks, int tasks_total)
{
    timeout(default_timeout);

    int r, c;
    getmaxyx(stdscr, r, c);

    int start_y = r / 2;
    int start_x = c / 2;

    bool active = false;

    while(1) {
        if(active){
            print_time(&tasks[tasks_total], start_y, start_x);
        }
        else
            mvprintw(start_y, start_x, "PRESS [s] TO BEGIN");

        int key = getch();
        switch(key) {
            case CMD_START:
                add_task(tasks, &tasks_total);
                active = true;
                break;
            case CMD_FINISH:
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

    int total = 0;
    Task my_tasks[max_tasks];

    main_screen(my_tasks, total);

    endwin();
    return 0;
}
