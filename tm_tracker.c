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
    bar_gap = 4,
    bar_height = 3,
    minutes_in_hour = 60,
    days_in_year = 366,
    months_in_year = 12
};

enum {
    CMD_START = 's',
    CMD_DELETE = 'd',
    CMD_CATEGORY = 'c',
    CMD_HISTORY = 'h',
    CMD_STATS = 't',
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

static void action_bar(const char **bar_items, int bar_count)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int total_width = 0;
    for(int i = 0; i < bar_count; i++) 
        total_width += strlen(bar_items[i]) + bar_gap;

    // Center the bar at the bottom of the screen
    int x_offset = (cols - total_width + bar_gap) / 2;
    int y_pos = rows - 2;

    // Draw a background strip for the menu
    attron(COLOR_PAIR(3)); 
    mvhline(y_pos - 1, 0, ACS_HLINE, cols); 
    mvhline(y_pos + 1, 0, ACS_HLINE, cols); 
    attroff(COLOR_PAIR(3));
    
    for(int i = 0; i < bar_count; i++) {
        mvaddstr(y_pos, x_offset, bar_items[i]);
        x_offset += strlen(bar_items[i]) + bar_gap;
    }
    refresh();
}

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

static void categories_dashboard(Category *categories, int *category_count, int *chosen)
{
    erase(); 
    refresh();
    int highlight = 0;

    int r, c;
    getmaxyx(stdscr, r, c);
    int y = r / 2;
    int x = c / 2;

    static const char *bar_items[] = {
        "[a] Add",
        "[d] Delete",
        "[Esc] Back"
    };

    int bar_count = sizeof(bar_items) / sizeof(bar_items[0]);

    while(1) {
        attron(A_BOLD);
        mvprintw(y - 3, x, "CATEGORIES DASHBOARD");
        attroff(A_BOLD);

        action_bar(bar_items, bar_count);

        if(*category_count == 0)
            mvprintw(y, x, "-- Nothing to display yet --");

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
                *chosen = highlight;
                return;
            case key_escape:
                clear();
                refresh();
                *chosen = -1;
                return;
        }
    }
}

static void print_interval_item(Interval *interval, Category *categories, int y, int x, bool highlighted)
{
    time_t time_focused = interval->end - interval->start;
    struct tm *start = localtime(&interval->start);
    int start_h = start->tm_hour;
    int start_m = start->tm_min;
    int start_month = start->tm_mon + 1; 
    int start_day = start->tm_mday;
    struct tm *end = localtime(&interval->end);
    int end_h = end->tm_hour;
    int end_m = end->tm_min;
    int minutes_focused = time_focused / 60;
    int seconds_focused = time_focused % 60;
    if(!highlighted) attron(COLOR_PAIR(3));
    mvprintw(y, x, "%c %s: [%02d/%02d]%02d:%02d-%02d:%02d(%02dm%02ds)", highlighted ? '>' : '-', categories[interval->category_idx].name,
            start_day, start_month, start_h, start_m, end_h, end_m, minutes_focused, seconds_focused);
    if(!highlighted) attroff(COLOR_PAIR(3));
}

static void history_dashboard(Interval *intervals, Category *categories, int *interval_count)
{
    timeout(-1);
    erase();
    refresh();

    int r, c;
    getmaxyx(stdscr, r, c);
    int start_y = (r - 5) / 2;
    int start_x = (c - name_max_length) / 2;

    static const char *bar_items[] = {
        "[d] Delete",
        "[Esc] Back"
    };

    int bar_count = sizeof(bar_items) / sizeof(bar_items[0]);

    int highlight = 0;

    while(1) {
        attron(A_BOLD);
        mvprintw(start_y - 3, start_x, "HISTORY");
        attroff(A_BOLD);

        action_bar(bar_items, bar_count);

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
        int idx;
        categories_dashboard(categories, category_count, &idx);
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

static int get_day_total_time(Interval *intervals, Category *categories, int interval_count, int day)
{
    int total = 0;
    for(int i = 0; i < interval_count; i++) {
        int interval_day = localtime(&intervals[i].start)->tm_yday;
        if(interval_day == day) {
            total += intervals[i].end - intervals[i].start;
        }
    }
    return total;
}

static int get_month_total_time(Interval *intervals, Category *categories, int interval_count, int month)
{
    int total = 0;
    for(int i = 0; i < interval_count; i++) {
        int interval_month = localtime(&intervals[i].start)->tm_mon;
        if(interval_month == month) {
            total += intervals[i].end - intervals[i].start;
        }
    }
    return total;
}

static int get_year_total_time(Interval *intervals, Category *categories, int interval_count, int year)
{
    int total = 0;
    for(int i = 0; i < interval_count; i++) {
        int interval_year = localtime(&intervals[i].start)->tm_year;
        if(interval_year == year) {
            total += intervals[i].end - intervals[i].start;
        }
    }
    return total;
}

typedef int (*get_total_func)(Interval*, Category*, int, int);
typedef void(*update_time)(struct tm*, struct tm*, int);
typedef int(*get_total_target)(struct tm*);
typedef void(*display_date_line)(struct tm*, int, int);

static int get_yday(struct tm *dynamic_t) { return dynamic_t->tm_yday; }

static int get_month(struct tm *dynamic_t) { return dynamic_t->tm_mon; }

static int get_year(struct tm *dynamic_t) { return dynamic_t->tm_year; }

static void update_year(struct tm *dynamic_t, struct tm *t, int step)
{
    dynamic_t->tm_year += step;
    mktime(dynamic_t);
    if(step > 0 && dynamic_t->tm_year > t->tm_year)
        (dynamic_t->tm_year) -= step;
}

static void update_month(struct tm *dynamic_t, struct tm *t, int step)
{
    dynamic_t->tm_mon += step;
    mktime(dynamic_t);
    if(step > 0 && dynamic_t->tm_year == t->tm_year
            && dynamic_t->tm_mon > t->tm_mon)
        (dynamic_t->tm_mon) -= step;
}

static void update_day(struct tm *dynamic_t, struct tm *t, int step)
{
    dynamic_t->tm_mday += step;
    mktime(dynamic_t);
    if(dynamic_t->tm_yday > t->tm_yday 
            && dynamic_t->tm_year == t->tm_year)
        (dynamic_t->tm_mday) -= step;
}

static void display_year_line(struct tm *dynamic_t, int y, int x)
{
    attron(COLOR_PAIR(3));
    mvprintw(y + 2, x, "<- %d ->", dynamic_t->tm_year + 1900);
    attroff(COLOR_PAIR(3));
}

static void display_month_line(struct tm *dynamic_t, int y, int x)
{
    attron(COLOR_PAIR(3));
    mvprintw(y + 2, x, "<- %02d/%d ->", dynamic_t->tm_mon + 1, dynamic_t->tm_year + 1900);
    attroff(COLOR_PAIR(3));
}

static void display_day_line(struct tm *dynamic_t, int y, int x)
{
    attron(COLOR_PAIR(3));
    mvprintw(y + 2, x, "<- %02d/%02d/%d ->", dynamic_t->tm_mday, dynamic_t->tm_mon + 1, dynamic_t->tm_year + 1900);
    attroff(COLOR_PAIR(3));
}

static void stats(
        Interval *intervals,
        Category *categories,
        int interval_count,
        int y,
        int x,
        const char *title,
        display_date_line display_line,
        get_total_target get_target,
        update_time update_tm,
        get_total_func get_total)
{
    erase();
    refresh();
    time_t now = time(NULL);
    struct tm t = *localtime(&now);
    struct tm dynamic_t = t;

    while(1) {
        mktime(&dynamic_t);
        int total = get_total(intervals, categories,
                interval_count, get_target(&dynamic_t));

        attron(A_BOLD);
        mvprintw(y, x, "---- %s STATS ----", title);
        attroff(A_BOLD);

        display_line(&dynamic_t, y, x);
        mvprintw(y + 4, x, "Total: %02dm%02ds",
                total / minutes_in_hour,
                total % minutes_in_hour);
        refresh();

        int key = getch();
        switch(key) {
            case KEY_RIGHT:
                update_tm(&dynamic_t, &t, 1);
                break;
            case KEY_LEFT:
                update_tm(&dynamic_t, &t, -1);
                break;
            case key_escape:
                erase();
                refresh();
                return;
        }
    }

}

static void statistics_screen(Interval *intervals, Category *categories, int interval_count, int y, int x)
{
    erase();
    refresh();

    static const char *bar_items[] = {
        "[d] Day",
        "[m] Month",
        "[y] Year",
        "[Esc] Exit"
    };

    const char day_title[] = "DAY";
    const char month_title[] = "MONTH";
    const char year_title[] = "YEAR";

    int bar_count = sizeof(bar_items) / sizeof(bar_items[0]);

    while(1) {
        attron(A_BOLD);
        mvaddstr(y - 2, x, "-- STATISTICS SCREEN --");
        attroff(A_BOLD);

        action_bar(bar_items, bar_count);
        
        int key = getch();
        switch(key) {
            case 'd':
                stats(intervals, categories, interval_count, y, x,
                        day_title, display_day_line, get_yday,
                        update_day, get_day_total_time);
                break;
            case 'm':
                stats(intervals, categories, interval_count, y, x,
                        month_title, display_month_line, get_month,
                        update_month, get_month_total_time);
                break;
            case 'y':
                stats(intervals, categories, interval_count, y, x,
                        year_title, display_year_line, get_year,
                        update_year, get_year_total_time);
                break;
            case key_escape:
                erase();
                refresh();
                return;
        }
    }
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

    static const char *bar_items[] = {
        "[s] Start",
        "[c] Categories",
        "[h] History",
        "[t] Stats",
        "[Esc] Exit"
    };

    int bar_count = sizeof(bar_items) / sizeof(bar_items[0]);

    int r, c;
    getmaxyx(stdscr, r, c);

    int start_y = (r - bar_height) / 2;
    int start_x = c / 2;

    while(1) {
        attron(A_BOLD);
        mvaddstr(start_y, start_x, "-- MAIN SCREEN --");
        attroff(A_BOLD);

        action_bar(bar_items, bar_count);

        int key = getch();
        switch(key) {
            case CMD_START:
                if(start_interval(intervals, interval_count, categories, category_count))
                    active_screen(&intervals[*interval_count - 1], categories);
                break;
            case CMD_CATEGORY:
                int option;
                categories_dashboard(categories, category_count, &option);
                break;
            case CMD_HISTORY:
                history_dashboard(intervals, categories, interval_count);
                break;
            case CMD_STATS:
                statistics_screen(intervals, categories, *interval_count, start_y, start_x);
                break;
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
