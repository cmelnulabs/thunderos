/**
 * Framebuffer Console
 * 
 * Text console rendered on the graphical framebuffer.
 * Provides a terminal-like interface with scrolling and cursor support.
 */

#ifndef FBCONSOLE_H
#define FBCONSOLE_H

#include <stdint.h>
#include <stddef.h>

/* Console dimensions (calculated from framebuffer and font size) */
#define FBCON_MAX_COLS      100     /* Maximum columns (800/8) */
#define FBCON_MAX_ROWS      37      /* Maximum rows (600/16) */

/* Default console colors */
#define FBCON_FG_DEFAULT    0xFFAAAAAA  /* Light gray */
#define FBCON_BG_DEFAULT    0xFF000000  /* Black */

/* ANSI color codes (indices 0-15) */
typedef enum {
    FBCON_COLOR_BLACK = 0,
    FBCON_COLOR_RED,
    FBCON_COLOR_GREEN,
    FBCON_COLOR_YELLOW,
    FBCON_COLOR_BLUE,
    FBCON_COLOR_MAGENTA,
    FBCON_COLOR_CYAN,
    FBCON_COLOR_WHITE,
    FBCON_COLOR_BRIGHT_BLACK,
    FBCON_COLOR_BRIGHT_RED,
    FBCON_COLOR_BRIGHT_GREEN,
    FBCON_COLOR_BRIGHT_YELLOW,
    FBCON_COLOR_BRIGHT_BLUE,
    FBCON_COLOR_BRIGHT_MAGENTA,
    FBCON_COLOR_BRIGHT_CYAN,
    FBCON_COLOR_BRIGHT_WHITE
} fbcon_color_t;

/**
 * Initialize the framebuffer console
 * 
 * @return 0 on success, -1 on error
 */
int fbcon_init(void);

/**
 * Check if framebuffer console is available
 * 
 * @return 1 if available, 0 otherwise
 */
int fbcon_available(void);

/**
 * Clear the console screen
 */
void fbcon_clear(void);

/**
 * Write a single character to the console
 * 
 * @param c Character to write
 */
void fbcon_putc(char c);

/**
 * Write a string to the console
 * 
 * @param str String to write
 */
void fbcon_puts(const char *str);

/**
 * Write a character with specific colors
 * 
 * @param c Character to write
 * @param fg Foreground color (ARGB)
 * @param bg Background color (ARGB)
 */
void fbcon_putc_color(char c, uint32_t fg, uint32_t bg);

/**
 * Set the current foreground color
 * 
 * @param color ARGB color value
 */
void fbcon_set_fg(uint32_t color);

/**
 * Set the current background color
 * 
 * @param color ARGB color value
 */
void fbcon_set_bg(uint32_t color);

/**
 * Set colors from ANSI color codes
 * 
 * @param fg Foreground color index (0-15)
 * @param bg Background color index (0-15)
 */
void fbcon_set_colors(fbcon_color_t fg, fbcon_color_t bg);

/**
 * Reset colors to default
 */
void fbcon_reset_colors(void);

/**
 * Get the cursor position
 * 
 * @param col Pointer to store column (can be NULL)
 * @param row Pointer to store row (can be NULL)
 */
void fbcon_get_cursor(uint32_t *col, uint32_t *row);

/**
 * Set the cursor position
 * 
 * @param col Column (0-based)
 * @param row Row (0-based)
 */
void fbcon_set_cursor(uint32_t col, uint32_t row);

/**
 * Show or hide the cursor
 * 
 * @param visible 1 to show, 0 to hide
 */
void fbcon_cursor_visible(int visible);

/**
 * Scroll the console up by one line
 */
void fbcon_scroll_up(void);

/**
 * Flush any pending updates to the display
 */
void fbcon_flush(void);

/**
 * Get console dimensions
 * 
 * @param cols Pointer to store columns (can be NULL)
 * @param rows Pointer to store rows (can be NULL)
 */
void fbcon_get_size(uint32_t *cols, uint32_t *rows);

/**
 * Get ARGB color from color index
 * 
 * @param color Color index (0-15)
 * @return ARGB color value
 */
uint32_t fbcon_get_color(fbcon_color_t color);

#endif /* FBCONSOLE_H */
