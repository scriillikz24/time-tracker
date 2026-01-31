#define _DEFAULT_SOURCE
#include <curses.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif
#define CATEGORIES_FILE ".categories.dat"
#define INTERVALS_FILE ".intervals.dat"
#define ESC_HINT "<- Esc"

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
    CMD_DELETE = 'd',
    CMD_QUIT = 'q',
    CMD_CATEGORY = 'c',
    CMD_INTERVAL = 'i',
    CMD_CREATE = 'a'

};

typedef struct Category {
    char name[name_max_length];
} Category;

typedef struct Interval {
    int category_idx; // Index into the categories array
    time_t start;
    time_t end;
} Interval;

static void print_time(WINDOW *win, Interval *interval, Category *categories, int height, int width)
{
    time_t current = time(NULL);
    time_t passed = current - interval->start;

    int minutes = passed / 60;
    int seconds = passed % 60;

    char *category = categories[interval->category_idx].name;
    wattron(win, A_BOLD);
    mvwprintw(win, 1, (width - strlen(category)) / 2, "%s", category);
    wattroff(win, A_BOLD);
    wattron(win, COLOR_PAIR(3));
    mvwprintw(win, 1, 1, "%s", ESC_HINT);
    wattroff(win, COLOR_PAIR(3));
    mvwprintw(win, height - 2, (width - 4) / 2, "%02d:%02d", minutes, seconds);
    wrefresh(win);
}

static bool get_text_input(char *buffer, int max_len) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int height = 3;
    int width = name_max_length + 2;
    int start_y = (rows - height) / 2;
    int start_x = (cols - width) / 2;

    WINDOW *win = newwin(height, width, start_y, start_x);
    keypad(win, TRUE);
    
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
            werase(win);
            wrefresh(win);
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
    werase(win);
    wrefresh(win);
    delwin(win);
    return true;
}

static void print_category_item(Category *category, int y, int x, bool highlighted) {
    if(!highlighted) attron(COLOR_PAIR(3)); 
    mvprintw(y, x, "%c %s", highlighted ? '>' : ' ', category->name);
    if(!highlighted) attroff(COLOR_PAIR(3)); 
}

static void add_category(Category *categories, int *category_count)
{
    clear();
    refresh();

    char temp_name[name_max_length] = {0};

    do {
        if(!get_text_input(temp_name, name_max_length)) {
            return;
        }
    } while(strlen(temp_name) <= 0);
    strncpy(categories[*category_count].name, temp_name, name_max_length - 1);
    categories[*category_count].name[name_max_length - 1] = '\0';
    (*category_count)++;
}

static void delete_category(Category *categories, int *category_count, int idx)
{
    int i;
    for(i = idx; i < (*category_count) - 1; i++)
        categories[i] = categories[i+1];
    (*category_count)--;
}

static void delete_interval(Interval *intervals, int *count, int idx)
{
    for(int i = idx; i < (*count) - 1; i++)
        intervals[i] = intervals[i+1];
    (*count)--;
}

static int categories_dashboard(Category *categories, int *category_count, int y, int x)
{
    erase(); 
    refresh();
    int highlight = 0;

    while(1) {
        attron(A_BOLD);
        mvprintw(y - 3, x, "THE CATEGORIES DASHBOARD");
        attroff(A_BOLD);

        if(*category_count == 0)
            mvprintw(y, x, "- Press [a] to add a category.");

        for(int i = 0; i < *category_count; i++) {
            print_category_item(&categories[i], y + i, x, i == highlight);
        }

        int key;
        key = getch();
        switch(key) {
            case CMD_CREATE:
                add_category(categories, category_count);
                break;
            case CMD_DELETE:
                delete_category(categories, category_count, highlight);
                if(highlight >= (*category_count))
                    highlight = *category_count - 1;
                erase(); 
                refresh();
                break;
            case 'k':
            case KEY_UP:
                if(highlight > 0) highlight--;
                break;
            case 'j':
            case KEY_DOWN:
                if(highlight < *category_count - 1) highlight++;
                break;
            case key_enter:
                clear();
                refresh();
                return highlight;
            case key_escape:
                clear();
                refresh();
                return -1;
        }
    }
}

static void print_interval_item(Interval *interval, Category *categories, int y, int x, bool highlighted)
{
    time_t time_focused = interval->end - interval->start;
    struct tm *start = localtime(&interval->start);
    int start_h = start->tm_hour;
    int start_m = start->tm_min;
    struct tm *end = localtime(&interval->end);
    int end_h = end->tm_hour;
    int end_m = end->tm_min;
    int minutes_focused = time_focused / 60;
    int seconds_focused = time_focused % 60;
    if(!highlighted) attron(COLOR_PAIR(3));
    mvprintw(y, x, "%c %s: %d:%d-%d:%d(%dm%ds)", highlighted ? '>' : '-', categories[interval->category_idx].name,
            start_h, start_m, end_h, end_m, minutes_focused, seconds_focused);
    if(!highlighted) attroff(COLOR_PAIR(3));
}

static void intervals_dashboard(Interval *intervals, Category *categories, int *interval_count, int start_y, int start_x)
{
    timeout(-1);
    erase();
    refresh();

    int highlight = 0;

    while(1) {
        attron(A_BOLD);
        mvprintw(start_y - 3, start_x, "THE INTERVALS DASHBOARD");
        attroff(A_BOLD);

        if(*interval_count == 0) {
            mvprintw(start_y, start_x, "-- No intervals to display --");
            getch();
            clear();
            refresh();
            timeout(default_timeout);
            return;
        }

        for(int i = 0; i < *interval_count; i++)
            print_interval_item(&intervals[i], categories, start_y + i, start_x, i == highlight);

        int key;
        key = getch();
        switch(key) {
            case CMD_DELETE:
                delete_interval(intervals, interval_count, highlight);
                if(highlight >= (*interval_count))
                    highlight = *interval_count - 1;
                erase(); 
                refresh();
                break;
            case 'k':
            case KEY_UP:
                if(highlight > 0) highlight--;
                break;
            case 'j':
            case KEY_DOWN:
                if(highlight < *interval_count - 1) highlight++;
                break;
            case key_escape:
                clear();
                refresh();
                timeout(default_timeout);
                return;
        }
    }
}

static bool start_interval(Interval *intervals, int *interval_count, Category *categories, int *category_count)
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
        return false;
    }

    int start_y = rows / 2;
    int start_x = cols / 2;

    Interval *current = &intervals[*interval_count];

    if(*category_count == 0) {
        mvprintw(start_y, start_x, "Create a category first.");
        timeout(-1);
        getch(); 
        timeout(default_timeout);
        clear();
        refresh();
        return false; // Not success
    }
    else {
        int idx = categories_dashboard(categories, category_count, start_y, start_x);
        if(idx >= 0) current->category_idx = idx;
        else return false;
    }
   
    current->start = time(NULL);
    current->end = 0;
    (*interval_count)++;
    return true;
}

static void get_data_path(char *dest, char *file_name) {
    const char *home = getenv("HOME");
    if(home == NULL)
        strncpy(dest, file_name, PATH_MAX);
    else
        snprintf(dest, PATH_MAX, "%s/%s", home, file_name);
}

static void push(void *attr, size_t size, int count, char *file_name)
{
    char path[PATH_MAX];
    get_data_path(path, file_name);

    FILE *dest = fopen(path, "wb");
    if(!dest) {
        perror(file_name);
        exit(1);
    }

    fwrite(&count, sizeof(int), 1, dest);
    fwrite(attr, size, count, dest);
    fclose(dest);
}

static void pull(void *attr, size_t size, int *count, char *file_name)
{
    char path[PATH_MAX];
    get_data_path(path, file_name);

    FILE *source = fopen(path, "rb");
    if(!source) {
        return;
    }
    fread(count, sizeof(int), 1, source);
    fread(attr, size, *count, source);
    fclose(source);
}

static void active_screen(Interval *interval, Category *categories)
{
    erase();
    refresh();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int height = 5;
    int width = name_max_length + 3; // +2 for padding
    int start_y = (rows - height) / 2;
    int start_x = (cols - width) / 2;

    WINDOW *win = newwin(height, width, start_y, start_x);
    keypad(win, TRUE);
    
    box(win, 0, 0); 
    
    while(1) {
        print_time(win, interval, categories, height, width);

        int key = getch();
        if(key == key_escape || key == 'q') {
            interval->end = time(NULL);
            werase(win);
            wrefresh(win);
            delwin(win);
            return;
        }
    }
}

static void main_screen(Interval *intervals, int *interval_count, Category *categories, int *category_count)
{
    timeout(default_timeout);

    int r, c;
    getmaxyx(stdscr, r, c);

    int start_y = (r - 10) / 2;
    int start_x = c / 2;

    while(1) {
        attron(A_BOLD);
        mvprintw(start_y - 3, start_x, "THE MAIN SCREEN");
        attroff(A_BOLD);

        mvprintw(start_y, start_x, "- PRESS [s] TO START AN INTERVAL");
        mvprintw(start_y + 1, start_x, "- PRESS [c] TO ENTER THE CATEGORY DASHBOARD");
        mvprintw(start_y + 2, start_x, "- PRESS [i] TO ENTER THE INTERVAL DASHBOARD");

        int key = getch();
        switch(key) {
            case CMD_START:
                if(start_interval(intervals, interval_count, categories, category_count))
                    active_screen(&intervals[*interval_count - 1], categories);
                break;
            case CMD_CATEGORY:
                categories_dashboard(categories, category_count, start_y, start_x);
                break;
            case CMD_INTERVAL:
                intervals_dashboard(intervals, categories, interval_count, start_y + 3, start_x);
                break;
            case CMD_QUIT:
            case key_escape:
                push(categories, sizeof(Category), *category_count, CATEGORIES_FILE);
                push(intervals, sizeof(Interval), *interval_count, INTERVALS_FILE);
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

    Interval intervals[max_intervals];
    int interval_count = 0;

    pull(categories, sizeof(Category), &category_count, CATEGORIES_FILE);
    pull(intervals, sizeof(Interval), &interval_count, INTERVALS_FILE);

    main_screen(intervals, &interval_count, categories, &category_count);

    endwin();
    return 0;
}
