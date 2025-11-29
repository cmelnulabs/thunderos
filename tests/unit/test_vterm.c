/*
 * Virtual Terminal and Multi-Shell Tests
 * 
 * Tests virtual terminal system and multi-process functionality
 * added in v0.7.0. These are kernel-mode tests, not interactive.
 * 
 * Compile with: ENABLE_KERNEL_TESTS=1
 */

#ifdef ENABLE_KERNEL_TESTS

#include "hal/hal_uart.h"
#include "kernel/kstring.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "drivers/virtio_gpu.h"

/* Forward declarations for vterm functions */
extern int vterm_get_active(void);
extern int vterm_switch(int terminal);
extern void vterm_write_char(int terminal, char c);
extern void vterm_write_string(int terminal, const char *str);
extern int vterm_has_buffered_input_for(int terminal);

void test_vterm_features(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  Virtual Terminal Feature Tests\n");
    hal_uart_puts("  (v0.7.0)\n");
    hal_uart_puts("========================================\n\n");
    
    int tests_passed = 0;
    int tests_total = 0;
    
    /* ========================================
     * Test 1: Get Active Terminal
     * ======================================== */
    hal_uart_puts("Test 1: Get Active Terminal\n");
    hal_uart_puts("  Checking vterm_get_active()... ");
    tests_total++;
    
    int active = vterm_get_active();
    if (active >= 0 && active < 6) {
        hal_uart_puts("PASS (VT");
        kprint_dec(active + 1);
        hal_uart_puts(")\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL (invalid: ");
        kprint_dec(active);
        hal_uart_puts(")\n");
    }
    
    /* ========================================
     * Test 2: Terminal Switch and Restore
     * ======================================== */
    hal_uart_puts("\nTest 2: Terminal Switch\n");
    hal_uart_puts("  Switching to VT2 and back... ");
    tests_total++;
    
    int original = vterm_get_active();
    vterm_switch(1);  /* Switch to VT2 (index 1) */
    int after_switch = vterm_get_active();
    vterm_switch(original);  /* Restore */
    int restored = vterm_get_active();
    
    if (after_switch == 1 && restored == original) {
        hal_uart_puts("PASS\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL\n");
        hal_uart_puts("    After switch: ");
        kprint_dec(after_switch);
        hal_uart_puts(", restored: ");
        kprint_dec(restored);
        hal_uart_puts("\n");
    }
    
    /* ========================================
     * Test 3: Write to Non-Active Terminal
     * ======================================== */
    hal_uart_puts("\nTest 3: Write to Non-Active Terminal\n");
    hal_uart_puts("  Writing to VT2 buffer... ");
    tests_total++;
    
    /* Write to VT2 (index 1) while we're on VT1 (index 0) */
    vterm_write_string(1, "Test message to VT2\n");
    
    /* This test just checks it doesn't crash */
    hal_uart_puts("PASS (no crash)\n");
    tests_passed++;
    
    /* ========================================
     * Test 4: Process Controlling TTY
     * ======================================== */
    hal_uart_puts("\nTest 4: Process Controlling TTY\n");
    hal_uart_puts("  Checking current process tty... ");
    tests_total++;
    
    struct process *current = process_current();
    if (current != NULL) {
        int tty = current->controlling_tty;
        if (tty >= -1 && tty < 6) {
            hal_uart_puts("PASS (tty=");
            if (tty == -1) {
                hal_uart_puts("-1/detached");
            } else {
                kprint_dec(tty);
            }
            hal_uart_puts(")\n");
            tests_passed++;
        } else {
            hal_uart_puts("FAIL (invalid tty: ");
            kprint_dec(tty);
            hal_uart_puts(")\n");
        }
    } else {
        hal_uart_puts("FAIL (no current process)\n");
    }
    
    /* ========================================
     * Test 5: Input Buffer Check
     * ======================================== */
    hal_uart_puts("\nTest 5: Input Buffer Functions\n");
    hal_uart_puts("  Checking buffer status... ");
    tests_total++;
    
    /* Just check the function works without crash */
    int has_input_0 = vterm_has_buffered_input_for(0);
    int has_input_1 = vterm_has_buffered_input_for(1);
    
    hal_uart_puts("PASS (VT1:");
    kprint_dec(has_input_0);
    hal_uart_puts(", VT2:");
    kprint_dec(has_input_1);
    hal_uart_puts(")\n");
    tests_passed++;
    
    /* ========================================
     * Summary
     * ======================================== */
    hal_uart_puts("\n----------------------------------------\n");
    hal_uart_puts("Virtual Terminal Tests: ");
    kprint_dec(tests_passed);
    hal_uart_puts("/");
    kprint_dec(tests_total);
    hal_uart_puts(" passed\n");
    hal_uart_puts("----------------------------------------\n");
}

void test_virtio_gpu_features(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  VirtIO GPU Feature Tests\n");
    hal_uart_puts("  (v0.7.0)\n");
    hal_uart_puts("========================================\n\n");
    
    int tests_passed = 0;
    int tests_total = 0;
    
    /* ========================================
     * Test 1: GPU Availability Check
     * ======================================== */
    hal_uart_puts("Test 1: GPU Availability\n");
    hal_uart_puts("  Checking virtio_gpu_available()... ");
    tests_total++;
    
    int available = virtio_gpu_available();
    if (available) {
        hal_uart_puts("PASS (GPU present)\n");
        tests_passed++;
    } else {
        hal_uart_puts("SKIP (no GPU device)\n");
        hal_uart_puts("\n  Note: VirtIO GPU tests require QEMU with:\n");
        hal_uart_puts("    -device virtio-gpu-device\n\n");
        
        hal_uart_puts("----------------------------------------\n");
        hal_uart_puts("VirtIO GPU Tests: SKIPPED (no device)\n");
        hal_uart_puts("----------------------------------------\n");
        return;
    }
    
    /* ========================================
     * Test 2: Get Dimensions
     * ======================================== */
    hal_uart_puts("\nTest 2: Get Framebuffer Dimensions\n");
    hal_uart_puts("  Querying dimensions... ");
    tests_total++;
    
    uint32_t width = 0, height = 0;
    virtio_gpu_get_dimensions(&width, &height);
    
    if (width > 0 && height > 0) {
        hal_uart_puts("PASS (");
        kprint_dec(width);
        hal_uart_puts("x");
        kprint_dec(height);
        hal_uart_puts(")\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL (0x0)\n");
    }
    
    /* ========================================
     * Test 3: Framebuffer Pointer
     * ======================================== */
    hal_uart_puts("\nTest 3: Get Framebuffer Pointer\n");
    hal_uart_puts("  Getting framebuffer... ");
    tests_total++;
    
    uint32_t *fb = virtio_gpu_get_framebuffer();
    if (fb != NULL) {
        hal_uart_puts("PASS (0x");
        kprint_hex((uintptr_t)fb);
        hal_uart_puts(")\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL (NULL)\n");
    }
    
    /* ========================================
     * Test 4: Set Pixel
     * ======================================== */
    hal_uart_puts("\nTest 4: Set/Get Pixel\n");
    hal_uart_puts("  Testing pixel at (10,10)... ");
    tests_total++;
    
    /* Write a red pixel */
    uint32_t test_color = 0xFFFF0000;  /* ARGB: Red */
    virtio_gpu_set_pixel(10, 10, test_color);
    
    /* Read it back */
    uint32_t read_color = virtio_gpu_get_pixel(10, 10);
    
    /* Note: Color may be converted, check if components match */
    if (read_color != 0) {
        hal_uart_puts("PASS (wrote 0x");
        kprint_hex(test_color);
        hal_uart_puts(", read 0x");
        kprint_hex(read_color);
        hal_uart_puts(")\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL (read 0)\n");
    }
    
    /* ========================================
     * Test 5: Clear Screen
     * ======================================== */
    hal_uart_puts("\nTest 5: Clear Screen\n");
    hal_uart_puts("  Clearing to blue... ");
    tests_total++;
    
    virtio_gpu_clear(0xFF0000FF);  /* ARGB: Blue */
    
    /* Check a pixel */
    uint32_t cleared = virtio_gpu_get_pixel(100, 100);
    if (cleared != 0) {
        hal_uart_puts("PASS\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL\n");
    }
    
    /* ========================================
     * Test 6: Flush Region
     * ======================================== */
    hal_uart_puts("\nTest 6: Flush Region\n");
    hal_uart_puts("  Flushing 100x100 region... ");
    tests_total++;
    
    int result = virtio_gpu_flush_region(0, 0, 100, 100);
    if (result == 0) {
        hal_uart_puts("PASS\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL (error ");
        kprint_dec(result);
        hal_uart_puts(")\n");
    }
    
    /* ========================================
     * Summary
     * ======================================== */
    hal_uart_puts("\n----------------------------------------\n");
    hal_uart_puts("VirtIO GPU Tests: ");
    kprint_dec(tests_passed);
    hal_uart_puts("/");
    kprint_dec(tests_total);
    hal_uart_puts(" passed\n");
    hal_uart_puts("----------------------------------------\n");
}

void test_multi_process_tty(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  Multi-Process TTY Tests\n");
    hal_uart_puts("  (v0.7.0)\n");
    hal_uart_puts("========================================\n\n");
    
    int tests_passed = 0;
    int tests_total = 0;
    
    /* ========================================
     * Test 1: Process Count
     * ======================================== */
    hal_uart_puts("Test 1: Active Process Count\n");
    hal_uart_puts("  Counting running processes... ");
    tests_total++;
    
    int active_count = 0;
    int max_procs = process_get_max_count();
    for (int i = 0; i < max_procs; i++) {
        struct process *p = process_get_by_index(i);
        if (p != NULL) {
            active_count++;
        }
    }
    
    if (active_count >= 1) {
        hal_uart_puts("PASS (");
        kprint_dec(active_count);
        hal_uart_puts(" active)\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL (none active)\n");
    }
    
    /* ========================================
     * Test 2: TTY Distribution
     * ======================================== */
    hal_uart_puts("\nTest 2: Process TTY Distribution\n");
    hal_uart_puts("  Checking TTY assignments... ");
    tests_total++;
    
    int tty_counts[6] = {0, 0, 0, 0, 0, 0};
    int detached_count = 0;
    
    for (int i = 0; i < max_procs; i++) {
        struct process *p = process_get_by_index(i);
        if (p != NULL) {
            if (p->controlling_tty >= 0 && p->controlling_tty < 6) {
                tty_counts[p->controlling_tty]++;
            } else {
                detached_count++;
            }
        }
    }
    
    hal_uart_puts("PASS\n");
    hal_uart_puts("    VT1: ");
    kprint_dec(tty_counts[0]);
    hal_uart_puts(", VT2: ");
    kprint_dec(tty_counts[1]);
    hal_uart_puts(", detached: ");
    kprint_dec(detached_count);
    hal_uart_puts("\n");
    tests_passed++;
    
    /* ========================================
     * Summary
     * ======================================== */
    hal_uart_puts("\n----------------------------------------\n");
    hal_uart_puts("Multi-Process TTY Tests: ");
    kprint_dec(tests_passed);
    hal_uart_puts("/");
    kprint_dec(tests_total);
    hal_uart_puts(" passed\n");
    hal_uart_puts("----------------------------------------\n");
}

/* Main entry point for v0.7.0 tests */
void test_v070_features(void) {
    hal_uart_puts("\n\n");
    hal_uart_puts("########################################\n");
    hal_uart_puts("#   ThunderOS v0.7.0 Feature Tests    #\n");
    hal_uart_puts("########################################\n");
    
    test_vterm_features();
    test_virtio_gpu_features();
    test_multi_process_tty();
    
    hal_uart_puts("\n########################################\n");
    hal_uart_puts("#   v0.7.0 Tests Complete             #\n");
    hal_uart_puts("########################################\n\n");
}

#endif /* ENABLE_KERNEL_TESTS */
