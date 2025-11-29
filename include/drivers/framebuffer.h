/**
 * Framebuffer Abstraction Layer
 * 
 * Provides a hardware-independent interface for framebuffer operations.
 * Can work with VirtIO GPU or other display backends.
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

/* Common color definitions (ARGB format) */
#define FB_COLOR_BLACK          0xFF000000
#define FB_COLOR_WHITE          0xFFFFFFFF
#define FB_COLOR_RED            0xFFFF0000
#define FB_COLOR_GREEN          0xFF00FF00
#define FB_COLOR_BLUE           0xFF0000FF
#define FB_COLOR_YELLOW         0xFFFFFF00
#define FB_COLOR_CYAN           0xFF00FFFF
#define FB_COLOR_MAGENTA        0xFFFF00FF
#define FB_COLOR_GRAY           0xFF808080
#define FB_COLOR_DARK_GRAY      0xFF404040
#define FB_COLOR_LIGHT_GRAY     0xFFC0C0C0

/* Terminal colors (dark variants) */
#define FB_COLOR_TERM_BLACK     0xFF000000
#define FB_COLOR_TERM_RED       0xFFAA0000
#define FB_COLOR_TERM_GREEN     0xFF00AA00
#define FB_COLOR_TERM_YELLOW    0xFFAA5500
#define FB_COLOR_TERM_BLUE      0xFF0000AA
#define FB_COLOR_TERM_MAGENTA   0xFFAA00AA
#define FB_COLOR_TERM_CYAN      0xFF00AAAA
#define FB_COLOR_TERM_WHITE     0xFFAAAAAA

/* Terminal colors (bright variants) */
#define FB_COLOR_BRIGHT_BLACK   0xFF555555
#define FB_COLOR_BRIGHT_RED     0xFFFF5555
#define FB_COLOR_BRIGHT_GREEN   0xFF55FF55
#define FB_COLOR_BRIGHT_YELLOW  0xFFFFFF55
#define FB_COLOR_BRIGHT_BLUE    0xFF5555FF
#define FB_COLOR_BRIGHT_MAGENTA 0xFFFF55FF
#define FB_COLOR_BRIGHT_CYAN    0xFF55FFFF
#define FB_COLOR_BRIGHT_WHITE   0xFFFFFFFF

/* Framebuffer backend types */
typedef enum {
    FB_BACKEND_NONE = 0,
    FB_BACKEND_VIRTIO_GPU,
    FB_BACKEND_LINEAR,          /* Simple linear framebuffer */
} fb_backend_t;

/**
 * Framebuffer information structure
 */
typedef struct {
    uint32_t width;             /* Width in pixels */
    uint32_t height;            /* Height in pixels */
    uint32_t pitch;             /* Bytes per row */
    uint32_t bpp;               /* Bits per pixel */
    uint32_t *pixels;           /* Pixel data pointer */
    fb_backend_t backend;       /* Backend type */
} fb_info_t;

/* ============================================================================
 * Initialization and Query
 * ============================================================================ */

/**
 * Initialize framebuffer subsystem
 * 
 * @return 0 on success, -1 on error (errno set)
 */
int fb_init(void);

/**
 * Check if framebuffer is available
 * 
 * @return 1 if available, 0 otherwise
 */
int fb_available(void);

/**
 * Get framebuffer information
 * 
 * @param info Pointer to fb_info_t structure to fill
 * @return 0 on success, -1 on error
 */
int fb_get_info(fb_info_t *info);

/**
 * Shutdown framebuffer subsystem
 */
void fb_shutdown(void);

/* ============================================================================
 * Basic Drawing Operations
 * ============================================================================ */

/**
 * Set a single pixel
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param color ARGB color value
 */
void fb_set_pixel(uint32_t x, uint32_t y, uint32_t color);

/**
 * Get a single pixel
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @return ARGB color value
 */
uint32_t fb_get_pixel(uint32_t x, uint32_t y);

/**
 * Clear the entire framebuffer
 * 
 * @param color ARGB color to fill with
 */
void fb_clear(uint32_t color);

/**
 * Flush framebuffer to display
 * 
 * @return 0 on success, -1 on error
 */
int fb_flush(void);

/**
 * Flush a specific region to display
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param width Width of region
 * @param height Height of region
 * @return 0 on success, -1 on error
 */
int fb_flush_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

/* ============================================================================
 * Graphics Primitives
 * ============================================================================ */

/**
 * Draw a horizontal line
 * 
 * @param x1 Starting X coordinate
 * @param x2 Ending X coordinate
 * @param y Y coordinate
 * @param color ARGB color
 */
void fb_draw_hline(uint32_t x1, uint32_t x2, uint32_t y, uint32_t color);

/**
 * Draw a vertical line
 * 
 * @param x X coordinate
 * @param y1 Starting Y coordinate
 * @param y2 Ending Y coordinate
 * @param color ARGB color
 */
void fb_draw_vline(uint32_t x, uint32_t y1, uint32_t y2, uint32_t color);

/**
 * Draw a line (Bresenham's algorithm)
 * 
 * @param x1 Starting X coordinate
 * @param y1 Starting Y coordinate
 * @param x2 Ending X coordinate
 * @param y2 Ending Y coordinate
 * @param color ARGB color
 */
void fb_draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);

/**
 * Draw a rectangle outline
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param width Width
 * @param height Height
 * @param color ARGB color
 */
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);

/**
 * Draw a filled rectangle
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param width Width
 * @param height Height
 * @param color ARGB color
 */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Create ARGB color from components
 * 
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return ARGB color value
 */
static inline uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/**
 * Create ARGB color with alpha
 * 
 * @param a Alpha component (0-255)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return ARGB color value
 */
static inline uint32_t fb_argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/**
 * Extract red component from ARGB color
 */
static inline uint8_t fb_get_r(uint32_t color)
{
    return (color >> 16) & 0xFF;
}

/**
 * Extract green component from ARGB color
 */
static inline uint8_t fb_get_g(uint32_t color)
{
    return (color >> 8) & 0xFF;
}

/**
 * Extract blue component from ARGB color
 */
static inline uint8_t fb_get_b(uint32_t color)
{
    return color & 0xFF;
}

/**
 * Extract alpha component from ARGB color
 */
static inline uint8_t fb_get_a(uint32_t color)
{
    return (color >> 24) & 0xFF;
}

#endif /* FRAMEBUFFER_H */
