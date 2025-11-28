/*
 * Framebuffer Console
 * 
 * Text console rendered on the graphical framebuffer.
 */

#include <drivers/fbconsole.h>
#include <drivers/framebuffer.h>
#include <drivers/font.h>
#include <kernel/errno.h>
#include <stddef.h>

/* Console state */
static struct {
    int initialized;
    uint32_t cols;          /* Number of text columns */
    uint32_t rows;          /* Number of text rows */
    uint32_t cursor_col;    /* Current cursor column */
    uint32_t cursor_row;    /* Current cursor row */
    uint32_t fg_color;      /* Current foreground color */
    uint32_t bg_color;      /* Current background color */
    int cursor_visible;     /* Cursor visibility flag */
    int dirty;              /* Screen needs flush */
} g_fbcon = {0};

/* ANSI color palette (16 colors) */
static const uint32_t ansi_colors[16] = {
    0xFF000000,  /* 0:  Black */
    0xFFAA0000,  /* 1:  Red */
    0xFF00AA00,  /* 2:  Green */
    0xFFAA5500,  /* 3:  Yellow/Brown */
    0xFF0000AA,  /* 4:  Blue */
    0xFFAA00AA,  /* 5:  Magenta */
    0xFF00AAAA,  /* 6:  Cyan */
    0xFFAAAAAA,  /* 7:  White (light gray) */
    0xFF555555,  /* 8:  Bright Black (dark gray) */
    0xFFFF5555,  /* 9:  Bright Red */
    0xFF55FF55,  /* 10: Bright Green */
    0xFFFFFF55,  /* 11: Bright Yellow */
    0xFF5555FF,  /* 12: Bright Blue */
    0xFFFF55FF,  /* 13: Bright Magenta */
    0xFF55FFFF,  /* 14: Bright Cyan */
    0xFFFFFFFF,  /* 15: Bright White */
};

/**
 * Draw cursor at current position
 */
static void draw_cursor(int show)
{
    if (!g_fbcon.initialized) return;
    
    uint32_t x = g_fbcon.cursor_col * FONT_WIDTH;
    uint32_t y = g_fbcon.cursor_row * FONT_HEIGHT;
    
    /* Draw cursor as an underscore on the last two rows of the character cell */
    uint32_t color = show ? g_fbcon.fg_color : g_fbcon.bg_color;
    for (uint32_t row = FONT_HEIGHT - 2; row < FONT_HEIGHT; row++) {
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            fb_set_pixel(x + col, y + row, color);
        }
    }
}

/**
 * Initialize the framebuffer console
 */
int fbcon_init(void)
{
    if (g_fbcon.initialized) {
        clear_errno();
        return 0;
    }
    
    /* Check if framebuffer is available */
    if (!fb_available()) {
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    /* Get framebuffer info */
    fb_info_t info;
    if (fb_get_info(&info) < 0) {
        return -1;
    }
    
    /* Calculate console dimensions */
    g_fbcon.cols = info.width / FONT_WIDTH;
    g_fbcon.rows = info.height / FONT_HEIGHT;
    
    /* Limit to max dimensions */
    if (g_fbcon.cols > FBCON_MAX_COLS) g_fbcon.cols = FBCON_MAX_COLS;
    if (g_fbcon.rows > FBCON_MAX_ROWS) g_fbcon.rows = FBCON_MAX_ROWS;
    
    /* Initialize state */
    g_fbcon.cursor_col = 0;
    g_fbcon.cursor_row = 0;
    g_fbcon.fg_color = FBCON_FG_DEFAULT;
    g_fbcon.bg_color = FBCON_BG_DEFAULT;
    g_fbcon.cursor_visible = 1;
    g_fbcon.dirty = 0;
    g_fbcon.initialized = 1;
    
    /* Clear screen */
    fbcon_clear();
    
    clear_errno();
    return 0;
}

/**
 * Check if framebuffer console is available
 */
int fbcon_available(void)
{
    return g_fbcon.initialized;
}

/**
 * Clear the console screen
 */
void fbcon_clear(void)
{
    if (!g_fbcon.initialized) return;
    
    fb_clear(g_fbcon.bg_color);
    g_fbcon.cursor_col = 0;
    g_fbcon.cursor_row = 0;
    g_fbcon.dirty = 1;
    
    if (g_fbcon.cursor_visible) {
        draw_cursor(1);
    }
}

/**
 * Scroll the console up by one line
 */
void fbcon_scroll_up(void)
{
    if (!g_fbcon.initialized) return;
    
    fb_info_t info;
    if (fb_get_info(&info) < 0) return;
    
    uint32_t *pixels = info.pixels;
    if (!pixels) return;
    
    uint32_t row_pixels = info.width * FONT_HEIGHT;
    uint32_t console_height = g_fbcon.rows * FONT_HEIGHT;
    
    /* Copy each row up */
    for (uint32_t y = 0; y < console_height - FONT_HEIGHT; y++) {
        for (uint32_t x = 0; x < info.width; x++) {
            pixels[y * info.width + x] = pixels[(y + FONT_HEIGHT) * info.width + x];
        }
    }
    
    /* Clear the last row */
    uint32_t last_row_start = (console_height - FONT_HEIGHT) * info.width;
    for (uint32_t i = 0; i < row_pixels; i++) {
        pixels[last_row_start + i] = g_fbcon.bg_color;
    }
    
    g_fbcon.dirty = 1;
}

/**
 * Handle newline
 */
static void newline(void)
{
    /* Hide cursor before moving */
    if (g_fbcon.cursor_visible) {
        draw_cursor(0);
    }
    
    g_fbcon.cursor_col = 0;
    g_fbcon.cursor_row++;
    
    if (g_fbcon.cursor_row >= g_fbcon.rows) {
        fbcon_scroll_up();
        g_fbcon.cursor_row = g_fbcon.rows - 1;
    }
    
    /* Show cursor at new position */
    if (g_fbcon.cursor_visible) {
        draw_cursor(1);
    }
}

/**
 * Handle carriage return
 */
static void carriage_return(void)
{
    if (g_fbcon.cursor_visible) {
        draw_cursor(0);
    }
    g_fbcon.cursor_col = 0;
    if (g_fbcon.cursor_visible) {
        draw_cursor(1);
    }
}

/**
 * Handle backspace
 */
static void backspace(void)
{
    if (g_fbcon.cursor_col > 0) {
        if (g_fbcon.cursor_visible) {
            draw_cursor(0);
        }
        g_fbcon.cursor_col--;
        
        /* Clear the character cell */
        uint32_t x = g_fbcon.cursor_col * FONT_WIDTH;
        uint32_t y = g_fbcon.cursor_row * FONT_HEIGHT;
        fb_fill_rect(x, y, FONT_WIDTH, FONT_HEIGHT, g_fbcon.bg_color);
        
        if (g_fbcon.cursor_visible) {
            draw_cursor(1);
        }
        g_fbcon.dirty = 1;
    }
}

/**
 * Handle tab
 */
static void tab(void)
{
    /* Tab to next 8-column boundary */
    uint32_t next_tab = ((g_fbcon.cursor_col / 8) + 1) * 8;
    while (g_fbcon.cursor_col < next_tab && g_fbcon.cursor_col < g_fbcon.cols) {
        fbcon_putc(' ');
    }
}

/**
 * Write a single character to the console
 */
void fbcon_putc(char c)
{
    if (!g_fbcon.initialized) return;
    
    /* Handle control characters */
    switch (c) {
    case '\n':
        newline();
        return;
    case '\r':
        carriage_return();
        return;
    case '\b':
        backspace();
        return;
    case '\t':
        tab();
        return;
    case '\0':
        return;
    }
    
    /* Skip non-printable characters */
    if (c < 0x20 || c > 0x7E) {
        return;
    }
    
    /* Hide cursor before drawing */
    if (g_fbcon.cursor_visible) {
        draw_cursor(0);
    }
    
    /* Draw the character */
    uint32_t x = g_fbcon.cursor_col * FONT_WIDTH;
    uint32_t y = g_fbcon.cursor_row * FONT_HEIGHT;
    font_draw_char(x, y, c, g_fbcon.fg_color, g_fbcon.bg_color);
    g_fbcon.dirty = 1;
    
    /* Advance cursor */
    g_fbcon.cursor_col++;
    if (g_fbcon.cursor_col >= g_fbcon.cols) {
        newline();
    }
    
    /* Show cursor at new position */
    if (g_fbcon.cursor_visible) {
        draw_cursor(1);
    }
}

/**
 * Write a string to the console
 */
void fbcon_puts(const char *str)
{
    if (!str) return;
    
    while (*str) {
        fbcon_putc(*str++);
    }
}

/**
 * Write a character with specific colors
 */
void fbcon_putc_color(char c, uint32_t fg, uint32_t bg)
{
    uint32_t old_fg = g_fbcon.fg_color;
    uint32_t old_bg = g_fbcon.bg_color;
    
    g_fbcon.fg_color = fg;
    g_fbcon.bg_color = bg;
    
    fbcon_putc(c);
    
    g_fbcon.fg_color = old_fg;
    g_fbcon.bg_color = old_bg;
}

/**
 * Set the current foreground color
 */
void fbcon_set_fg(uint32_t color)
{
    g_fbcon.fg_color = color;
}

/**
 * Set the current background color
 */
void fbcon_set_bg(uint32_t color)
{
    g_fbcon.bg_color = color;
}

/**
 * Set colors from ANSI color codes
 */
void fbcon_set_colors(fbcon_color_t fg, fbcon_color_t bg)
{
    if (fg < 16) g_fbcon.fg_color = ansi_colors[fg];
    if (bg < 16) g_fbcon.bg_color = ansi_colors[bg];
}

/**
 * Reset colors to default
 */
void fbcon_reset_colors(void)
{
    g_fbcon.fg_color = FBCON_FG_DEFAULT;
    g_fbcon.bg_color = FBCON_BG_DEFAULT;
}

/**
 * Get the cursor position
 */
void fbcon_get_cursor(uint32_t *col, uint32_t *row)
{
    if (col) *col = g_fbcon.cursor_col;
    if (row) *row = g_fbcon.cursor_row;
}

/**
 * Set the cursor position
 */
void fbcon_set_cursor(uint32_t col, uint32_t row)
{
    if (!g_fbcon.initialized) return;
    
    /* Hide cursor at old position */
    if (g_fbcon.cursor_visible) {
        draw_cursor(0);
    }
    
    /* Clamp to valid range */
    if (col >= g_fbcon.cols) col = g_fbcon.cols - 1;
    if (row >= g_fbcon.rows) row = g_fbcon.rows - 1;
    
    g_fbcon.cursor_col = col;
    g_fbcon.cursor_row = row;
    
    /* Show cursor at new position */
    if (g_fbcon.cursor_visible) {
        draw_cursor(1);
    }
}

/**
 * Show or hide the cursor
 */
void fbcon_cursor_visible(int visible)
{
    if (g_fbcon.cursor_visible == visible) return;
    
    g_fbcon.cursor_visible = visible;
    draw_cursor(visible);
    g_fbcon.dirty = 1;
}

/**
 * Flush any pending updates to the display
 */
void fbcon_flush(void)
{
    if (!g_fbcon.initialized) return;
    if (!g_fbcon.dirty) return;
    
    fb_flush();
    g_fbcon.dirty = 0;
}

/**
 * Get console dimensions
 */
void fbcon_get_size(uint32_t *cols, uint32_t *rows)
{
    if (cols) *cols = g_fbcon.initialized ? g_fbcon.cols : 0;
    if (rows) *rows = g_fbcon.initialized ? g_fbcon.rows : 0;
}

/**
 * Get ARGB color from color index
 */
uint32_t fbcon_get_color(fbcon_color_t color)
{
    if (color >= 16) return FBCON_FG_DEFAULT;
    return ansi_colors[color];
}
