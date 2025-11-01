/*
 * Physical Memory Manager Implementation
 * 
 * Uses a bitmap allocator for simplicity and efficiency.
 * Each bit represents one 4KB page: 0=free, 1=allocated
 */

#include "mm/pmm.h"
#include "kernel/panic.h"
#include "hal/hal_uart.h"

// Bitmap allocation constants
#define BITS_PER_BYTE 8
#define BITMAP_SIZE 4096  // Supports up to 32MB with 4KB pages (4096 bytes * 8 bits/byte * 4KB/page)

// Hexadecimal conversion constants
#define HEX_BUFFER_SIZE 17      // 16 hex digits + null terminator
#define HEX_DIGIT_SHIFT 4       // Bits per hex digit
#define HEX_DIGIT_MASK 0xF      // Mask for one hex digit
#define HEX_LETTER_BASE 10      // Base value for letter hex digits (a-f start at 10)
#define HEX_DIGITS_IN_64BIT 16  // Number of hex digits in 64-bit value

// Decimal conversion constants
#define DECIMAL_BUFFER_SIZE 20  // Max digits in 64-bit number
#define DECIMAL_BASE 10

// Memory region information
static uintptr_t memory_start = 0;
static size_t total_pages = 0;
static size_t free_pages = 0;

// Bitmap to track page allocation
// Each byte represents 8 pages (1 bit per page)
static uint8_t page_bitmap[BITMAP_SIZE];

// Helper: Check if a bit is set in the bitmap
static inline int bitmap_test(size_t page_num) {
    size_t byte_index = page_num / BITS_PER_BYTE;
    size_t bit_index = page_num % BITS_PER_BYTE;
    return (page_bitmap[byte_index] >> bit_index) & 1;
}

// Helper: Set a bit in the bitmap (mark page as allocated)
static inline void bitmap_set(size_t page_num) {
    size_t byte_index = page_num / BITS_PER_BYTE;
    size_t bit_index = page_num % BITS_PER_BYTE;
    page_bitmap[byte_index] |= (1 << bit_index);
}

// Helper: Clear a bit in the bitmap (mark page as free)
static inline void bitmap_clear(size_t page_num) {
    size_t byte_index = page_num / BITS_PER_BYTE;
    size_t bit_index = page_num % BITS_PER_BYTE;
    page_bitmap[byte_index] &= ~(1 << bit_index);
}

/**
 * Initialize the physical memory manager
 */
void pmm_init(uintptr_t mem_start, size_t mem_size) {
    // Store memory region info
    memory_start = PAGE_ALIGN_UP(mem_start);
    total_pages = mem_size / PAGE_SIZE;
    
    // Limit to bitmap capacity
    size_t max_pages = BITMAP_SIZE * BITS_PER_BYTE;
    if (total_pages > max_pages) {
        total_pages = max_pages;
    }
    
    free_pages = total_pages;
    
    // Initialize bitmap: all pages start as free (0)
    for (size_t i = 0; i < BITMAP_SIZE; i++) {
        page_bitmap[i] = 0;
    }
    
    // Print initialization info
    hal_uart_puts("PMM: Initialized\n");
    hal_uart_puts("  Memory start: 0x");
    
    // Print hex address
    char hex[HEX_BUFFER_SIZE];
    uintptr_t addr = memory_start;
    for (int i = HEX_DIGITS_IN_64BIT - 1; i >= 0; i--) {
        int digit = (addr >> (i * HEX_DIGIT_SHIFT)) & HEX_DIGIT_MASK;
        hex[HEX_DIGITS_IN_64BIT - 1 - i] = digit < DECIMAL_BASE ? '0' + digit : 'a' + digit - HEX_LETTER_BASE;
    }
    hex[HEX_DIGITS_IN_64BIT] = '\0';
    hal_uart_puts(hex);
    hal_uart_puts("\n");
    
    hal_uart_puts("  Total pages: ");
    // Simple number printing
    if (total_pages < DECIMAL_BASE) {
        hal_uart_putc('0' + total_pages);
    } else {
        char buf[DECIMAL_BUFFER_SIZE];
        int i = 0;
        size_t n = total_pages;
        while (n > 0) {
            buf[i++] = '0' + (n % DECIMAL_BASE);
            n /= DECIMAL_BASE;
        }
        while (i > 0) {
            hal_uart_putc(buf[--i]);
        }
    }
    hal_uart_puts("\n");
    
    hal_uart_puts("  Page size: 4KB\n");
}

/**
 * Allocate a single physical page
 */
uintptr_t pmm_alloc_page(void) {
    // Find first free page in bitmap
    for (size_t page_num = 0; page_num < total_pages; page_num++) {
        if (!bitmap_test(page_num)) {
            // Found a free page!
            bitmap_set(page_num);
            free_pages--;
            
            // Calculate physical address
            uintptr_t page_addr = memory_start + (page_num * PAGE_SIZE);
            return page_addr;
        }
    }
    
    // Out of memory!
    hal_uart_puts("PMM: Out of memory!\n");
    return 0;
}

/**
 * Allocate multiple contiguous physical pages
 */
uintptr_t pmm_alloc_pages(size_t num_pages) {
    if (num_pages == 0) {
        return 0;
    }
    
    if (num_pages == 1) {
        return pmm_alloc_page();
    }
    
    // Check if request exceeds total pages (prevent underflow)
    if (num_pages > total_pages) {
        hal_uart_puts("PMM: Request exceeds total pages\n");
        return 0;
    }
    
    // Search for contiguous free pages
    for (size_t start_page = 0; start_page <= total_pages - num_pages; start_page++) {
        // Check if we have num_pages contiguous free pages starting at start_page
        int found = 1;
        for (size_t i = 0; i < num_pages; i++) {
            if (bitmap_test(start_page + i)) {
                found = 0;
                break;
            }
        }
        
        if (found) {
            // Allocate all pages
            for (size_t i = 0; i < num_pages; i++) {
                bitmap_set(start_page + i);
                free_pages--;
            }
            
            // Return physical address of first page
            return memory_start + (start_page * PAGE_SIZE);
        }
    }
    
    // Could not find contiguous pages
    hal_uart_puts("PMM: Could not allocate ");
    // Print num_pages
    char buf[DECIMAL_BUFFER_SIZE];
    int idx = 0;
    size_t n = num_pages;
    while (n > 0) {
        buf[idx++] = '0' + (n % DECIMAL_BASE);
        n /= DECIMAL_BASE;
    }
    while (idx > 0) {
        hal_uart_putc(buf[--idx]);
    }
    hal_uart_puts(" contiguous pages\n");
    return 0;
}

/**
 * Free a previously allocated page
 */
void pmm_free_page(uintptr_t page_addr) {
    // Validate address is page-aligned
    if (page_addr & (PAGE_SIZE - 1)) {
        hal_uart_puts("PMM: Error - address not page-aligned\n");
        return;
    }
    
    // Validate address is in managed region
    if (page_addr < memory_start) {
        hal_uart_puts("PMM: Error - address below managed region\n");
        return;
    }
    
    // Calculate page number
    size_t page_num = (page_addr - memory_start) / PAGE_SIZE;
    
    if (page_num >= total_pages) {
        hal_uart_puts("PMM: Error - address above managed region\n");
        return;
    }
    
    // Check if page is actually allocated
    if (!bitmap_test(page_num)) {
        hal_uart_puts("PMM: Warning - freeing already-free page\n");
        return;
    }
    
    // Free the page
    bitmap_clear(page_num);
    free_pages++;
}

/**
 * Free multiple contiguous physical pages
 */
void pmm_free_pages(uintptr_t page_addr, size_t num_pages) {
    if (num_pages == 0) {
        return;
    }
    
    for (size_t i = 0; i < num_pages; i++) {
        pmm_free_page(page_addr + (i * PAGE_SIZE));
    }
}

/**
 * Get memory statistics
 */
void pmm_get_stats(size_t *total, size_t *free) {
    if (total) *total = total_pages;
    if (free) *free = free_pages;
}
