# Time Tracker

A terminal-based time tracking application built in C with ncurses. Track focused work sessions across different categories, view detailed statistics, and analyze your productivity patterns over time.

## Features

### Core Functionality
- **Session Tracking**: Start and stop timed work sessions with a live countdown timer
- **Category Management**: Organize sessions into up to 5 custom categories
- **Automatic Time Limits**: Sessions automatically end after 2 hours (configurable)
- **Minimum Session Time**: Sessions under 5 minutes can be discarded as false starts

### History & Analysis
- **Session History**: View all tracked sessions with date, time, duration, and category
- **Sorting Options**: Sort history by date or duration, with ascending/descending order
- **Scrollable Interface**: Navigate through large session histories with keyboard shortcuts
- **Session Deletion**: Remove unwanted entries from your history

### Statistics Dashboard
- **Multiple Time Views**: View statistics by day, week, month, or year
- **Time Navigation**: Browse through past and future time periods
- **Category Distribution**: See time breakdown by category for any period
- **Total Time Calculations**: Automatic summation of focused time per period

### Data Management
- **Binary Storage**: Fast, efficient data storage in binary format
- **Data Validation**: Automatic detection and correction of corrupted data
- **Forest Import**: One-time import from Forest app CSV export (optional)
- **Persistent Categories**: Categories are saved between sessions

## Requirements

- GCC or compatible C compiler
- ncurses library (`libncurses-dev` or `ncurses-devel`)
- POSIX-compliant system (Linux, macOS, BSD)

## Installation

### Linux (Debian/Ubuntu)
```bash
# Install ncurses development library
sudo apt-get install libncurses-dev

# Compile the application
gcc -o tm_tracker tm_tracker.c -lncurses

# Run the application
./tm_tracker
```

### Linux (Fedora/RHEL)
```bash
# Install ncurses development library
sudo dnf install ncurses-devel

# Compile the application
gcc -o tm_tracker tm_tracker.c -lncurses

# Run the application
./tm_tracker
```

### macOS
```bash
# ncurses is pre-installed on macOS
# Compile the application
gcc -o tm_tracker tm_tracker.c -lncurses

# Run the application
./tm_tracker
```

## Usage

### Main Screen
The main screen displays your total focused time for the current day and provides access to all features.

**Keyboard Shortcuts:**
- `s` - Start a new tracking session
- `c` - Manage categories
- `h` - View session history
- `t` - View statistics
- `Esc` - Exit application (data is automatically saved)

### Starting a Session
1. Press `s` on the main screen
2. Select a category from the list (use arrow keys and Enter)
3. The timer starts automatically
4. Press `Esc` to stop the session
5. Confirm whether to save or discard the session

### Managing Categories
1. Press `c` on the main screen
2. Navigate with arrow keys (`j`/`k` or Up/Down)
3. `a` - Add a new category
4. `d` - Delete selected category
5. `Esc` - Return to main screen

**Note:** Deleting a category does not delete associated sessions. Those sessions will be marked as "[Deleted]" in the history.

### Viewing History
1. Press `h` on the main screen
2. Navigate through sessions:
   - `j`/`k` or Up/Down arrows - Move one item
   - `Space` - Page down (20 items)
   - `Backspace` - Page up (20 items)
3. `s` - Change sort method (Date/Duration)
4. `r` - Reverse sort order (ascending/descending)
5. `d` - Delete highlighted session
6. `Esc` - Return to main screen

### Statistics
1. Press `t` on the main screen
2. Select time period:
   - `d` - Day view
   - `w` - Week view
   - `m` - Month view
   - `y` - Year view
3. Navigate through time:
   - `h`/`l` or Left/Right arrows - Move to previous/next period
4. View total time and category distribution
5. `Esc` - Return to statistics menu
6. `Esc` again - Return to main screen

## Data Storage

All data is stored in your home directory with hidden files:

- `.categories.dat` - Category definitions
- `.intervals.dat` - Session tracking data
- `.forest_imported` - Flag file to prevent duplicate Forest imports

The application stores up to 5,000 sessions and supports 5 categories maximum. These limits can be modified by changing the constants in the source code and recompiling.

## Forest App Import

If you have data from the Forest app, you can import it on first run:

1. Export your Forest data as CSV
2. Place the file named `.forest.csv` in your home directory
3. Run the time tracker
4. Data will be automatically imported once (if you have fewer than 400 sessions)
5. The import will not run again after the first successful import

## Configuration

Key settings can be modified by editing the source code constants and recompiling:

```c
max_categories = 5        // Maximum number of categories
max_intervals = 5000      // Maximum number of sessions to store
max_time = 120            // Maximum session length in minutes
min_time = 300            // Minimum session length in seconds
visible_rows = 20         // Number of history items visible at once
```

## Color Support

The application automatically detects terminal color capabilities:
- **256-color terminals**: Full color scheme with dimmed accents
- **8/16-color terminals**: Fallback color scheme for compatibility

## Limitations

- Maximum 5 categories
- Maximum 5,000 sessions
- Session duration capped at 2 hours
- Sessions under 5 minutes must be confirmed to save
- Category names limited to 30 characters

## Technical Details

- Written in C99
- Uses ncurses for terminal UI
- Binary data format for fast I/O
- Data validation on every load
- Automatic handling of deleted categories
- Time calculations handle year boundaries correctly

## Troubleshooting

**Problem: Colors don't display correctly**
- Check terminal color support with `echo $TERM`
- Try running in a terminal with 256-color support (xterm-256color)

**Problem: Data appears corrupted**
- The application automatically validates and cleans data on load
- Corrupted sessions are silently discarded
- If problems persist, delete `.intervals.dat` to reset

**Problem: Forest import not working**
- Ensure the file is named exactly `.forest.csv`
- Place it in your home directory
- Check that you have fewer than 400 sessions
- Delete `.forest_imported` file to retry import

**Problem: Application won't compile**
- Verify ncurses library is installed
- Check compiler flags include `-lncurses`
- Ensure you have GCC or compatible C compiler

## License

This is personal project code provided as-is without warranty. Feel free to use, modify, and distribute as needed.

## Future Enhancements

Potential features for future development:
- Export to CSV for external analysis
- Configurable session time limits
- Pause/resume functionality
- Session notes
- Daily goals and streak tracking
- Multi-file organization (splitting code into modules)
- Configuration file support

---

Built with C and ncurses for efficient terminal-based time tracking.
