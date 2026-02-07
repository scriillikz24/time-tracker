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
#define DAY_TITLE "DAY"

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
    seconds_in_minute = 60,
    days_in_year = 366,
    days_in_week = 7,
    months_in_year = 12,
    min_time = 5 // in seconds
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
    
    for(int i = 0; i < bar_count; i++) {
        mvaddstr(y_pos, x_offset, bar_items[i]);
        x_offset += strlen(bar_items[i]) + bar_gap;
    }
    attroff(COLOR_PAIR(3));
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
    mvwprintw(win, height - 4, (width - 4) / 2, "%02d:%02d", minutes, seconds);
    char msg[name_max_length] = {0};
    if(time(NULL) - interval->start < min_time)
        strncpy(msg, "[Esc] Give Up!", name_max_length - 1);
    else
        strncpy(msg, "[Esc] Stop", name_max_length - 1);
    mvwhline(win, height - 2, 1, ' ', width - 2);
    wattron(win, COLOR_PAIR(3));
    mvwprintw(win, height - 2, (width - strlen(msg)) / 2, "%s", msg);
    wattroff(win, COLOR_PAIR(3));
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

static void print_category_item(char *name, int y, int x, bool highlighted) {
    if(!highlighted) attron(COLOR_PAIR(3)); 
    mvprintw(y, x, "%c %s", highlighted ? '>' : '-', name);
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
    for(int i = idx; i < (*category_count) - 1; i++)
        categories[i] = categories[i+1];
    (*category_count)--;
}

static void delete_interval(Interval *intervals, int *count, int idx)
{
    for(int i = idx; i < (*count) - 1; i++)
        intervals[i] = intervals[i+1];
    (*count)--;
}

typedef void (*print_query)(WINDOW*, const char*, int);

static void print_exit_query(
        WINDOW *win,
        const char *action_string,
        int width
        )
{
    wattron(win, COLOR_PAIR(3)); 
    mvwaddstr(win, 3, (width - 33) / 2,
            "Are you sure you want to "); // 33 is the length of the string
    wattroff(win, COLOR_PAIR(3)); 
    wattron(win, COLOR_PAIR(5)); 
    wprintw(win, "%s", action_string);
    wattroff(win, COLOR_PAIR(5)); 
    wattron(win, COLOR_PAIR(3)); 
    waddch(win, '?');
    wattroff(win, COLOR_PAIR(3)); 
}

static void print_delete_query(
        WINDOW *win,
        const char *category_name,
        int width)
{
    wattron(win, COLOR_PAIR(3)); 
    mvwprintw(win, 3, (width - 32) / 2,
            "Are you sure you want to delete:"); // 32 is the length of the string
    wattroff(win, COLOR_PAIR(3)); 
    
    mvwprintw(win, 4, (width - strlen(category_name) - 2) / 2,
            "'%s'?", category_name);
}

static bool confirm_action(
        print_query print_qr,
        const char *query_msg)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();
    refresh();

    int height = 8;
    int width = 50;
    int start_y = (rows - height) / 2;
    int start_x = (cols - width) / 2;

    WINDOW *win = newwin(height, width, start_y, start_x);
    keypad(win, TRUE);
    
    box(win, 0, 0); 

    wattron(win, COLOR_PAIR(3)); 
    mvwprintw(win, 1, 2, ESC_HINT);
    wattroff(win, COLOR_PAIR(3)); 

    // 4. Draw Content
    wattron(win, A_BOLD);
    mvwprintw(win, 1, (width - 14) / 2, " CONFIRMATION "); // 14 here is the length of the string passed
    wattroff(win, A_BOLD);

    print_qr(win, query_msg, width);

    // Drawing "Buttons"
    int btn_y = 6;
    
    wattron(win, COLOR_PAIR(3));
    mvwaddstr(win, btn_y, (width / 2) - 10, "[Y]es");
    wattroff(win, COLOR_PAIR(3));

    mvwaddstr(win, btn_y, (width / 2) + 5, "[N]o");

    wrefresh(win);

    // 5. Input Loop
    bool result = false;
    while (1) {
        int ch = wgetch(win);
        if (ch == 'y' || ch == 'Y') {
            result = true;
            break;
        } else if (ch == 'n' || ch == 'N' || ch == key_escape) {
            result = false;
            break;
        }
    }

    delwin(win);
    return result;
}

static void categories_dashboard(Category *categories, int *category_count, int *chosen)
{
    erase(); 
    refresh();
    int highlight = 0;

    int row, col;
    getmaxyx(stdscr, row, col);
    int y = row / 2;

    static const char *bar_items[] = {
        "[a] Add",
        "[d] Delete",
        "[Esc] Back"
    };

    int bar_count = sizeof(bar_items) / sizeof(bar_items[0]);

    while(1) {
        char dashboard_buff[] = "CATEGORIES DASHBOARD";
        int dashb_buff_len = strlen(dashboard_buff);

        attron(A_BOLD);
        mvaddstr(y - 2, (col - dashb_buff_len) / 2, dashboard_buff);
        attroff(A_BOLD);

        action_bar(bar_items, bar_count);

        if(*category_count == 0) {
            char buff[] = "No categories";
            mvaddstr(y, (col - strlen(buff)) / 2, buff);
        }

        for(int i = 0; i < *category_count; i++) {
            print_category_item(categories[i].name, y + i, (col - dashb_buff_len) / 2, i == highlight);
        }

        int key;
        key = getch();
        switch(key) {
        case CMD_CREATE:
            add_category(categories, category_count);
            break;
        case CMD_DELETE:
            if(*category_count > 0
                    && confirm_action(print_delete_query, categories[highlight].name))     
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

static void print_history_item(Interval *interval,
        Category *categories,
        int category_count,
        int y, int x,
        bool highlighted)
{
    time_t time_focused = interval->end - interval->start;

    struct tm *start = localtime(&interval->start);
    int start_h = start->tm_hour;
    int start_m = start->tm_min;
    int start_month = start->tm_mon + 1; 
    int start_day = start->tm_mday;
    int start_year = start->tm_year + 1900;

    struct tm *end = localtime(&interval->end);
    int end_h = end->tm_hour;
    int end_m = end->tm_min;

    int minutes_focused = time_focused / 60;
    int seconds_focused = time_focused % 60;

    char category_name[name_max_length] = {0};

    if(!highlighted) attron(COLOR_PAIR(3));
    if(interval->category_idx >= category_count)
        strncpy(category_name, "[DELETED]", name_max_length);
    else
        strncpy(category_name, categories[interval->category_idx].name, name_max_length);
    mvprintw(y, x, "%c %s: [%02d/%02d/%d]%02d:%02d-%02d:%02d(%02dm%02ds)",
            highlighted ? '>' : '-',
            category_name,
            start_day, start_month, start_year,
            start_h, start_m,
            end_h, end_m, minutes_focused,
            seconds_focused);
    if(!highlighted) attroff(COLOR_PAIR(3));
}

static void history_dashboard(Interval *intervals,
        Category *categories, 
        int *interval_count,
        int category_count)
{
    timeout(-1);
    erase();
    refresh();

    int r, c;
    getmaxyx(stdscr, r, c);
    int start_y = (r - *interval_count) / 2;
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
            print_history_item(&intervals[i],
                    categories,
                    category_count,
                    start_y + i, start_x,
                    i == highlight);

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
            if(*interval_count > 0)
                highlight = (highlight - 1 + *interval_count) %
                    *interval_count;
            break;
        case 'j':
        case KEY_DOWN:
            if(*interval_count > 0)
                highlight = (highlight + 1) % *interval_count;
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
    int mes_len = snprintf(message, sizeof(message),
            "Cannot have more than %d intervals.",
            max_intervals);
    if(*interval_count >= max_intervals) {
        mvprintw(rows / 2, (cols - mes_len) / 2, "%s", message);
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

static int get_week_total_time(Interval *intervals, Category *categories, int interval_count, int monday)
{
    int total = 0;
    for(int i = 0; i < interval_count; i++) {
        int interval_start_day = localtime(&intervals[i].start)->tm_yday;
        if(interval_start_day >= monday && interval_start_day <= monday + days_in_week - 1) {
            total += intervals[i].end - intervals[i].start;
        }
    }
    return total;
}

typedef int (*get_total_func)(Interval*, Category*, int, int);
typedef void(*update_time)(struct tm*, struct tm*, int);
typedef int(*get_total_target)(struct tm*);
typedef void(*display_date_line)(struct tm*, int, int);
typedef int(*get_interval_target)(Interval *interval);

static int get_interval_day(Interval *interval)
{
    return localtime(&interval->start)->tm_yday; 
}

static int get_interval_month(Interval *interval)
{
    return localtime(&interval->start)->tm_mon; 
}

static int get_interval_year(Interval *interval)
{
    return localtime(&interval->start)->tm_year; 
}

static void get_distribution(Interval *intervals,
        Category *categories,
        int interval_count,
        int category_count,
        int target,
        struct tm dynamic_t,
        int y, int x,
        get_interval_target get_target)
{
    typedef struct Pair {
        char category[name_max_length];
        int total;
    } Pair;

    Pair pairs[category_count];

    // Copy category names
    for(int i = 0; i < category_count; i++) {
        strncpy(pairs[i].category, categories[i].name, name_max_length);
        pairs[i].total = 0;
    }

    // Find total for each category in a given timeline
    for(int i = 0; i < interval_count; i++) {
        Interval *interval = &intervals[i];
        int interval_timeline = get_target(interval);
        mvprintw(0, 0, "TARGET HERE IS: %d", target);
        mvprintw(1, 0, "TIMELINE HERE IS: %d", interval_timeline);
        if(interval_timeline == target)
            pairs[interval->category_idx].total += interval->end - interval->start;
    }

    // Print "Category: total_mins/total_secs
    for(int i = 0; i < category_count; i++) {
        if(pairs[i].total <= 0)
            continue;
        int mins_focused = pairs[i].total / seconds_in_minute;
        int secs_focused = pairs[i].total % seconds_in_minute;

        attron(COLOR_PAIR(3));
        mvprintw(y + i, x, "%s: %02dm%02ds", pairs[i].category, mins_focused, secs_focused);
        attroff(COLOR_PAIR(3));
    }
}

static int get_yday(struct tm *dynamic_t) { return dynamic_t->tm_yday; }

static int get_week_monday(struct tm *dynamic_t)
{
    int days_since_monday = (dynamic_t->tm_wday + days_in_week - 1) % days_in_week;
    struct tm monday = *dynamic_t;
    monday.tm_mday -= days_since_monday;
    mktime(&monday); 
    return monday.tm_yday;
}

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

static void update_week(struct tm *dynamic_t, struct tm *t, int step)
{
    dynamic_t->tm_mday += step * days_in_week;
    mktime(dynamic_t);
    if(dynamic_t->tm_yday > t->tm_yday 
            && dynamic_t->tm_year == t->tm_year)
        dynamic_t->tm_mday -= step * days_in_week;
}

static void display_year_line(struct tm *dynamic_t, int y, int col)
{
    char buff[50];
    int len = snprintf(buff, sizeof(buff), "<- %d ->", dynamic_t->tm_year + 1900);
    attron(COLOR_PAIR(3));
    mvaddstr(y, (col - len) / 2, buff);
    attroff(COLOR_PAIR(3));
}

static void display_month_line(struct tm *dynamic_t, int y, int col)
{
    char buff[50];
    int len = snprintf(buff, sizeof(buff), "<- %02d/%d ->", dynamic_t->tm_mon + 1, dynamic_t->tm_year + 1900);
    attron(COLOR_PAIR(3));
    mvaddstr(y, (col - len) / 2, buff);
    attroff(COLOR_PAIR(3));
}

static void display_day_line(struct tm *dynamic_t, int y, int col)
{
    char buff[50];
    int len = snprintf(buff, sizeof(buff), "<- %02d/%02d/%d ->", dynamic_t->tm_mday,
            dynamic_t->tm_mon + 1, dynamic_t->tm_year + 1900);
    attron(COLOR_PAIR(3));
    mvaddstr(y, (col - len) / 2, buff);
    attroff(COLOR_PAIR(3));
}

static void display_week_line(struct tm *dynamic_t, int y, int col)
{
    int days_since_monday = (dynamic_t->tm_wday + days_in_week - 1) % days_in_week;
    int days_until_sunday = (days_in_week - dynamic_t->tm_wday) % days_in_week;

    struct tm monday = *dynamic_t;
    struct tm sunday = *dynamic_t;

    monday.tm_mday -= days_since_monday;
    sunday.tm_mday += days_until_sunday;
    mktime(&sunday);
    mktime(&monday);

    char buff[50];
    int len = snprintf(buff, sizeof(buff),"<- %02d/%02d-%02d/%02d ->",
            monday.tm_mday, monday.tm_mon + 1, sunday.tm_mday,
            sunday.tm_mon + 1);
    attron(COLOR_PAIR(3));
    mvaddstr(y, (col - len) / 2, buff);
    attroff(COLOR_PAIR(3));
}

static void stats(
        Interval *intervals,
        Category *categories,
        int interval_count,
        int category_count,
        const char *title,
        display_date_line display_line,
        get_total_target get_target,
        get_interval_target get_int_target,
        update_time update_tm,
        get_total_func get_total)
{
    erase();
    refresh();
    time_t now = time(NULL);
    struct tm t = *localtime(&now);
    struct tm dynamic_t = t;

    int row, col;
    getmaxyx(stdscr, row, col);
    int y = (row - bar_height) / 2;

    while(1) {
        mktime(&dynamic_t);
        int total = get_total(intervals, categories,
                interval_count, get_target(&dynamic_t));

        char stats_buff[100]; // Plenty of space for string
        int buff_len = snprintf(stats_buff, sizeof(stats_buff),
                "%s STATS", title);

        attron(A_BOLD);
        mvaddstr(y, (col - buff_len) / 2, stats_buff);
        attroff(A_BOLD);

        display_line(&dynamic_t, y + 2, col);

        char total_buff[50];
        int total_buff_len = snprintf(total_buff,
                sizeof(total_buff),
                "Total: %02dm%02ds",
                total / minutes_in_hour,
                total % minutes_in_hour);
        attron(COLOR_PAIR(9));
        mvaddstr(y + 4, (col - total_buff_len) / 2, total_buff);
        attroff(COLOR_PAIR(9));

        get_distribution(
                intervals,
                categories,
                interval_count,
                category_count,
                get_target(&dynamic_t),
                dynamic_t, 
                y + 6, (col - total_buff_len) / 2,
                get_int_target);
        
        refresh();

        int key = getch();
        switch(key) {
            case 'l':
            case KEY_RIGHT:
                update_tm(&dynamic_t, &t, 1);
                break;
            case 'h':
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

static void statistics_screen(
        Interval *intervals, Category *categories,
        int interval_count, int category_count)
{
    erase();
    refresh();

    static const char *bar_items[] = {
        "[d] Day",
        "[m] Month",
        "[y] Year",
        "[w] Week",
        "[Esc] Exit"
    };

    int row, col;
    getmaxyx(stdscr, row, col);
    int start_y = (row - bar_height) / 2;

    const char month_title[] = "MONTH";
    const char year_title[] = "YEAR";
    const char week_title[] = "WEEK";

    int bar_count = sizeof(bar_items) / sizeof(bar_items[0]);

    while(1) {
        const char stats_screen_buff[] = "STATISTICS SCREEN";
        int buff_len = strlen(stats_screen_buff);

        attron(A_BOLD);
        mvaddstr(start_y, (col - buff_len) / 2, stats_screen_buff);
        attroff(A_BOLD);

        action_bar(bar_items, bar_count);
        
        int key = getch();
        switch(key) {
            case 'd':
                stats(intervals, categories,
                        interval_count, category_count,
                        DAY_TITLE, display_day_line,
                        get_yday, get_interval_day,
                        update_day, get_day_total_time);
                break;
            case 'm':
                stats(intervals, categories,
                        interval_count, category_count,
                        month_title, display_month_line,
                        get_month, get_interval_month,
                        update_month, get_month_total_time);
                break;
            case 'y':
                stats(intervals, categories,
                        interval_count, category_count,
                        year_title, display_year_line,
                        get_year, get_interval_year,
                        update_year, get_year_total_time);
                break;
            case 'w':
                stats(intervals, categories,
                        interval_count, category_count,
                        week_title, display_week_line,
                        get_week_monday, get_interval_day,
                        update_week, get_week_total_time);
                break;
            case key_escape:
                erase();
                refresh();
                return;
        }
    }
}

static void active_screen(Interval *interval,
        Category *categories,
        int *interval_count)
{
    erase();
    refresh();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int height = 7;
    int width = name_max_length + 3; // +2 for padding
    int start_y = (rows - height) / 2;
    int start_x = (cols - width) / 2;

    const char stop_msg[] = "stop";
    const char giveup_msg[] = "give up";

    WINDOW *win = newwin(height, width, start_y, start_x);
    keypad(win, TRUE);
    
    box(win, 0, 0); 
    
    while(1) {
        print_time(win, interval, categories, height, width);
        interval->end = time(NULL);

        int key = getch();
        if(key == key_escape || key == 'q') {
            if(interval->end - interval->start < min_time) {
                if(confirm_action(print_exit_query, giveup_msg)) {
                    (*interval_count)--; // Deleting
                    delwin(win);
                    return;
                }
            } else if(confirm_action(print_exit_query, stop_msg)) {
                delwin(win);
                return;
            }
            erase();
            refresh();
        }
    }
}

static void main_screen(Interval *intervals,
        int *interval_count,
        Category *categories,
        int *category_count)
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

    int row, col;
    getmaxyx(stdscr, row, col);

    int start_y = (row - bar_height) / 2;

    time_t now = time(NULL);

    while(1) {
        char main_screen_buffer[] = "MAIN SCREEN";
        attron(A_BOLD);
        mvaddstr(start_y - 2,
                (col - strlen(main_screen_buffer)) / 2,
                main_screen_buffer);
        attroff(A_BOLD);

        int day_total = get_day_total_time(intervals,
                categories,
                *interval_count,
                localtime(&now)->tm_yday);
        int mins_total = day_total / seconds_in_minute; 
        int secs_total = day_total % seconds_in_minute;

        char buffer[100]; // Plenty of space for formatted message
        int buff_len = snprintf(buffer, sizeof(buffer),
                "You have focused for %dm%ds today",
                mins_total, secs_total);

        mvaddstr(start_y + 2, (col - buff_len) / 2, buffer);

        action_bar(bar_items, bar_count);

        int key = getch();
        switch(key) {
        case CMD_START:
            if(start_interval(intervals, interval_count, 
                        categories, category_count)) {
                active_screen
                    (&intervals[*interval_count - 1],
                     categories,
                     interval_count);
                erase();
                refresh();
            }
            break;
        case CMD_CATEGORY:
            int option;
            categories_dashboard(categories, category_count, &option);
            break;
        case CMD_HISTORY:
            history_dashboard(intervals, categories,
                    interval_count, *category_count);
            break;
        case CMD_STATS:
            statistics_screen(intervals, categories, *interval_count, *category_count);
            break;
        case key_escape:
            push(categories, sizeof(Category),
                    *category_count, CATEGORIES_FILE);
            push(intervals, sizeof(Interval),
                    *interval_count, INTERVALS_FILE);
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
