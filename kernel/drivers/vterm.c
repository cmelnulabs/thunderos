/*
 * Virtual Terminal Subsystem
 * 
 * Provides multiple virtual terminals with Alt+Fn switching.
 * Each terminal has its own screen buffer that persists when
 * switching between terminals.
 */

#include <drivers/vterm.h>
#include <drivers/fbconsole.h>
#include <drivers/framebuffer.h>
#include <drivers/font.h>
#include <kernel/errno.h>
#include <kernel/kstring.h>
#include <hal/hal_uart.h>
#include <stddef.h>

/* Virtual terminal array */
static vterm_t g_terminals[VTERM_MAX_TERMINALS];

/* Currently active terminal index */
static int g_active_terminal = 0;

/* Input processing state */
static vterm_input_state_t g_input_state = {0};

/* Initialization flag */
static int g_initialized = 0;

/* Default terminal colors */
#define DEFAULT_FG_COLOR    7   /* Light gray */
#define DEFAULT_BG_COLOR    0   /* Black */

/* ANSI color palette (matches fbconsole) */
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

/* Status bar colors */
#define STATUS_BAR_FG       0   /* Black text */
#define STATUS_BAR_BG       7   /* Light gray background */
#define STATUS_BAR_ACTIVE   2   /* Green for active terminal */

/* Forward declarations */
static void vterm_scroll_up(vterm_t *term);
static void vterm_newline(vterm_t *term);
static void vterm_draw_cell(uint32_t col, uint32_t row, vterm_cell_t *cell);

/**
 * Initialize a single terminal
 */
static void init_terminal(vterm_t *term, int index)
{
    /* Set dimensions based on framebuffer */
    if (fbcon_available()) {
        fbcon_get_size(&term->cols, &term->rows);
        /* Reserve one row for status bar */
        if (term->rows > 1) {
            term->rows--;
        }
    } else {
        term->cols = 80;
        term->rows = 24;
    }
    
    /* Clamp to max dimensions */
    if (term->cols > VTERM_MAX_COLS) term->cols = VTERM_MAX_COLS;
    if (term->rows > VTERM_MAX_ROWS - 1) term->rows = VTERM_MAX_ROWS - 1;
    
    /* Clear buffer */
    for (uint32_t row = 0; row < VTERM_MAX_ROWS; row++) {
        for (uint32_t col = 0; col < VTERM_MAX_COLS; col++) {
            term->buffer[row][col].ch = ' ';
            term->buffer[row][col].fg_color = DEFAULT_FG_COLOR;
            term->buffer[row][col].bg_color = DEFAULT_BG_COLOR;
            term->buffer[row][col].attrs = VTERM_ATTR_NONE;
        }
    }
    
    /* Initialize cursor */
    term->cursor_col = 0;
    term->cursor_row = 0;
    term->cursor_visible = 1;
    
    /* Initialize colors */
    term->fg_color = DEFAULT_FG_COLOR;
    term->bg_color = DEFAULT_BG_COLOR;
    term->attrs = VTERM_ATTR_NONE;
    
    /* Set terminal name */
    term->name[0] = 'V';
    term->name[1] = 'T';
    term->name[2] = '1' + index;
    term->name[3] = '\0';
    
    /* Mark as inactive until written to */
    term->active = (index == 0) ? 1 : 0;
}

/**
 * Initialize the virtual terminal subsystem
 */
int vterm_init(void)
{
    if (g_initialized) {
        clear_errno();
        return 0;
    }
    
    /* Check if framebuffer console is available */
    if (!fbcon_available()) {
        /* Fall back to UART-only mode */
        hal_uart_puts("[INFO] Virtual terminals require framebuffer - disabled\n");
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    /* Initialize all terminals */
    for (int i = 0; i < VTERM_MAX_TERMINALS; i++) {
        init_terminal(&g_terminals[i], i);
    }
    
    /* Reset input state */
    g_input_state.in_escape = 0;
    g_input_state.alt_pressed = 0;
    g_input_state.escape_len = 0;
    
    g_active_terminal = 0;
    g_initialized = 1;
    
    /* Draw initial screen */
    vterm_refresh();
    vterm_draw_status_bar();
    vterm_flush();
    
    clear_errno();
    return 0;
}

/**
 * Check if virtual terminal system is available
 */
int vterm_available(void)
{
    return g_initialized;
}

/**
 * Get the currently active virtual terminal
 */
vterm_t *vterm_get_active(void)
{
    if (!g_initialized) return NULL;
    return &g_terminals[g_active_terminal];
}

/**
 * Get a specific virtual terminal
 */
vterm_t *vterm_get(int index)
{
    if (!g_initialized) return NULL;
    if (index < 0 || index >= VTERM_MAX_TERMINALS) return NULL;
    return &g_terminals[index];
}

/**
 * Get the index of the currently active terminal
 */
int vterm_get_active_index(void)
{
    return g_active_terminal;
}

/**
 * Draw a single cell to the framebuffer
 */
static void vterm_draw_cell(uint32_t col, uint32_t row, vterm_cell_t *cell)
{
    if (!fbcon_available()) return;
    
    uint32_t fg = ansi_colors[cell->fg_color & 0x0F];
    uint32_t bg = ansi_colors[cell->bg_color & 0x0F];
    
    /* Handle reverse attribute */
    if (cell->attrs & VTERM_ATTR_REVERSE) {
        uint32_t tmp = fg;
        fg = bg;
        bg = tmp;
    }
    
    uint32_t x = col * FONT_WIDTH;
    uint32_t y = row * FONT_HEIGHT;
    
    font_draw_char(x, y, cell->ch, fg, bg);
}

/**
 * Refresh the display from the active terminal's buffer
 */
void vterm_refresh(void)
{
    if (!g_initialized || !fbcon_available()) return;
    
    vterm_t *term = &g_terminals[g_active_terminal];
    
    /* Redraw all cells */
    for (uint32_t row = 0; row < term->rows; row++) {
        for (uint32_t col = 0; col < term->cols; col++) {
            vterm_draw_cell(col, row, &term->buffer[row][col]);
        }
    }
    
    /* Draw cursor if visible */
    if (term->cursor_visible && term->cursor_row < term->rows) {
        uint32_t x = term->cursor_col * FONT_WIDTH;
        uint32_t y = term->cursor_row * FONT_HEIGHT;
        
        /* Draw underscore cursor */
        uint32_t fg = ansi_colors[term->fg_color];
        for (uint32_t cy = FONT_HEIGHT - 2; cy < FONT_HEIGHT; cy++) {
            for (uint32_t cx = 0; cx < FONT_WIDTH; cx++) {
                fb_set_pixel(x + cx, y + cy, fg);
            }
        }
    }
}

/**
 * Draw the terminal status bar
 */
void vterm_draw_status_bar(void)
{
    if (!g_initialized || !fbcon_available()) return;
    
    vterm_t *term = &g_terminals[g_active_terminal];
    uint32_t status_row = term->rows;  /* Status bar is on the row after content */
    
    uint32_t bg = ansi_colors[STATUS_BAR_BG];
    uint32_t fg = ansi_colors[STATUS_BAR_FG];
    uint32_t active_fg = ansi_colors[STATUS_BAR_ACTIVE];
    
    uint32_t y = status_row * FONT_HEIGHT;
    
    /* Clear the status bar row */
    for (uint32_t col = 0; col < term->cols; col++) {
        uint32_t x = col * FONT_WIDTH;
        fb_fill_rect(x, y, FONT_WIDTH, FONT_HEIGHT, bg);
    }
    
    /* Draw "ThunderOS" at the left */
    const char *title = " ThunderOS ";
    uint32_t x = 0;
    for (const char *p = title; *p; p++) {
        font_draw_char(x, y, *p, fg, bg);
        x += FONT_WIDTH;
    }
    
    /* Draw terminal tabs */
    x += FONT_WIDTH * 2;  /* Add some spacing */
    
    for (int i = 0; i < VTERM_MAX_TERMINALS; i++) {
        vterm_t *t = &g_terminals[i];
        
        /* Only show active or used terminals */
        if (!t->active && i != g_active_terminal) {
            continue;
        }
        
        /* Highlight current terminal */
        uint32_t tab_bg = (i == g_active_terminal) ? active_fg : bg;
        uint32_t tab_fg = (i == g_active_terminal) ? bg : fg;
        
        /* Draw " VTn " */
        font_draw_char(x, y, ' ', tab_fg, tab_bg);
        x += FONT_WIDTH;
        font_draw_char(x, y, 'V', tab_fg, tab_bg);
        x += FONT_WIDTH;
        font_draw_char(x, y, 'T', tab_fg, tab_bg);
        x += FONT_WIDTH;
        font_draw_char(x, y, '1' + i, tab_fg, tab_bg);
        x += FONT_WIDTH;
        font_draw_char(x, y, ' ', tab_fg, tab_bg);
        x += FONT_WIDTH;
        
        /* Space between tabs */
        x += FONT_WIDTH;
    }
    
    /* Draw help text on the right */
    const char *help = "Alt+F1-F6: Switch VT ";
    uint32_t help_len = 0;
    for (const char *p = help; *p; p++) help_len++;
    
    x = (term->cols - help_len) * FONT_WIDTH;
    for (const char *p = help; *p; p++) {
        font_draw_char(x, y, *p, fg, bg);
        x += FONT_WIDTH;
    }
}

/**
 * Switch to a different virtual terminal
 */
int vterm_switch(int index)
{
    if (!g_initialized) {
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    if (index < 0 || index >= VTERM_MAX_TERMINALS) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    if (index == g_active_terminal) {
        clear_errno();
        return 0;
    }
    
    /* Switch to new terminal */
    g_active_terminal = index;
    g_terminals[index].active = 1;
    
    /* Refresh display */
    vterm_refresh();
    vterm_draw_status_bar();
    vterm_flush();
    
    /* Log to UART */
    hal_uart_puts("[VT] Switched to ");
    hal_uart_puts(g_terminals[index].name);
    hal_uart_puts("\n");
    
    clear_errno();
    return 0;
}

/**
 * Scroll the terminal up by one line
 */
static void vterm_scroll_up(vterm_t *term)
{
    /* Move all rows up */
    for (uint32_t row = 0; row < term->rows - 1; row++) {
        for (uint32_t col = 0; col < term->cols; col++) {
            term->buffer[row][col] = term->buffer[row + 1][col];
        }
    }
    
    /* Clear the last row */
    for (uint32_t col = 0; col < term->cols; col++) {
        term->buffer[term->rows - 1][col].ch = ' ';
        term->buffer[term->rows - 1][col].fg_color = term->fg_color;
        term->buffer[term->rows - 1][col].bg_color = term->bg_color;
        term->buffer[term->rows - 1][col].attrs = VTERM_ATTR_NONE;
    }
}

/**
 * Handle newline on terminal
 */
static void vterm_newline(vterm_t *term)
{
    term->cursor_col = 0;
    term->cursor_row++;
    
    if (term->cursor_row >= term->rows) {
        vterm_scroll_up(term);
        term->cursor_row = term->rows - 1;
    }
}

/**
 * Write a character to the active virtual terminal
 */
void vterm_putc(char c)
{
    if (!g_initialized) {
        /* Fall back to fbconsole or uart */
        if (fbcon_available()) {
            fbcon_putc(c);
        } else {
            hal_uart_putc(c);
        }
        return;
    }
    
    vterm_t *term = &g_terminals[g_active_terminal];
    
    /* Handle control characters */
    switch (c) {
    case '\n':
        vterm_newline(term);
        /* Redraw from current cursor position to end */
        if (term == &g_terminals[g_active_terminal]) {
            vterm_refresh();
        }
        return;
        
    case '\r':
        term->cursor_col = 0;
        return;
        
    case '\b':
        if (term->cursor_col > 0) {
            term->cursor_col--;
            /* Clear the cell */
            term->buffer[term->cursor_row][term->cursor_col].ch = ' ';
            /* Redraw */
            if (term == &g_terminals[g_active_terminal]) {
                vterm_draw_cell(term->cursor_col, term->cursor_row, 
                               &term->buffer[term->cursor_row][term->cursor_col]);
            }
        }
        return;
        
    case '\t':
        /* Tab to next 8-column boundary */
        do {
            vterm_putc(' ');
        } while (term->cursor_col % 8 != 0 && term->cursor_col < term->cols);
        return;
        
    case '\0':
        return;
    }
    
    /* Skip non-printable characters */
    if (c < 0x20 || c > 0x7E) {
        return;
    }
    
    /* Store character in buffer */
    term->buffer[term->cursor_row][term->cursor_col].ch = c;
    term->buffer[term->cursor_row][term->cursor_col].fg_color = term->fg_color;
    term->buffer[term->cursor_row][term->cursor_col].bg_color = term->bg_color;
    term->buffer[term->cursor_row][term->cursor_col].attrs = term->attrs;
    
    /* Draw to screen if this is the active terminal */
    if (term == &g_terminals[g_active_terminal]) {
        vterm_draw_cell(term->cursor_col, term->cursor_row,
                       &term->buffer[term->cursor_row][term->cursor_col]);
    }
    
    /* Advance cursor */
    term->cursor_col++;
    if (term->cursor_col >= term->cols) {
        vterm_newline(term);
    }
}

/**
 * Write a string to the active virtual terminal
 */
void vterm_puts(const char *str)
{
    if (!str) return;
    
    while (*str) {
        vterm_putc(*str++);
    }
}

/**
 * Clear the active virtual terminal
 */
void vterm_clear(void)
{
    if (!g_initialized) {
        if (fbcon_available()) {
            fbcon_clear();
        }
        return;
    }
    
    vterm_t *term = &g_terminals[g_active_terminal];
    
    /* Clear buffer */
    for (uint32_t row = 0; row < term->rows; row++) {
        for (uint32_t col = 0; col < term->cols; col++) {
            term->buffer[row][col].ch = ' ';
            term->buffer[row][col].fg_color = term->fg_color;
            term->buffer[row][col].bg_color = term->bg_color;
            term->buffer[row][col].attrs = VTERM_ATTR_NONE;
        }
    }
    
    /* Reset cursor */
    term->cursor_col = 0;
    term->cursor_row = 0;
    
    /* Redraw */
    vterm_refresh();
    vterm_draw_status_bar();
}

/**
 * Set cursor position on active terminal
 */
void vterm_set_cursor(uint32_t col, uint32_t row)
{
    if (!g_initialized) return;
    
    vterm_t *term = &g_terminals[g_active_terminal];
    
    if (col >= term->cols) col = term->cols - 1;
    if (row >= term->rows) row = term->rows - 1;
    
    term->cursor_col = col;
    term->cursor_row = row;
}

/**
 * Get cursor position on active terminal
 */
void vterm_get_cursor(uint32_t *col, uint32_t *row)
{
    if (!g_initialized) {
        if (col) *col = 0;
        if (row) *row = 0;
        return;
    }
    
    vterm_t *term = &g_terminals[g_active_terminal];
    if (col) *col = term->cursor_col;
    if (row) *row = term->cursor_row;
}

/**
 * Set colors on active terminal
 */
void vterm_set_colors(uint8_t fg, uint8_t bg)
{
    if (!g_initialized) return;
    
    vterm_t *term = &g_terminals[g_active_terminal];
    term->fg_color = fg & 0x0F;
    term->bg_color = bg & 0x0F;
}

/**
 * Flush pending updates to the display
 */
void vterm_flush(void)
{
    if (!g_initialized) {
        if (fbcon_available()) {
            fbcon_flush();
        }
        return;
    }
    
    fb_flush();
}

/**
 * Process keyboard input
 * 
 * Handles escape sequences for terminal switching.
 * ANSI escape sequences for function keys:
 *   F1  = ESC O P  or  ESC [ 1 1 ~
 *   F2  = ESC O Q  or  ESC [ 1 2 ~
 *   F3  = ESC O R  or  ESC [ 1 3 ~
 *   F4  = ESC O S  or  ESC [ 1 4 ~
 *   F5  = ESC [ 1 5 ~
 *   F6  = ESC [ 1 7 ~
 * 
 * With Alt (Meta) key:
 *   Alt+F1 = ESC ESC O P  or  ESC [ 1 ; 3 P  (xterm)
 *   Alt+Fn = ESC [ n ; 3 ~  (xterm modifier format)
 * 
 * For simplicity, we also support direct Alt+1 through Alt+6:
 *   Alt+1 = ESC 1
 *   Alt+2 = ESC 2
 *   etc.
 */
char vterm_process_input(char c)
{
    /* Simple Alt+number detection: ESC followed by 1-6 */
    if (g_input_state.in_escape) {
        g_input_state.escape_buf[g_input_state.escape_len++] = c;
        
        /* Alt+1 through Alt+6 (ESC + digit) */
        if (g_input_state.escape_len == 1 && c >= '1' && c <= '6') {
            g_input_state.in_escape = 0;
            g_input_state.escape_len = 0;
            
            /* Switch terminal */
            int new_term = c - '1';
            vterm_switch(new_term);
            return 0;  /* Consumed */
        }
        
        /* ESC O P/Q/R/S for F1-F4 */
        if (g_input_state.escape_len == 1 && c == 'O') {
            /* Wait for next character */
            return 0;
        }
        
        if (g_input_state.escape_len == 2 && g_input_state.escape_buf[0] == 'O') {
            g_input_state.in_escape = 0;
            g_input_state.escape_len = 0;
            
            /* Check for F1-F4 */
            switch (c) {
            case 'P':  /* F1 */
                vterm_switch(0);
                return 0;
            case 'Q':  /* F2 */
                vterm_switch(1);
                return 0;
            case 'R':  /* F3 */
                vterm_switch(2);
                return 0;
            case 'S':  /* F4 */
                vterm_switch(3);
                return 0;
            }
            /* Unknown sequence, pass through */
            return c;
        }
        
        /* ESC [ sequence */
        if (g_input_state.escape_len == 1 && c == '[') {
            /* Wait for more characters */
            return 0;
        }
        
        /* Check for ESC [ 1 x ~ format (F1-F6) */
        if (g_input_state.escape_len >= 2 && g_input_state.escape_buf[0] == '[') {
            if (c == '~') {
                /* End of sequence */
                g_input_state.in_escape = 0;
                g_input_state.escape_len = 0;
                
                /* Parse function key number */
                if (g_input_state.escape_buf[1] == '1') {
                    if (g_input_state.escape_len == 4) {
                        char key = g_input_state.escape_buf[2];
                        switch (key) {
                        case '1':  /* F1: ESC [ 1 1 ~ */
                            vterm_switch(0);
                            return 0;
                        case '2':  /* F2: ESC [ 1 2 ~ */
                            vterm_switch(1);
                            return 0;
                        case '3':  /* F3: ESC [ 1 3 ~ */
                            vterm_switch(2);
                            return 0;
                        case '4':  /* F4: ESC [ 1 4 ~ */
                            vterm_switch(3);
                            return 0;
                        case '5':  /* F5: ESC [ 1 5 ~ */
                            vterm_switch(4);
                            return 0;
                        case '7':  /* F6: ESC [ 1 7 ~ */
                            vterm_switch(5);
                            return 0;
                        }
                    }
                }
                return 0;  /* Consume unknown function key */
            }
            
            /* Keep accumulating */
            if (g_input_state.escape_len < 7) {
                return 0;
            }
            
            /* Sequence too long, abort */
            g_input_state.in_escape = 0;
            g_input_state.escape_len = 0;
            return 0;
        }
        
        /* Unknown escape sequence - timeout or invalid */
        if (g_input_state.escape_len > 6) {
            g_input_state.in_escape = 0;
            g_input_state.escape_len = 0;
        }
        
        return 0;  /* Still processing escape */
    }
    
    /* Start of escape sequence */
    if (c == 0x1B) {  /* ESC */
        g_input_state.in_escape = 1;
        g_input_state.escape_len = 0;
        return 0;
    }
    
    /* Regular character */
    return c;
}
