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
#include <kernel/signal.h>
#include <kernel/process.h>
#include <hal/hal_uart.h>
#include <stddef.h>

/* Virtual terminal array */
static vterm_t g_terminals[VTERM_MAX_TERMINALS];

/* Currently active terminal index */
static int g_active_terminal = 0;

/* Track which terminal should receive input (for console multiplexing) */
static int g_input_terminal = 0;

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
static int input_buffer_put_to(int index, char c);

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
    
    /* No foreground process initially */
    term->fg_pid = -1;
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
    
    /* Initialize all terminals (works with or without framebuffer) */
    for (int i = 0; i < VTERM_MAX_TERMINALS; i++) {
        init_terminal(&g_terminals[i], i);
    }
    
    /* Reset input state */
    g_input_state.in_escape = 0;
    g_input_state.alt_pressed = 0;
    g_input_state.escape_len = 0;
    
    g_active_terminal = 0;
    g_initialized = 1;
    
    /* If framebuffer is available, draw initial screen */
    if (fbcon_available()) {
        vterm_refresh();
        vterm_draw_status_bar();
        vterm_flush();
    }
    
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
        
        /* Highlight current terminal */
        uint32_t tab_bg = (i == g_active_terminal) ? active_fg : bg;
        uint32_t tab_fg = (i == g_active_terminal) ? bg : fg;
        
        /* Dim inactive terminals */
        if (!t->active && i != g_active_terminal) {
            tab_fg = ansi_colors[8];  /* Dark gray for inactive */
        }
        
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
    
    /* Update input terminal tracking */
    g_input_terminal = index;
    
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
        
    default:
        break;
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
    
    /* Only flush to framebuffer if available */
    if (fbcon_available()) {
        fb_flush();
    }
    /* In UART-only mode, output is already immediate */
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
    /* DEBUG: Log every character received */
    // hal_uart_puts("[IN] ");
    // hal_uart_putc(c >= 32 && c < 127 ? c : '?');
    // hal_uart_puts("\n");
    
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
        
        /* ESC [ sequence */
        if (g_input_state.escape_len == 1 && c == '[') {
            /* Wait for more characters */
            return 0;
        }
        
        /* First character after ESC is not a valid sequence start - abort and pass through */
        if (g_input_state.escape_len == 1) {
            g_input_state.in_escape = 0;
            g_input_state.escape_len = 0;
            /* Pass the ESC and the character to userspace */
            input_buffer_put_to(g_active_terminal, 0x1B);
            return c;
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
            default:
                break;
            }
            /* Unknown ESC O x sequence, pass through */
            input_buffer_put_to(g_active_terminal, 0x1B);
            input_buffer_put_to(g_active_terminal, 'O');
            return c;
        }
        
        /* Check for arrow keys: ESC [ A/B/C/D - pass through to userspace */
        if (g_input_state.escape_len == 2 && g_input_state.escape_buf[0] == '[') {
            if (c == 'A' || c == 'B' || c == 'C' || c == 'D') {
                /* Arrow key - pass the entire sequence to userspace */
                g_input_state.in_escape = 0;
                g_input_state.escape_len = 0;
                /* Buffer the entire escape sequence for userspace */
                input_buffer_put_to(g_active_terminal, 0x1B);  /* ESC */
                input_buffer_put_to(g_active_terminal, '[');
                input_buffer_put_to(g_active_terminal, c);     /* A/B/C/D */
                return 0;  /* Consumed - chars are in buffer */
            }
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
                        default:
                            break;
                        case '7':  /* F6: ESC [ 1 7 ~ */
                            vterm_switch(5);
                            return 0;
                        }
                    }
                }
                return 0;  /* Consume unknown function key */
            }
            
            /* Keep accumulating if it looks like a valid sequence in progress */
            if (g_input_state.escape_len < 7 && (c >= '0' && c <= '9')) {
                return 0;
            }
            
            /* Invalid character in CSI sequence, abort and pass through */
            /* Save the accumulated digits before resetting */
            int saved_len = g_input_state.escape_len;
            g_input_state.in_escape = 0;
            g_input_state.escape_len = 0;
            /* Pass ESC [ and any accumulated digits to userspace */
            input_buffer_put_to(g_active_terminal, 0x1B);
            input_buffer_put_to(g_active_terminal, '[');
            for (int i = 1; i < saved_len - 1; i++) {
                input_buffer_put_to(g_active_terminal, g_input_state.escape_buf[i]);
            }
            return c;
        }
        
        /* Sequence too long, abort */
        g_input_state.in_escape = 0;
        g_input_state.escape_len = 0;
        return 0;
    }
    
    /* Start of escape sequence */
    if (c == 0x1B) {  /* ESC */
        g_input_state.in_escape = 1;
        g_input_state.escape_len = 0;
        return 0;
    }
    
    /* Handle Ctrl+C - send SIGINT to foreground process */
    if (c == 0x03) {  /* Ctrl+C */
        vterm_t *term = &g_terminals[g_active_terminal];
        if (term->fg_pid > 0) {
            struct process *fg_proc = process_get(term->fg_pid);
            if (fg_proc && fg_proc->state != PROC_UNUSED) {
                signal_send(fg_proc, SIGINT);
                /* Echo ^C to show it was received */
                vterm_putc('^');
                vterm_putc('C');
                vterm_putc('\n');
            }
        }
        return 0;  /* Consumed */
    }
    
    /* Handle Ctrl+Z - send SIGTSTP to foreground process (suspend) */
    if (c == 0x1A) {  /* Ctrl+Z */
        vterm_t *term = &g_terminals[g_active_terminal];
        if (term->fg_pid > 0) {
            struct process *fg_proc = process_get(term->fg_pid);
            if (fg_proc && fg_proc->state != PROC_UNUSED) {
                signal_send(fg_proc, SIGTSTP);
                /* Echo ^Z to show it was received */
                vterm_putc('^');
                vterm_putc('Z');
                vterm_putc('\n');
            }
        }
        return 0;  /* Consumed */
    }
    
    /* Regular character */
    return c;
}

/*
 * Console Multiplexing Implementation
 * 
 * These functions enable routing output to specific terminals,
 * independent of which terminal is currently displayed.
 */

/**
 * Internal function to write character to a specific terminal
 */
static void vterm_putc_internal(vterm_t *term, int index, char c)
{
    if (!term) return;
    
    /* Mark terminal as active (has content) */
    term->active = 1;
    
    /* Handle control characters */
    switch (c) {
    case '\n':
        /* Move to next line */
        term->cursor_row++;
        term->cursor_col = 0;
        if (term->cursor_row >= term->rows) {
            /* Scroll up */
            for (uint32_t row = 0; row < term->rows - 1; row++) {
                for (uint32_t col = 0; col < term->cols; col++) {
                    term->buffer[row][col] = term->buffer[row + 1][col];
                }
            }
            /* Clear bottom row */
            for (uint32_t col = 0; col < term->cols; col++) {
                term->buffer[term->rows - 1][col].ch = ' ';
                term->buffer[term->rows - 1][col].fg_color = term->fg_color;
                term->buffer[term->rows - 1][col].bg_color = term->bg_color;
                term->buffer[term->rows - 1][col].attrs = VTERM_ATTR_NONE;
            }
            term->cursor_row = term->rows - 1;
        }
        /* Redraw if this is the active terminal */
        if (index == g_active_terminal) {
            vterm_refresh();
        }
        return;
        
    case '\r':
        term->cursor_col = 0;
        return;
        
    case '\b':
        if (term->cursor_col > 0) {
            term->cursor_col--;
            term->buffer[term->cursor_row][term->cursor_col].ch = ' ';
            if (index == g_active_terminal) {
                vterm_draw_cell(term->cursor_col, term->cursor_row, 
                               &term->buffer[term->cursor_row][term->cursor_col]);
            }
        }
        return;
        
    case '\t':
        /* Tab to next 8-column boundary */
        do {
            vterm_putc_internal(term, index, ' ');
        } while (term->cursor_col % 8 != 0 && term->cursor_col < term->cols);
        return;
        
    case '\0':
        return;
        
    default:
        break;
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
    if (index == g_active_terminal) {
        vterm_draw_cell(term->cursor_col, term->cursor_row,
                       &term->buffer[term->cursor_row][term->cursor_col]);
    }
    
    /* Advance cursor */
    term->cursor_col++;
    if (term->cursor_col >= term->cols) {
        term->cursor_row++;
        term->cursor_col = 0;
        if (term->cursor_row >= term->rows) {
            /* Scroll */
            for (uint32_t row = 0; row < term->rows - 1; row++) {
                for (uint32_t col = 0; col < term->cols; col++) {
                    term->buffer[row][col] = term->buffer[row + 1][col];
                }
            }
            for (uint32_t col = 0; col < term->cols; col++) {
                term->buffer[term->rows - 1][col].ch = ' ';
                term->buffer[term->rows - 1][col].fg_color = term->fg_color;
                term->buffer[term->rows - 1][col].bg_color = term->bg_color;
                term->buffer[term->rows - 1][col].attrs = VTERM_ATTR_NONE;
            }
            term->cursor_row = term->rows - 1;
            if (index == g_active_terminal) {
                vterm_refresh();
            }
        }
    }
}

/**
 * Write a character to a specific virtual terminal
 */
void vterm_putc_to(int index, char c)
{
    if (!g_initialized) {
        /* Fallback to UART */
        hal_uart_putc(c);
        return;
    }
    
    if (index < 0 || index >= VTERM_MAX_TERMINALS) {
        /* Default to active terminal */
        index = g_active_terminal;
    }
    
    vterm_t *term = &g_terminals[index];
    vterm_putc_internal(term, index, c);
    
    /* In UART-only mode, echo to UART if writing to active terminal */
    if (!fbcon_available() && index == g_active_terminal) {
        hal_uart_putc(c);
    }
}

/**
 * Write a string to a specific virtual terminal
 */
void vterm_puts_to(int index, const char *str)
{
    if (!str) return;
    
    while (*str) {
        vterm_putc_to(index, *str++);
    }
    
    /* Flush if writing to active terminal */
    if (g_initialized && index == g_active_terminal) {
        vterm_flush();
    }
}

/**
 * Write a character to the kernel console (VT1)
 */
void vterm_kernel_putc(char c)
{
    /* Kernel console is always VT1 (index 0) */
    vterm_putc_to(VTERM_KERNEL_CONSOLE, c);
    
    /* Also echo to UART for debugging */
    hal_uart_putc(c);
}

/**
 * Write a string to the kernel console (VT1)
 */
void vterm_kernel_puts(const char *str)
{
    if (!str) return;
    
    while (*str) {
        vterm_kernel_putc(*str++);
    }
    
    /* Flush if kernel console is visible */
    if (g_initialized && g_active_terminal == VTERM_KERNEL_CONSOLE) {
        vterm_flush();
    }
}

/**
 * Set the terminal that should receive input
 */
void vterm_set_input_terminal(int index)
{
    if (index >= 0 && index < VTERM_MAX_TERMINALS) {
        g_input_terminal = index;
    }
}

/**
 * Get the terminal currently receiving input
 */
int vterm_get_input_terminal(void)
{
    return g_input_terminal;
}

/**
 * Get input from a specific terminal
 * 
 * Returns 0 if input is not destined for this terminal.
 */
char vterm_getc_from(int index)
{
    /* Input goes to the currently active terminal */
    if (index != g_active_terminal) {
        return 0;
    }
    
    /* Read character from UART */
    char c = hal_uart_getc();
    
    /* Process through terminal system for VT switching */
    c = vterm_process_input(c);
    
    return c;
}

/*
 * Per-terminal input buffers for console multiplexing.
 * Each VT has its own input queue so multiple shells can run independently.
 */
#define INPUT_BUFFER_SIZE 64

typedef struct {
    char buffer[INPUT_BUFFER_SIZE];
    volatile int head;
    volatile int tail;
} vterm_input_buffer_t;

static vterm_input_buffer_t g_input_buffers[VTERM_MAX_TERMINALS];

/**
 * Check if input buffer for a terminal has data
 */
static int input_buffer_available_for(int index)
{
    if (index < 0 || index >= VTERM_MAX_TERMINALS) return 0;
    return g_input_buffers[index].head != g_input_buffers[index].tail;
}

/**
 * Get character from a terminal's input buffer
 * Returns -1 if empty
 */
static int input_buffer_get_from(int index)
{
    if (index < 0 || index >= VTERM_MAX_TERMINALS) return -1;
    vterm_input_buffer_t *buf = &g_input_buffers[index];
    if (buf->head == buf->tail) {
        return -1;
    }
    char c = buf->buffer[buf->tail];
    buf->tail = (buf->tail + 1) % INPUT_BUFFER_SIZE;
    return (unsigned char)c;
}

/**
 * Put character in a terminal's input buffer
 * Returns 0 on success, -1 if full
 */
static int input_buffer_put_to(int index, char c)
{
    if (index < 0 || index >= VTERM_MAX_TERMINALS) return -1;
    vterm_input_buffer_t *buf = &g_input_buffers[index];
    int next = (buf->head + 1) % INPUT_BUFFER_SIZE;
    if (next == buf->tail) {
        return -1;  /* Buffer full */
    }
    buf->buffer[buf->head] = c;
    buf->head = next;
    return 0;
}

/* Legacy single-buffer functions for backward compatibility */
static int input_buffer_available(void)
{
    return input_buffer_available_for(g_active_terminal);
}

static int input_buffer_get(void)
{
    return input_buffer_get_from(g_active_terminal);
}

/**
 * Poll for keyboard input and handle VT switching
 * 
 * This is called from timer interrupt to allow VT switching
 * even when no process is reading input. Regular characters
 * are buffered to the ACTIVE terminal's input queue.
 */
int vterm_poll_input(void)
{
    int processed = 0;
    
    /* Read all available UART data */
    while (hal_uart_data_available()) {
        int c = hal_uart_getc_nonblock();
        if (c < 0) break;
        
        /* Process through VT system */
        char result = vterm_process_input((char)c);
        
        if (result == 0) {
            /* Character was consumed by VT switch or escape processing */
            processed = 1;
        } else {
            /* Regular character - buffer to active terminal's input queue */
            input_buffer_put_to(g_active_terminal, result);
            processed = 1;
        }
    }
    
    return processed;
}

/**
 * Get a character that was buffered during polling for a specific terminal
 */
int vterm_get_buffered_input_for(int index)
{
    return input_buffer_get_from(index);
}

/**
 * Check if there's buffered input available for a specific terminal
 */
int vterm_has_buffered_input_for(int index)
{
    return input_buffer_available_for(index);
}

/**
 * Get a character that was buffered during polling
 * Called from sys_read to get pre-buffered input
 * Returns -1 if buffer empty
 */
int vterm_get_buffered_input(void)
{
    return input_buffer_get();
}

/**
 * Check if there's buffered input available
 */
int vterm_has_buffered_input(void)
{
    return input_buffer_available();
}

/**
 * Set the foreground process for a terminal
 */
void vterm_set_fg_pid(int index, int pid)
{
    if (index < 0 || index >= VTERM_MAX_TERMINALS) {
        return;
    }
    g_terminals[index].fg_pid = pid;
}

/**
 * Get the foreground process for a terminal
 */
int vterm_get_fg_pid(int index)
{
    if (index < 0 || index >= VTERM_MAX_TERMINALS) {
        return -1;
    }
    return g_terminals[index].fg_pid;
}

/**
 * Set the foreground process for the active terminal
 */
void vterm_set_active_fg_pid(int pid)
{
    g_terminals[g_active_terminal].fg_pid = pid;
}
