/*
 * Memory Management Test Program
 * 
 * Tests DMA allocation, address translation, and memory barriers
 */

#include "hal/hal_uart.h"
#include "mm/dma.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "kernel/kstring.h"
#include "arch/barrier.h"

void test_memory_management(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  Memory Management Feature Tests\n");
    hal_uart_puts("========================================\n\n");
    
    int tests_passed = 0;
    int tests_total = 0;
    
    // ========================================
    // Test 1: DMA Allocation
    // ========================================
    hal_uart_puts("Test 1: DMA Allocation\n");
    hal_uart_puts("  Allocating 8KB DMA region... ");
    tests_total++;
    
    dma_region_t *region1 = dma_alloc(8192, DMA_ZERO);
    if (region1 != NULL) {
        hal_uart_puts("PASS\n");
        hal_uart_puts("    Virtual:  0x");
        kprint_hex((uintptr_t)dma_virt_addr(region1));
        hal_uart_puts("\n");
        hal_uart_puts("    Physical: 0x");
        kprint_hex(dma_phys_addr(region1));
        hal_uart_puts("\n");
        hal_uart_puts("    Size:     ");
        kprint_dec(dma_size(region1));
        hal_uart_puts(" bytes\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL\n");
    }
    
    // ========================================
    // Test 2: DMA Memory is Zeroed
    // ========================================
    hal_uart_puts("\nTest 2: DMA Memory is Zeroed (DMA_ZERO flag)\n");
    hal_uart_puts("  Checking first 256 bytes... ");
    tests_total++;
    
    if (region1 != NULL) {
        uint8_t *ptr = (uint8_t *)dma_virt_addr(region1);
        int all_zero = 1;
        for (size_t i = 0; i < 256; i++) {
            if (ptr[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        
        if (all_zero) {
            hal_uart_puts("PASS\n");
            tests_passed++;
        } else {
            hal_uart_puts("FAIL\n");
        }
    } else {
        hal_uart_puts("SKIP (region1 is NULL)\n");
    }
    
    // ========================================
    // Test 3: Physical Contiguity
    // ========================================
    hal_uart_puts("\nTest 3: Physical Contiguity (2 pages)\n");
    hal_uart_puts("  Verifying physical addresses are contiguous... ");
    tests_total++;
    
    if (region1 != NULL) {
        uintptr_t phys_base = dma_phys_addr(region1);
        uintptr_t virt_base = (uintptr_t)dma_virt_addr(region1);
        
        // Check that second page follows first page physically
        uintptr_t phys_expected = phys_base + PAGE_SIZE;
        
        // Translate second page virtual to physical
        uintptr_t phys_page2;
        int result = virt_to_phys(get_kernel_page_table(), virt_base + PAGE_SIZE, &phys_page2);
        
        if (result == 0 && phys_page2 == phys_expected) {
            hal_uart_puts("PASS\n");
            hal_uart_puts("    Page 1 physical: 0x");
            kprint_hex(phys_base);
            hal_uart_puts("\n");
            hal_uart_puts("    Page 2 physical: 0x");
            kprint_hex(phys_page2);
            hal_uart_puts("\n");
            tests_passed++;
        } else {
            hal_uart_puts("FAIL\n");
        }
    } else {
        hal_uart_puts("SKIP (region1 is NULL)\n");
    }
    
    // ========================================
    // Test 4: Multiple DMA Regions
    // ========================================
    hal_uart_puts("\nTest 4: Multiple DMA Regions\n");
    hal_uart_puts("  Allocating second 4KB region... ");
    tests_total++;
    
    dma_region_t *region2 = dma_alloc(4096, 0);
    if (region2 != NULL) {
        hal_uart_puts("PASS\n");
        hal_uart_puts("    Virtual:  0x");
        kprint_hex((uintptr_t)dma_virt_addr(region2));
        hal_uart_puts("\n");
        hal_uart_puts("    Physical: 0x");
        kprint_hex(dma_phys_addr(region2));
        hal_uart_puts("\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL\n");
    }
    
    // ========================================
    // Test 5: Address Translation
    // ========================================
    hal_uart_puts("\nTest 5: Virtual-to-Physical Translation\n");
    hal_uart_puts("  Testing translate_virt_to_phys()... ");
    tests_total++;
    
    if (region1 != NULL) {
        uintptr_t virt = (uintptr_t)dma_virt_addr(region1);
        uintptr_t phys_expected = dma_phys_addr(region1);
        uintptr_t phys_translated = translate_virt_to_phys(virt);
        
        if (phys_translated == phys_expected) {
            hal_uart_puts("PASS\n");
            hal_uart_puts("    Virtual:  0x");
            kprint_hex(virt);
            hal_uart_puts("\n");
            hal_uart_puts("    Physical: 0x");
            kprint_hex(phys_translated);
            hal_uart_puts("\n");
            tests_passed++;
        } else {
            hal_uart_puts("FAIL\n");
            hal_uart_puts("    Expected: 0x");
            kprint_hex(phys_expected);
            hal_uart_puts("\n");
            hal_uart_puts("    Got:      0x");
            kprint_hex(phys_translated);
            hal_uart_puts("\n");
        }
    } else {
        hal_uart_puts("SKIP (region1 is NULL)\n");
    }
    
    // ========================================
    // Test 6: Memory Barriers
    // ========================================
    hal_uart_puts("\nTest 6: Memory Barriers\n");
    hal_uart_puts("  Testing fence instructions (should not crash)... ");
    tests_total++;
    
    // These should execute without errors
    memory_barrier();
    write_barrier();
    read_barrier();
    io_barrier();
    data_memory_barrier();
    data_sync_barrier();
    compiler_barrier();
    
    hal_uart_puts("PASS\n");
    tests_passed++;
    
    // ========================================
    // Test 7: Barrier Helper Functions
    // ========================================
    hal_uart_puts("\nTest 7: Barrier Helper Functions\n");
    hal_uart_puts("  Testing read32/write32 with barriers... ");
    tests_total++;
    
    if (region1 != NULL) {
        volatile uint32_t *ptr = (volatile uint32_t *)dma_virt_addr(region1);
        
        // Write with barrier
        write32_barrier(ptr, 0xDEADBEEF);
        
        // Read with barrier
        uint32_t value = read32_barrier(ptr);
        
        if (value == 0xDEADBEEF) {
            hal_uart_puts("PASS\n");
            tests_passed++;
        } else {
            hal_uart_puts("FAIL\n");
            hal_uart_puts("    Expected: 0xDEADBEEF\n");
            hal_uart_puts("    Got:      0x");
            kprint_hex(value);
            hal_uart_puts("\n");
        }
    } else {
        hal_uart_puts("SKIP (region1 is NULL)\n");
    }
    
    // ========================================
    // Test 8: DMA Statistics
    // ========================================
    hal_uart_puts("\nTest 8: DMA Statistics\n");
    hal_uart_puts("  Checking allocation stats... ");
    tests_total++;
    
    size_t num_regions = 0;
    size_t num_bytes = 0;
    dma_get_stats(&num_regions, &num_bytes);
    
    // We allocated 2 regions (8KB + 4KB = 12KB = 12288 bytes)
    size_t expected_regions = (region1 != NULL ? 1 : 0) + (region2 != NULL ? 1 : 0);
    size_t expected_bytes = (region1 != NULL ? 8192 : 0) + (region2 != NULL ? 4096 : 0);
    
    if (num_regions == expected_regions && num_bytes == expected_bytes) {
        hal_uart_puts("PASS\n");
        hal_uart_puts("    Regions: ");
        kprint_dec(num_regions);
        hal_uart_puts("\n");
        hal_uart_puts("    Bytes:   ");
        kprint_dec(num_bytes);
        hal_uart_puts("\n");
        tests_passed++;
    } else {
        hal_uart_puts("FAIL\n");
        hal_uart_puts("    Expected: ");
        kprint_dec(expected_regions);
        hal_uart_puts(" regions, ");
        kprint_dec(expected_bytes);
        hal_uart_puts(" bytes\n");
        hal_uart_puts("    Got:      ");
        kprint_dec(num_regions);
        hal_uart_puts(" regions, ");
        kprint_dec(num_bytes);
        hal_uart_puts(" bytes\n");
    }
    
    // ========================================
    // Test 9: DMA Free
    // ========================================
    hal_uart_puts("\nTest 9: DMA Free\n");
    hal_uart_puts("  Freeing first region... ");
    tests_total++;
    
    if (region1 != NULL) {
        dma_free(region1);
        region1 = NULL;
        
        // Check stats again
        dma_get_stats(&num_regions, &num_bytes);
        
        if (num_regions == 1 && num_bytes == 4096) {
            hal_uart_puts("PASS\n");
            tests_passed++;
        } else {
            hal_uart_puts("FAIL (stats incorrect after free)\n");
        }
    } else {
        hal_uart_puts("SKIP (region1 is NULL)\n");
    }
    
    // ========================================
    // Test 10: Free Second Region
    // ========================================
    hal_uart_puts("\nTest 10: Free Second Region\n");
    hal_uart_puts("  Freeing second region... ");
    tests_total++;
    
    if (region2 != NULL) {
        dma_free(region2);
        region2 = NULL;
        
        // Check stats again
        dma_get_stats(&num_regions, &num_bytes);
        
        if (num_regions == 0 && num_bytes == 0) {
            hal_uart_puts("PASS\n");
            tests_passed++;
        } else {
            hal_uart_puts("FAIL (stats should be zero)\n");
        }
    } else {
        hal_uart_puts("SKIP (region2 is NULL)\n");
    }
    
    // ========================================
    // Summary
    // ========================================
    hal_uart_puts("\n========================================\n");
    hal_uart_puts("Test Summary:\n");
    hal_uart_puts("  Passed: ");
    kprint_dec(tests_passed);
    hal_uart_puts(" / ");
    kprint_dec(tests_total);
    hal_uart_puts("\n");
    
    if (tests_passed == tests_total) {
        hal_uart_puts("  Status: ALL TESTS PASSED!\n");
    } else {
        hal_uart_puts("  Status: SOME TESTS FAILED\n");
    }
    hal_uart_puts("========================================\n\n");
}
