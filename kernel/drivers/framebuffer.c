/*
 * Framebuffer Abstraction Layer
 * 
 * Provides hardware-independent framebuffer operations.
 * Currently supports VirtIO GPU backend.
 */

#include <drivers/framebuffer.h>
#include <drivers/virtio_gpu.h>
#include <kernel/errno.h>
#include <stddef.h>

/* Global framebuffer state */
static fb_info_t g_fb_info = {0};
static int g_fb_initialized = 0;

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * Initialize framebuffer subsystem
 */
int fb_init(void)
{
    if (g_fb_initialized) {
        clear_errno();
        return 0;
    }
    
    /* Try VirtIO GPU backend */
    if (virtio_gpu_available()) {
        uint32_t width = 0;
        uint32_t height = 0;
        virtio_gpu_get_dimensions(&width, &height);
        
        g_fb_info.width = width;
        g_fb_info.height = height;
        g_fb_info.pitch = width * 4;
        g_fb_info.bpp = 32;
        g_fb_info.pixels = virtio_gpu_get_framebuffer();
        g_fb_info.backend = FB_BACKEND_VIRTIO_GPU;
        
        g_fb_initialized = 1;
        clear_errno();
        return 0;
    }
    
    /* No framebuffer available */
    RETURN_ERRNO(THUNDEROS_ENODEV);
}

/**
 * Check if framebuffer is available
 */
int fb_available(void)
{
    return g_fb_initialized && g_fb_info.pixels != NULL;
}

/**
 * Get framebuffer information
 */
int fb_get_info(fb_info_t *info)
{
    if (!info) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    if (!g_fb_initialized) {
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    *info = g_fb_info;
    clear_errno();
    return 0;
}

/**
 * Shutdown framebuffer
 */
void fb_shutdown(void)
{
    if (g_fb_info.backend == FB_BACKEND_VIRTIO_GPU) {
        virtio_gpu_shutdown();
    }
    
    g_fb_info.width = 0;
    g_fb_info.height = 0;
    g_fb_info.pitch = 0;
    g_fb_info.bpp = 0;
    g_fb_info.pixels = NULL;
    g_fb_info.backend = FB_BACKEND_NONE;
    g_fb_initialized = 0;
}

/* ============================================================================
 * Basic Drawing Operations
 * ============================================================================ */

/**
 * Set a single pixel
 */
void fb_set_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!g_fb_initialized) return;
    
    switch (g_fb_info.backend) {
    case FB_BACKEND_VIRTIO_GPU:
        virtio_gpu_set_pixel(x, y, color);
        break;
    case FB_BACKEND_LINEAR:
        if (x < g_fb_info.width && y < g_fb_info.height && g_fb_info.pixels) {
            g_fb_info.pixels[y * g_fb_info.width + x] = color;
        }
        break;
    default:
        break;
    }
}

/**
 * Get a single pixel
 */
uint32_t fb_get_pixel(uint32_t x, uint32_t y)
{
    if (!g_fb_initialized) return 0;
    
    switch (g_fb_info.backend) {
    case FB_BACKEND_VIRTIO_GPU:
        return virtio_gpu_get_pixel(x, y);
    case FB_BACKEND_LINEAR:
        if (x < g_fb_info.width && y < g_fb_info.height && g_fb_info.pixels) {
            return g_fb_info.pixels[y * g_fb_info.width + x];
        }
        break;
    default:
        break;
    }
    
    return 0;
}

/**
 * Clear the framebuffer
 */
void fb_clear(uint32_t color)
{
    if (!g_fb_initialized) return;
    
    switch (g_fb_info.backend) {
    case FB_BACKEND_VIRTIO_GPU:
        virtio_gpu_clear(color);
        break;
    case FB_BACKEND_LINEAR:
        if (g_fb_info.pixels) {
            uint32_t num_pixels = g_fb_info.width * g_fb_info.height;
            for (uint32_t i = 0; i < num_pixels; i++) {
                g_fb_info.pixels[i] = color;
            }
        }
        break;
    default:
        break;
    }
}

/**
 * Flush framebuffer to display
 */
int fb_flush(void)
{
    if (!g_fb_initialized) {
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    switch (g_fb_info.backend) {
    case FB_BACKEND_VIRTIO_GPU:
        return virtio_gpu_flush();
    case FB_BACKEND_LINEAR:
        /* No flush needed for direct framebuffer */
        clear_errno();
        return 0;
    default:
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
}

/**
 * Flush a specific region
 */
int fb_flush_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!g_fb_initialized) {
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    switch (g_fb_info.backend) {
    case FB_BACKEND_VIRTIO_GPU:
        return virtio_gpu_flush_region(x, y, width, height);
    case FB_BACKEND_LINEAR:
        clear_errno();
        return 0;
    default:
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
}

/* ============================================================================
 * Graphics Primitives
 * ============================================================================ */

/**
 * Draw a horizontal line
 */
void fb_draw_hline(uint32_t x1, uint32_t x2, uint32_t y, uint32_t color)
{
    if (!g_fb_initialized) return;
    if (y >= g_fb_info.height) return;
    
    /* Swap if needed */
    if (x1 > x2) {
        uint32_t tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    
    /* Clamp to screen */
    if (x2 >= g_fb_info.width) x2 = g_fb_info.width - 1;
    
    for (uint32_t x = x1; x <= x2; x++) {
        fb_set_pixel(x, y, color);
    }
}

/**
 * Draw a vertical line
 */
void fb_draw_vline(uint32_t x, uint32_t y1, uint32_t y2, uint32_t color)
{
    if (!g_fb_initialized) return;
    if (x >= g_fb_info.width) return;
    
    /* Swap if needed */
    if (y1 > y2) {
        uint32_t tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    
    /* Clamp to screen */
    if (y2 >= g_fb_info.height) y2 = g_fb_info.height - 1;
    
    for (uint32_t y = y1; y <= y2; y++) {
        fb_set_pixel(x, y, color);
    }
}

/**
 * Draw a line using Bresenham's algorithm
 */
void fb_draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color)
{
    if (!g_fb_initialized) return;
    
    int32_t dx = x2 - x1;
    int32_t dy = y2 - y1;
    
    /* Handle special cases */
    if (dx == 0 && dy == 0) {
        if (x1 >= 0 && y1 >= 0) {
            fb_set_pixel((uint32_t)x1, (uint32_t)y1, color);
        }
        return;
    }
    
    if (dy == 0) {
        if (y1 >= 0 && (uint32_t)y1 < g_fb_info.height) {
            fb_draw_hline((uint32_t)(x1 < x2 ? x1 : x2), 
                          (uint32_t)(x1 < x2 ? x2 : x1), 
                          (uint32_t)y1, color);
        }
        return;
    }
    
    if (dx == 0) {
        if (x1 >= 0 && (uint32_t)x1 < g_fb_info.width) {
            fb_draw_vline((uint32_t)x1,
                          (uint32_t)(y1 < y2 ? y1 : y2),
                          (uint32_t)(y1 < y2 ? y2 : y1), color);
        }
        return;
    }
    
    /* Bresenham's algorithm */
    int32_t sx = (dx > 0) ? 1 : -1;
    int32_t sy = (dy > 0) ? 1 : -1;
    
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    
    int32_t err;
    
    if (dx > dy) {
        err = dx / 2;
        while (x1 != x2) {
            if (x1 >= 0 && y1 >= 0 && 
                (uint32_t)x1 < g_fb_info.width && 
                (uint32_t)y1 < g_fb_info.height) {
                fb_set_pixel((uint32_t)x1, (uint32_t)y1, color);
            }
            err -= dy;
            if (err < 0) {
                y1 += sy;
                err += dx;
            }
            x1 += sx;
        }
    } else {
        err = dy / 2;
        while (y1 != y2) {
            if (x1 >= 0 && y1 >= 0 && 
                (uint32_t)x1 < g_fb_info.width && 
                (uint32_t)y1 < g_fb_info.height) {
                fb_set_pixel((uint32_t)x1, (uint32_t)y1, color);
            }
            err -= dx;
            if (err < 0) {
                x1 += sx;
                err += dy;
            }
            y1 += sy;
        }
    }
    
    /* Draw final point */
    if (x2 >= 0 && y2 >= 0 && 
        (uint32_t)x2 < g_fb_info.width && 
        (uint32_t)y2 < g_fb_info.height) {
        fb_set_pixel((uint32_t)x2, (uint32_t)y2, color);
    }
}

/**
 * Draw a rectangle outline
 */
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    if (!g_fb_initialized) return;
    if (width == 0 || height == 0) return;
    
    /* Top edge */
    fb_draw_hline(x, x + width - 1, y, color);
    /* Bottom edge */
    fb_draw_hline(x, x + width - 1, y + height - 1, color);
    /* Left edge */
    fb_draw_vline(x, y, y + height - 1, color);
    /* Right edge */
    fb_draw_vline(x + width - 1, y, y + height - 1, color);
}

/**
 * Draw a filled rectangle
 */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    if (!g_fb_initialized) return;
    if (width == 0 || height == 0) return;
    
    /* Clamp to screen bounds */
    if (x >= g_fb_info.width || y >= g_fb_info.height) return;
    
    if (x + width > g_fb_info.width) {
        width = g_fb_info.width - x;
    }
    if (y + height > g_fb_info.height) {
        height = g_fb_info.height - y;
    }
    
    for (uint32_t row = y; row < y + height; row++) {
        for (uint32_t col = x; col < x + width; col++) {
            fb_set_pixel(col, row, color);
        }
    }
}
