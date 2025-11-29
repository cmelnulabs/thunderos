/**
 * Virtual Terminal Subsystem
 * 
 * Provides multiple virtual terminals with Alt+Fn switching.
 * Each virtual terminal maintains its own screen buffer, cursor
 * position, and colors.
 */

#ifndef VTERM_H
#define VTERM_H

#include <stdint.h>
#include <stddef.h>

/* Maximum number of virtual terminals */
#define VTERM_MAX_TERMINALS     6

/* Virtual terminal dimensions (matches FBCON_MAX_*) */
#define VTERM_MAX_COLS          100
#define VTERM_MAX_ROWS          37

/* Character cell - stores character and attributes */
typedef struct {
    char ch;                /* Character */
    uint8_t fg_color;       /* Foreground color index (0-15) */
    uint8_t bg_color;       /* Background color index (0-15) */
    uint8_t attrs;          /* Attributes (bold, underline, etc.) */
} vterm_cell_t;

/* Character attributes */
#define VTERM_ATTR_NONE         0x00
#define VTERM_ATTR_BOLD         0x01
#define VTERM_ATTR_UNDERLINE    0x02
#define VTERM_ATTR_BLINK        0x04
#define VTERM_ATTR_REVERSE      0x08

/* Virtual terminal state */
typedef struct {
    /* Screen buffer */
    vterm_cell_t buffer[VTERM_MAX_ROWS][VTERM_MAX_COLS];
    
    /* Terminal dimensions */
    uint32_t cols;
    uint32_t rows;
    
    /* Cursor position */
    uint32_t cursor_col;
    uint32_t cursor_row;
    
    /* Current colors */
    uint8_t fg_color;
    uint8_t bg_color;
    uint8_t attrs;
    
    /* Cursor visibility */
    int cursor_visible;
    
    /* Terminal name (for display) */
    char name[16];
    
    /* Is terminal active (has been written to) */
    int active;
} vterm_t;

/* Keyboard input state for escape sequence processing */
typedef struct {
    int in_escape;          /* Currently processing escape sequence */
    int alt_pressed;        /* Alt key is held */
    char escape_buf[8];     /* Buffer for escape sequence */
    int escape_len;         /* Length of escape sequence */
} vterm_input_state_t;

/**
 * Initialize the virtual terminal subsystem
 * 
 * @return 0 on success, -1 on error
 */
int vterm_init(void);

/**
 * Get the currently active virtual terminal
 * 
 * @return Pointer to active terminal, or NULL if not initialized
 */
vterm_t *vterm_get_active(void);

/**
 * Get a specific virtual terminal
 * 
 * @param index Terminal index (0 to VTERM_MAX_TERMINALS-1)
 * @return Pointer to terminal, or NULL if invalid index
 */
vterm_t *vterm_get(int index);

/**
 * Switch to a different virtual terminal
 * 
 * @param index Terminal index to switch to (0 to VTERM_MAX_TERMINALS-1)
 * @return 0 on success, -1 on error
 */
int vterm_switch(int index);

/**
 * Get the index of the currently active terminal
 * 
 * @return Active terminal index (0 to VTERM_MAX_TERMINALS-1)
 */
int vterm_get_active_index(void);

/**
 * Write a character to the active virtual terminal
 * 
 * @param c Character to write
 */
void vterm_putc(char c);

/**
 * Write a string to the active virtual terminal
 * 
 * @param str String to write
 */
void vterm_puts(const char *str);

/**
 * Clear the active virtual terminal
 */
void vterm_clear(void);

/**
 * Set cursor position on active terminal
 * 
 * @param col Column (0-based)
 * @param row Row (0-based)
 */
void vterm_set_cursor(uint32_t col, uint32_t row);

/**
 * Get cursor position on active terminal
 * 
 * @param col Pointer to store column (can be NULL)
 * @param row Pointer to store row (can be NULL)
 */
void vterm_get_cursor(uint32_t *col, uint32_t *row);

/**
 * Set colors on active terminal
 * 
 * @param fg Foreground color index (0-15)
 * @param bg Background color index (0-15)
 */
void vterm_set_colors(uint8_t fg, uint8_t bg);

/**
 * Process keyboard input
 * 
 * Handles escape sequences for terminal switching (Alt+F1..F6).
 * Returns the character if it should be passed to the shell,
 * or 0 if it was consumed by the terminal system.
 * 
 * @param c Character from keyboard/UART
 * @return Character to pass to application, or 0 if consumed
 */
char vterm_process_input(char c);

/**
 * Refresh the display from the active terminal's buffer
 * 
 * Redraws the entire screen from the terminal buffer.
 */
void vterm_refresh(void);

/**
 * Flush pending updates to the display
 */
void vterm_flush(void);

/**
 * Check if virtual terminal system is available
 * 
 * @return 1 if available, 0 if not
 */
int vterm_available(void);

/**
 * Draw the terminal status bar (shows current VT)
 */
void vterm_draw_status_bar(void);

#endif /* VTERM_H */
