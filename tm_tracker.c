#include <curses.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

enum {
    key_escape = 27,
    default_timeout = 1000
};

enum {
    CMD_START = 's',
    CMD_FINISH = 'f',
    CMD_QUIT = 'q'
};

static void print_time(int start_y, int start_x, time_t start)
{
    erase();
    time_t current = time(NULL);
    time_t passed = current - start;
    struct tm *t = localtime(&passed);
    mvprintw(start_y, start_x, "%02d:%02d", t->tm_min, t->tm_sec);
    refresh();
}

static void main_screen()
{
    timeout(default_timeout);

    int r, c;
    getmaxyx(stdscr, r, c);

    int start_y = r / 2;
    int start_x = c / 2;

    bool active = false;
    time_t start_time;
    

    while(1) {
        if(active){
            print_time(start_y, start_x, start_time);
        }
        else
            mvprintw(start_y, start_x, "PRESS [s] TO BEGIN");

        int key = getch();
        switch(key) {
            case CMD_START:
                active = !active;
                start_time = time(NULL);
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

    main_screen();

    endwin();
    return 0;
}
