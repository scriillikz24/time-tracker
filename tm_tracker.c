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
    default_timeout = 1000,
    colors_max = 256,
};

enum {
    CMD_START = 's',
    CMD_FINISH = 'f',
    CMD_QUIT = 'q'
};

typedef struct Category {
    char name[name_max_length];
} Category;

typedef struct Interval {
    int category_idx; // Index into the categories array
    time_t start;
    time_t end;
} Interval;

static void print_time(Interval *interval, Category *categories,  int start_y, int start_x)
{
    erase();
    time_t current = time(NULL);
    time_t passed = current - interval->start;

    int minutes = passed / 60;
    int seconds = passed % 60;

    mvprintw(start_y - 1, start_x, "%s", categories[interval->category_idx].name);
    mvprintw(start_y, start_x, "%02d:%02d", minutes, seconds);
    refresh();
}

static bool get_text_input(WINDOW *win, char *buffer, int max_len) {
    werase(win);
    wrefresh(win);
    box(win, 0, 0);

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

static void print_category_item(WINDOW *win, Category *category, int y, int x, bool highlighted) {
    if(!highlighted) attron(COLOR_PAIR(3)); 
    mvwprintw(win, y, x, "%s", category->name);
    if(!highlighted) attroff(COLOR_PAIR(3)); 
}

static int categories_dashboard(WINDOW *win, Category *categories, int *category_count)
{
    werase(win);
    wrefresh(win);

    keypad(win, 1);
    int highlight = 0;
    while(1) {
        for(int i = 0; i < *category_count; i++) {
            print_category_item(win, &categories[i], 3, 1, i == highlight);
        }

        int key;
        key = wgetch(win);
        switch(key) {
            case KEY_UP:
                if(highlight < *category_count) highlight++;
                break;
            case KEY_DOWN:
                if(highlight > 0) highlight--;
                break;
            case key_enter:
                return highlight;
        }
    }
}

static void start_interval(Interval *intervals, int *interval_count, Category *categories, int *category_count)
{
    clear();
    refresh();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    char message[100]; // Length of message below
    snprintf(message, sizeof(message), "Cannot have more than %d intervals. Press any key to return.", max_intervals);
    if(*interval_count >= max_intervals) {
        mvprintw(rows / 2, (cols - strlen(message)) / 2, "%s", message);
        getch();
        return;
    }

    int height = 4 + *category_count;
    int width = name_max_length + 25;
    int start_y = (rows - height) / 2;
    int start_x = (cols - width) / 2;

    Interval *current = &intervals[*interval_count];

    WINDOW *win = newwin(height, width, start_y, start_x);
    keypad(win, TRUE);
    
    box(win, 0, 0); 
    
    char temp_name[name_max_length] = {0};
    if(*category_count == 0) {
        mvwprintw(win, 1, 1, "Press [a] to create new category.");
        if(wgetch(win) != 'a')
            return;
        do {
            if(!get_text_input(win, temp_name, name_max_length)) {
                delwin(0);
                return;
            }
        } while(strlen(temp_name) <= 0);
        strncpy(categories[current->category_idx].name, temp_name, name_max_length - 1);
        categories[current->category_idx].name[name_max_length - 1] = '\0';
        current->category_idx = *category_count;
        (*category_count)++;
    }
    else {
        current->category_idx = categories_dashboard(win, categories, category_count);
    }
   
    current->start = time(NULL);
    current->end = 0;
    (*interval_count)++;

    delwin(win);
}

static void main_screen(Interval *intervals, int interval_count, Category *categories, int category_count)
{
    timeout(default_timeout);

    int r, c;
    getmaxyx(stdscr, r, c);

    int start_y = r / 2;
    int start_x = c / 2;

    bool active = false;

    while(1) {
        if(active){
            print_time(&intervals[interval_count - 1], categories, start_y, start_x);
        }
        else
            mvprintw(start_y, start_x, "PRESS [s] TO BEGIN");

        int key = getch();
        switch(key) {
            case CMD_START:
                start_interval(intervals, &interval_count, categories, &category_count);
                active = true;
                break;
            case CMD_FINISH:
                erase();
                refresh();
                intervals[interval_count].end = time(NULL);
                interval_count--;
                active = false;
                break;
            case CMD_QUIT:
            case key_escape:
                endwin();
                exit(0);
        }
    }
}

static void init_colors()
{
    if(!has_colors())
        return;

    start_color();

    init_pair(1, COLOR_GREEN, COLOR_WHITE); 
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_RED, COLOR_BLACK);
    init_pair(6, COLOR_YELLOW, COLOR_BLACK);

    // High-Definition vs. Fallback Pairs
    if(COLORS >= colors_max) {
        init_pair(3, 242, COLOR_BLACK); // Dimmed grey
        init_pair(4, COLOR_GREEN, 242);
        init_pair(7, COLOR_RED, 242);
        init_pair(8, COLOR_WHITE, 242);
        init_pair(9, 250, COLOR_BLACK); // Light grey
    } else {
        // Fallback for 8/16 color terminals (e.g., standard macOS Terminal)
        init_pair(3, COLOR_WHITE, COLOR_BLACK); // Use white... 
        init_pair(4, COLOR_GREEN, COLOR_WHITE);
        init_pair(7, COLOR_RED, COLOR_WHITE);
        init_pair(8, COLOR_BLACK, COLOR_WHITE);
        init_pair(9, COLOR_WHITE, COLOR_BLACK);
    }
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, 1);
    curs_set(0);
    init_colors();

    Category categories[max_categories]; 
    int category_count = 0;

    Interval my_intervals[max_intervals];
    int interval_count = 0;

    main_screen(my_intervals, interval_count, categories, category_count);

    endwin();
    return 0;
}
