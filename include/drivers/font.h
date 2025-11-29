/**
 * Bitmap Font for Framebuffer Console
 * 
 * Provides 8x16 VGA-compatible bitmap font rendering.
 */

#ifndef FONT_H
#define FONT_H

#include <stdint.h>

/* Font dimensions */
#define FONT_WIDTH      8
#define FONT_HEIGHT     16

/* First and last printable ASCII character */
#define FONT_FIRST_CHAR 0x20    /* Space */
#define FONT_LAST_CHAR  0x7E    /* Tilde */
#define FONT_NUM_CHARS  (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1)

/**
 * Get the bitmap data for a character
 * 
 * @param c Character to get bitmap for
 * @return Pointer to 16-byte bitmap data, or NULL for unprintable chars
 */
const uint8_t *font_get_glyph(char c);

/**
 * Draw a character to the framebuffer
 * 
 * @param x X coordinate (top-left)
 * @param y Y coordinate (top-left)
 * @param c Character to draw
 * @param fg Foreground color (ARGB)
 * @param bg Background color (ARGB)
 */
void font_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);

/**
 * Draw a character with transparent background
 * 
 * @param x X coordinate (top-left)
 * @param y Y coordinate (top-left)
 * @param c Character to draw
 * @param fg Foreground color (ARGB)
 */
void font_draw_char_transparent(uint32_t x, uint32_t y, char c, uint32_t fg);

/**
 * Draw a string to the framebuffer
 * 
 * @param x X coordinate (top-left)
 * @param y Y coordinate (top-left)
 * @param str String to draw
 * @param fg Foreground color (ARGB)
 * @param bg Background color (ARGB)
 */
void font_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);

/**
 * Draw a string with transparent background
 * 
 * @param x X coordinate (top-left)
 * @param y Y coordinate (top-left)
 * @param str String to draw
 * @param fg Foreground color (ARGB)
 */
void font_draw_string_transparent(uint32_t x, uint32_t y, const char *str, uint32_t fg);

/**
 * Calculate string width in pixels
 * 
 * @param str String to measure
 * @return Width in pixels
 */
uint32_t font_string_width(const char *str);

/**
 * Get font height
 * 
 * @return Font height in pixels
 */
static inline uint32_t font_get_height(void)
{
    return FONT_HEIGHT;
}

/**
 * Get font width
 * 
 * @return Font width in pixels
 */
static inline uint32_t font_get_width(void)
{
    return FONT_WIDTH;
}

#endif /* FONT_H */
