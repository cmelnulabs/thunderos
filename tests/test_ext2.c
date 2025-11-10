/*
 * test_ext2.c - ext2 filesystem tests
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/fs/ext2.h"
#include "../include/drivers/virtio_blk.h"
#include "../include/hal/hal_uart.h"
#include "../include/mm/kmalloc.h"
#include "../include/mm/dma.h"

/* Test statistics */
static int tests_passed = 0;
static int tests_failed = 0;

/* Test result macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            hal_uart_puts("  [PASS] " message "\n"); \
            tests_passed++; \
        } else { \
            hal_uart_puts("  [FAIL] " message "\n"); \
            tests_failed++; \
        } \
    } while (0)

/* Global ext2 filesystem context */
static ext2_fs_t g_ext2_fs;

/**
 * Test 1: Mount ext2 filesystem and verify superblock
 */
void test_ext2_mount(void) {
    hal_uart_puts("\n[TEST] ext2 mount and superblock validation\n");
    
    /* Get VirtIO block device */
    virtio_blk_device_t *blk_dev = virtio_blk_get_device();
    TEST_ASSERT(blk_dev != NULL, "VirtIO block device available");
    
    if (!blk_dev) {
        return;
    }
    
    /* Mount ext2 filesystem */
    int ret = ext2_mount(&g_ext2_fs, blk_dev);
    TEST_ASSERT(ret == 0, "ext2_mount() succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Verify superblock magic */
    TEST_ASSERT(g_ext2_fs.superblock != NULL, "Superblock loaded");
    TEST_ASSERT(g_ext2_fs.superblock->s_magic == EXT2_SUPER_MAGIC, 
                "Superblock magic is 0xEF53");
    
    /* Verify block size is reasonable */
    TEST_ASSERT(g_ext2_fs.block_size >= EXT2_MIN_BLOCK_SIZE && 
                g_ext2_fs.block_size <= EXT2_MAX_BLOCK_SIZE,
                "Block size is valid");
    
    /* Verify we have at least one block group */
    TEST_ASSERT(g_ext2_fs.num_groups > 0, "At least one block group exists");
    
    /* Print filesystem info */
    hal_uart_puts("  Filesystem info:\n");
    hal_uart_puts("    Total inodes: ");
    hal_uart_put_uint32(g_ext2_fs.superblock->s_inodes_count);
    hal_uart_puts("\n");
    
    hal_uart_puts("    Total blocks: ");
    hal_uart_put_uint32(g_ext2_fs.superblock->s_blocks_count);
    hal_uart_puts("\n");
    
    hal_uart_puts("    Block size: ");
    hal_uart_put_uint32(g_ext2_fs.block_size);
    hal_uart_puts(" bytes\n");
    
    hal_uart_puts("    Block groups: ");
    hal_uart_put_uint32(g_ext2_fs.num_groups);
    hal_uart_puts("\n");
}

/**
 * Test 2: Read root directory inode
 */
void test_ext2_read_root_inode(void) {
    hal_uart_puts("\n[TEST] ext2 read root directory inode\n");
    
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret == 0) {
        /* Verify it's a directory */
        TEST_ASSERT((root_inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR,
                    "Root inode is a directory");
        
        /* Verify it has reasonable size */
        TEST_ASSERT(root_inode.i_size > 0, "Root directory has non-zero size");
        
        hal_uart_puts("  Root directory size: ");
        hal_uart_put_uint32(root_inode.i_size);
        hal_uart_puts(" bytes\n");
    }
}

/**
 * Test 3: List root directory contents
 */
static void dir_entry_callback(const char *name, uint32_t inode, uint8_t type) {
    hal_uart_puts("    ");
    
    /* Print file type indicator */
    if (type == EXT2_FT_DIR) {
        hal_uart_puts("[DIR]  ");
    } else if (type == EXT2_FT_REG_FILE) {
        hal_uart_puts("[FILE] ");
    } else {
        hal_uart_puts("[????] ");
    }
    
    /* Print inode number */
    hal_uart_put_uint32(inode);
    hal_uart_puts(" ");
    
    /* Print name */
    hal_uart_puts(name);
    hal_uart_puts("\n");
}

void test_ext2_list_root_dir(void) {
    hal_uart_puts("\n[TEST] ext2 list root directory\n");
    
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret == 0) {
        hal_uart_puts("  Root directory contents:\n");
        ret = ext2_list_dir(&g_ext2_fs, &root_inode, dir_entry_callback);
        TEST_ASSERT(ret == 0, "List directory succeeded");
    }
}

/**
 * Test 4: Read a test file from filesystem
 */
void test_ext2_read_file(void) {
    hal_uart_puts("\n[TEST] ext2 read test file\n");
    
    /* Read root directory inode */
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Look up "test.txt" in root directory */
    uint32_t test_inode_num = ext2_lookup(&g_ext2_fs, &root_inode, "test.txt");
    TEST_ASSERT(test_inode_num != 0, "Found test.txt in root directory");
    
    if (test_inode_num == 0) {
        return;
    }
    
    /* Read test file inode */
    ext2_inode_t test_inode;
    ret = ext2_read_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Read test.txt inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Verify it's a regular file */
    TEST_ASSERT((test_inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFREG,
                "test.txt is a regular file");
    
    /* Allocate buffer for file contents */
    uint32_t file_size = test_inode.i_size;
    char *buffer = (char *)kmalloc(file_size + 1);
    TEST_ASSERT(buffer != NULL, "Allocated buffer for file");
    
    if (!buffer) {
        return;
    }
    
    /* Read file contents */
    ret = ext2_read_file(&g_ext2_fs, &test_inode, 0, buffer, file_size);
    TEST_ASSERT(ret == (int)file_size, "Read complete file contents");
    
    if (ret > 0) {
        buffer[ret] = '\0';  /* Null terminate */
        hal_uart_puts("  File contents: \"");
        hal_uart_puts(buffer);
        hal_uart_puts("\"\n");
    }
    
    kfree(buffer);
}

/**
 * Test 5: Write data to a file
 */
void test_ext2_write_file(void) {
    hal_uart_puts("\n[TEST] ext2 write to existing file\n");
    
    /* Read root directory inode */
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Look up "test.txt" in root directory */
    uint32_t test_inode_num = ext2_lookup(&g_ext2_fs, &root_inode, "test.txt");
    TEST_ASSERT(test_inode_num != 0, "Found test.txt in root directory");
    
    if (test_inode_num == 0) {
        return;
    }
    
    /* Read test file inode */
    ext2_inode_t test_inode;
    ret = ext2_read_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Read test.txt inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Save original size */
    uint32_t original_size = test_inode.i_size;
    
    /* Write new data at the beginning */
    const char *new_data = "MODIFIED";
    ret = ext2_write_file(&g_ext2_fs, &test_inode, 0, new_data, 8);
    TEST_ASSERT(ret == 8, "Wrote 8 bytes to file");
    
    /* Write inode back to disk */
    ret = ext2_write_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Updated inode on disk");
    
    /* Read back to verify */
    char verify_buffer[16];
    ret = ext2_read_file(&g_ext2_fs, &test_inode, 0, verify_buffer, 8);
    TEST_ASSERT(ret == 8, "Read back 8 bytes");
    
    if (ret == 8) {
        int match = 1;
        for (int i = 0; i < 8; i++) {
            if (verify_buffer[i] != new_data[i]) {
                match = 0;
                break;
            }
        }
        TEST_ASSERT(match, "Written data matches");
    }
    
    /* Verify file size was updated if needed */
    if (original_size < 8) {
        TEST_ASSERT(test_inode.i_size == 8, "File size updated to 8 bytes");
    } else {
        TEST_ASSERT(test_inode.i_size == original_size, "File size preserved");
    }
}

/**
 * Test 6: Append data to a file
 */
void test_ext2_append_file(void) {
    hal_uart_puts("\n[TEST] ext2 append to file\n");
    
    /* Read root directory inode */
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Look up "test.txt" in root directory */
    uint32_t test_inode_num = ext2_lookup(&g_ext2_fs, &root_inode, "test.txt");
    TEST_ASSERT(test_inode_num != 0, "Found test.txt in root directory");
    
    if (test_inode_num == 0) {
        return;
    }
    
    /* Read test file inode */
    ext2_inode_t test_inode;
    ret = ext2_read_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Read test.txt inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Save original size */
    uint32_t original_size = test_inode.i_size;
    
    /* Append data */
    const char *append_data = " APPENDED";
    ret = ext2_write_file(&g_ext2_fs, &test_inode, original_size, append_data, 9);
    TEST_ASSERT(ret == 9, "Appended 9 bytes to file");
    
    /* Verify new size */
    TEST_ASSERT(test_inode.i_size == original_size + 9, "File size increased by 9");
    
    /* Write inode back to disk */
    ret = ext2_write_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Updated inode on disk");
    
    /* Read back the appended portion */
    char verify_buffer[16];
    ret = ext2_read_file(&g_ext2_fs, &test_inode, original_size, verify_buffer, 9);
    TEST_ASSERT(ret == 9, "Read back appended data");
    
    if (ret == 9) {
        int match = 1;
        for (int i = 0; i < 9; i++) {
            if (verify_buffer[i] != append_data[i]) {
                match = 0;
                break;
            }
        }
        TEST_ASSERT(match, "Appended data matches");
    }
}

/**
 * Test 7: Write large data across multiple blocks
 */
void test_ext2_write_large_file(void) {
    hal_uart_puts("\n[TEST] ext2 write large data (multi-block)\n");
    
    /* Read root directory inode */
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Look up "test.txt" in root directory */
    uint32_t test_inode_num = ext2_lookup(&g_ext2_fs, &root_inode, "test.txt");
    TEST_ASSERT(test_inode_num != 0, "Found test.txt in root directory");
    
    if (test_inode_num == 0) {
        return;
    }
    
    /* Read test file inode */
    ext2_inode_t test_inode;
    ret = ext2_read_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Read test.txt inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Allocate a large buffer (3KB to span multiple blocks) */
    uint32_t large_size = 3072;
    char *large_buffer = (char *)kmalloc(large_size);
    TEST_ASSERT(large_buffer != NULL, "Allocated 3KB buffer");
    
    if (!large_buffer) {
        return;
    }
    
    /* Fill buffer with pattern */
    for (uint32_t i = 0; i < large_size; i++) {
        large_buffer[i] = (char)('A' + (i % 26));
    }
    
    /* Write large data */
    ret = ext2_write_file(&g_ext2_fs, &test_inode, 0, large_buffer, large_size);
    TEST_ASSERT(ret == (int)large_size, "Wrote 3KB to file");
    TEST_ASSERT(test_inode.i_size == large_size, "File size is 3KB");
    
    /* Write inode back to disk */
    ret = ext2_write_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Updated inode on disk");
    
    /* Allocate verify buffer */
    char *verify_buffer = (char *)kmalloc(large_size);
    TEST_ASSERT(verify_buffer != NULL, "Allocated verify buffer");
    
    if (verify_buffer) {
        /* Read back and verify */
        ret = ext2_read_file(&g_ext2_fs, &test_inode, 0, verify_buffer, large_size);
        TEST_ASSERT(ret == (int)large_size, "Read back 3KB");
        
        if (ret == (int)large_size) {
            int match = 1;
            for (uint32_t i = 0; i < large_size; i++) {
                if (verify_buffer[i] != large_buffer[i]) {
                    match = 0;
                    hal_uart_puts("  Mismatch at offset ");
                    hal_uart_put_uint32(i);
                    hal_uart_puts("\n");
                    break;
                }
            }
            TEST_ASSERT(match, "Large data matches after write");
        }
        
        kfree(verify_buffer);
    }
    
    kfree(large_buffer);
}

/**
 * Test 8: Write data at various offsets
 */
void test_ext2_write_offset(void) {
    hal_uart_puts("\n[TEST] ext2 write at various offsets\n");
    
    /* Read root directory inode */
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Look up "test.txt" in root directory */
    uint32_t test_inode_num = ext2_lookup(&g_ext2_fs, &root_inode, "test.txt");
    TEST_ASSERT(test_inode_num != 0, "Found test.txt in root directory");
    
    if (test_inode_num == 0) {
        return;
    }
    
    /* Read test file inode */
    ext2_inode_t test_inode;
    ret = ext2_read_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Read test.txt inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Write at offset 100 */
    const char *data_100 = "AT_100";
    ret = ext2_write_file(&g_ext2_fs, &test_inode, 100, data_100, 6);
    TEST_ASSERT(ret == 6, "Wrote 6 bytes at offset 100");
    
    /* Write at offset 500 (different block) */
    const char *data_500 = "AT_500";
    ret = ext2_write_file(&g_ext2_fs, &test_inode, 500, data_500, 6);
    TEST_ASSERT(ret == 6, "Wrote 6 bytes at offset 500");
    
    /* Verify file size is at least 506 (may be larger from previous tests) */
    TEST_ASSERT(test_inode.i_size >= 506, "File size is at least 506 bytes");
    
    /* Write inode back to disk */
    ret = ext2_write_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Updated inode on disk");
    
    /* Verify data at offset 100 */
    char verify_buffer[16];
    ret = ext2_read_file(&g_ext2_fs, &test_inode, 100, verify_buffer, 6);
    if (ret == 6) {
        int match = 1;
        for (int i = 0; i < 6; i++) {
            if (verify_buffer[i] != data_100[i]) {
                match = 0;
                break;
            }
        }
        TEST_ASSERT(match, "Data at offset 100 matches");
    }
    
    /* Verify data at offset 500 */
    ret = ext2_read_file(&g_ext2_fs, &test_inode, 500, verify_buffer, 6);
    if (ret == 6) {
        int match = 1;
        for (int i = 0; i < 6; i++) {
            if (verify_buffer[i] != data_500[i]) {
                match = 0;
                break;
            }
        }
        TEST_ASSERT(match, "Data at offset 500 matches");
    }
}

/**
 * Test 9: Create a new file
 */
void test_ext2_create_file(void) {
    hal_uart_puts("\n[TEST] ext2 create new file\n");
    
    /* Create a new file in root directory */
    uint32_t new_inode = ext2_create_file(&g_ext2_fs, EXT2_ROOT_INO, "newfile.txt", 
                                          EXT2_S_IRUSR | EXT2_S_IWUSR);
    TEST_ASSERT(new_inode != 0, "Created new file");
    
    if (new_inode != 0) {
        /* Verify file exists */
        ext2_inode_t root_inode;
        int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
        TEST_ASSERT(ret == 0, "Read root inode");
        
        if (ret == 0) {
            uint32_t found = ext2_lookup(&g_ext2_fs, &root_inode, "newfile.txt");
            TEST_ASSERT(found == new_inode, "New file found in directory");
            
            /* Read the new file's inode */
            ext2_inode_t file_inode;
            ret = ext2_read_inode(&g_ext2_fs, new_inode, &file_inode);
            TEST_ASSERT(ret == 0, "Read new file inode");
            TEST_ASSERT((file_inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFREG, 
                        "New file is regular file");
            TEST_ASSERT(file_inode.i_size == 0, "New file has size 0");
        }
    }
}

/**
 * Test 10: Create a new directory
 */
void test_ext2_create_directory(void) {
    hal_uart_puts("\n[TEST] ext2 create new directory\n");
    
    /* Create a new directory in root */
    uint32_t new_dir_inode = ext2_create_dir(&g_ext2_fs, EXT2_ROOT_INO, "testdir",
                                             EXT2_S_IRUSR | EXT2_S_IWUSR | EXT2_S_IXUSR);
    TEST_ASSERT(new_dir_inode != 0, "Created new directory");
    
    if (new_dir_inode != 0) {
        /* Verify directory exists in root */
        ext2_inode_t root_inode;
        int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
        TEST_ASSERT(ret == 0, "Read root inode");
        
        if (ret == 0) {
            uint32_t found = ext2_lookup(&g_ext2_fs, &root_inode, "testdir");
            TEST_ASSERT(found == new_dir_inode, "New directory found in root");
            
            /* Read the new directory's inode */
            ext2_inode_t dir_inode;
            ret = ext2_read_inode(&g_ext2_fs, new_dir_inode, &dir_inode);
            TEST_ASSERT(ret == 0, "Read new directory inode");
            TEST_ASSERT((dir_inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR,
                        "New entry is a directory");
            TEST_ASSERT(dir_inode.i_links_count == 2, "Directory has 2 links (. and parent)");
        }
    }
}

/**
 * Test 11: Create file in subdirectory
 */
void test_ext2_create_in_subdir(void) {
    hal_uart_puts("\n[TEST] ext2 create file in subdirectory\n");
    
    /* First, find the testdir we created */
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "Read root inode");
    
    if (ret != 0) {
        return;
    }
    
    uint32_t testdir_inode_num = ext2_lookup(&g_ext2_fs, &root_inode, "testdir");
    TEST_ASSERT(testdir_inode_num != 0, "Found testdir");
    
    if (testdir_inode_num == 0) {
        return;
    }
    
    /* Create a file in testdir */
    uint32_t subfile_inode = ext2_create_file(&g_ext2_fs, testdir_inode_num, "subfile.txt",
                                              EXT2_S_IRUSR | EXT2_S_IWUSR);
    TEST_ASSERT(subfile_inode != 0, "Created file in subdirectory");
    
    if (subfile_inode != 0) {
        /* Verify file exists in subdirectory */
        ext2_inode_t testdir_inode;
        ret = ext2_read_inode(&g_ext2_fs, testdir_inode_num, &testdir_inode);
        TEST_ASSERT(ret == 0, "Read testdir inode");
        
        if (ret == 0) {
            uint32_t found = ext2_lookup(&g_ext2_fs, &testdir_inode, "subfile.txt");
            TEST_ASSERT(found == subfile_inode, "Subfile found in testdir");
        }
    }
}

/**
 * Run all ext2 tests
 */
void test_ext2_all(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("       ext2 Filesystem Tests\n");
    hal_uart_puts("========================================\n");
    
    tests_passed = 0;
    tests_failed = 0;
    
    /* Run tests in sequence */
    test_ext2_mount();
    test_ext2_read_root_inode();
    test_ext2_list_root_dir();
    test_ext2_read_file();
    test_ext2_write_file();
    test_ext2_append_file();
    test_ext2_write_large_file();
    test_ext2_write_offset();
    test_ext2_create_file();
    test_ext2_create_directory();
    test_ext2_create_in_subdir();
    
    /* Unmount filesystem */
    ext2_unmount(&g_ext2_fs);
    
    /* Print summary */
    hal_uart_puts("\n========================================\n");
    hal_uart_puts("Tests passed: ");
    hal_uart_put_uint32(tests_passed);
    hal_uart_puts(", Tests failed: ");
    hal_uart_put_uint32(tests_failed);
    hal_uart_puts("\n");
    
    if (tests_failed == 0) {
        hal_uart_puts("*** ALL TESTS PASSED ***\n");
    } else {
        hal_uart_puts("*** SOME TESTS FAILED ***\n");
    }
    hal_uart_puts("========================================\n");
}

