/*
 * ext2_write.c - ext2 write operations
 */

#include "../include/fs/ext2.h"
#include "../include/drivers/virtio_blk.h"
#include "../include/mm/kmalloc.h"
#include "../include/hal/hal_uart.h"
#include <stddef.h>

/**
 * Read a block from the block device
 */
static int read_block(void *device, uint32_t block_num, void *buffer, uint32_t block_size) {
    (void)device;
    
    uint32_t sector = (block_num * block_size) / 512;
    uint32_t num_sectors = block_size / 512;
    
    for (uint32_t i = 0; i < num_sectors; i++) {
        int ret = virtio_blk_read(sector + i, (uint8_t *)buffer + (i * 512), 1);
        if (ret != 1) {
            return -1;
        }
    }
    
    return 0;
}

/**
 * Write a block to the block device
 */
static int write_block(void *device, uint32_t block_num, const void *buffer, uint32_t block_size) {
    (void)device;
    
    uint32_t sector = (block_num * block_size) / 512;
    uint32_t num_sectors = block_size / 512;
    
    for (uint32_t i = 0; i < num_sectors; i++) {
        int ret = virtio_blk_write(sector + i, (const uint8_t *)buffer + (i * 512), 1);
        if (ret != 1) {
            return -1;
        }
    }
    
    return 0;
}

/**
 * Get or allocate a block number for a given file block index
 * Handles direct, indirect, and double-indirect blocks
 * If allocate is true, allocates blocks as needed
 */
static uint32_t get_or_alloc_block(ext2_fs_t *fs, ext2_inode_t *inode, 
                                    uint32_t file_block, int allocate) {
    uint32_t *indirect_buffer = NULL;
    uint32_t *dindirect_buffer = NULL;
    uint32_t block_num = 0;
    uint32_t ptrs_per_block = fs->block_size / sizeof(uint32_t);
    
    /* Direct blocks */
    if (file_block < EXT2_NDIR_BLOCKS) {
        if (inode->i_block[file_block] == 0 && allocate) {
            /* Allocate new block */
            inode->i_block[file_block] = ext2_alloc_block(fs, 0);
            if (inode->i_block[file_block] == 0) {
                return 0;
            }
            /* Zero the new block */
            uint8_t *zero_buf = (uint8_t *)kmalloc(fs->block_size);
            if (zero_buf) {
                for (uint32_t i = 0; i < fs->block_size; i++) {
                    zero_buf[i] = 0;
                }
                write_block(fs->device, inode->i_block[file_block], zero_buf, fs->block_size);
                kfree(zero_buf);
            }
        }
        return inode->i_block[file_block];
    }
    
    file_block -= EXT2_NDIR_BLOCKS;
    
    /* Indirect block */
    if (file_block < ptrs_per_block) {
        /* Allocate indirect block if needed */
        if (inode->i_block[EXT2_IND_BLOCK] == 0) {
            if (!allocate) {
                return 0;
            }
            inode->i_block[EXT2_IND_BLOCK] = ext2_alloc_block(fs, 0);
            if (inode->i_block[EXT2_IND_BLOCK] == 0) {
                return 0;
            }
            /* Zero the indirect block */
            uint8_t *zero_buf = (uint8_t *)kmalloc(fs->block_size);
            if (zero_buf) {
                for (uint32_t i = 0; i < fs->block_size; i++) {
                    zero_buf[i] = 0;
                }
                write_block(fs->device, inode->i_block[EXT2_IND_BLOCK], zero_buf, fs->block_size);
                kfree(zero_buf);
            }
        }
        
        /* Read indirect block */
        indirect_buffer = (uint32_t *)kmalloc(fs->block_size);
        if (!indirect_buffer) {
            return 0;
        }
        
        if (read_block(fs->device, inode->i_block[EXT2_IND_BLOCK], 
                      indirect_buffer, fs->block_size) != 0) {
            kfree(indirect_buffer);
            return 0;
        }
        
        /* Check if data block needs allocation */
        if (indirect_buffer[file_block] == 0 && allocate) {
            indirect_buffer[file_block] = ext2_alloc_block(fs, 0);
            if (indirect_buffer[file_block] == 0) {
                kfree(indirect_buffer);
                return 0;
            }
            /* Write updated indirect block */
            write_block(fs->device, inode->i_block[EXT2_IND_BLOCK], 
                       indirect_buffer, fs->block_size);
            /* Zero the new data block */
            uint8_t *zero_buf = (uint8_t *)kmalloc(fs->block_size);
            if (zero_buf) {
                for (uint32_t i = 0; i < fs->block_size; i++) {
                    zero_buf[i] = 0;
                }
                write_block(fs->device, indirect_buffer[file_block], zero_buf, fs->block_size);
                kfree(zero_buf);
            }
        }
        
        block_num = indirect_buffer[file_block];
        kfree(indirect_buffer);
        return block_num;
    }
    
    file_block -= ptrs_per_block;
    
    /* Double-indirect block */
    if (file_block < ptrs_per_block * ptrs_per_block) {
        /* Allocate double-indirect block if needed */
        if (inode->i_block[EXT2_DIND_BLOCK] == 0) {
            if (!allocate) {
                return 0;
            }
            inode->i_block[EXT2_DIND_BLOCK] = ext2_alloc_block(fs, 0);
            if (inode->i_block[EXT2_DIND_BLOCK] == 0) {
                return 0;
            }
            /* Zero the double-indirect block */
            uint8_t *zero_buf = (uint8_t *)kmalloc(fs->block_size);
            if (zero_buf) {
                for (uint32_t i = 0; i < fs->block_size; i++) {
                    zero_buf[i] = 0;
                }
                write_block(fs->device, inode->i_block[EXT2_DIND_BLOCK], zero_buf, fs->block_size);
                kfree(zero_buf);
            }
        }
        
        /* Read double-indirect block */
        dindirect_buffer = (uint32_t *)kmalloc(fs->block_size);
        if (!dindirect_buffer) {
            return 0;
        }
        
        if (read_block(fs->device, inode->i_block[EXT2_DIND_BLOCK], 
                      dindirect_buffer, fs->block_size) != 0) {
            kfree(dindirect_buffer);
            return 0;
        }
        
        /* Get or allocate indirect block */
        uint32_t indirect_index = file_block / ptrs_per_block;
        uint32_t indirect_block_num = dindirect_buffer[indirect_index];
        
        if (indirect_block_num == 0 && allocate) {
            indirect_block_num = ext2_alloc_block(fs, 0);
            if (indirect_block_num == 0) {
                kfree(dindirect_buffer);
                return 0;
            }
            dindirect_buffer[indirect_index] = indirect_block_num;
            /* Write updated double-indirect block */
            write_block(fs->device, inode->i_block[EXT2_DIND_BLOCK], 
                       dindirect_buffer, fs->block_size);
            /* Zero the new indirect block */
            uint8_t *zero_buf = (uint8_t *)kmalloc(fs->block_size);
            if (zero_buf) {
                for (uint32_t i = 0; i < fs->block_size; i++) {
                    zero_buf[i] = 0;
                }
                write_block(fs->device, indirect_block_num, zero_buf, fs->block_size);
                kfree(zero_buf);
            }
        }
        
        kfree(dindirect_buffer);
        
        if (indirect_block_num == 0) {
            return 0;
        }
        
        /* Read indirect block */
        indirect_buffer = (uint32_t *)kmalloc(fs->block_size);
        if (!indirect_buffer) {
            return 0;
        }
        
        if (read_block(fs->device, indirect_block_num, 
                      indirect_buffer, fs->block_size) != 0) {
            kfree(indirect_buffer);
            return 0;
        }
        
        /* Get or allocate data block */
        uint32_t data_index = file_block % ptrs_per_block;
        if (indirect_buffer[data_index] == 0 && allocate) {
            indirect_buffer[data_index] = ext2_alloc_block(fs, 0);
            if (indirect_buffer[data_index] == 0) {
                kfree(indirect_buffer);
                return 0;
            }
            /* Write updated indirect block */
            write_block(fs->device, indirect_block_num, indirect_buffer, fs->block_size);
            /* Zero the new data block */
            uint8_t *zero_buf = (uint8_t *)kmalloc(fs->block_size);
            if (zero_buf) {
                for (uint32_t i = 0; i < fs->block_size; i++) {
                    zero_buf[i] = 0;
                }
                write_block(fs->device, indirect_buffer[data_index], zero_buf, fs->block_size);
                kfree(zero_buf);
            }
        }
        
        block_num = indirect_buffer[data_index];
        kfree(indirect_buffer);
        return block_num;
    }
    
    /* Triple-indirect block - not implemented */
    hal_uart_puts("ext2: Triple-indirect blocks not yet supported\n");
    return 0;
}

/**
 * Write data to a file
 * Returns number of bytes written, or -1 on error
 */
int ext2_write_file(ext2_fs_t *fs, ext2_inode_t *inode, uint32_t offset,
                    const void *buffer, uint32_t size) {
    if (!fs || !inode || !buffer || size == 0) {
        hal_uart_puts("ext2: Invalid parameters to ext2_write_file\n");
        return -1;
    }
    
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t bytes_written = 0;
    
    /* Allocate temporary buffer for block operations */
    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) {
        hal_uart_puts("ext2: Failed to allocate block buffer\n");
        return -1;
    }
    
    while (bytes_written < size) {
        /* Calculate which file block we need */
        uint32_t file_block = (offset + bytes_written) / fs->block_size;
        uint32_t block_offset = (offset + bytes_written) % fs->block_size;
        
        /* Get or allocate the actual block number on disk */
        uint32_t block_num = get_or_alloc_block(fs, inode, file_block, 1);
        if (block_num == 0) {
            hal_uart_puts("ext2: Failed to allocate block for file write\n");
            kfree(block_buffer);
            return -1;
        }
        
        /* Calculate how much to write in this block */
        uint32_t to_write = fs->block_size - block_offset;
        if (to_write > size - bytes_written) {
            to_write = size - bytes_written;
        }
        
        /* If we're writing a partial block, read it first */
        if (block_offset != 0 || to_write < fs->block_size) {
            if (read_block(fs->device, block_num, block_buffer, fs->block_size) != 0) {
                /* If read fails, zero the buffer (might be a new block) */
                for (uint32_t i = 0; i < fs->block_size; i++) {
                    block_buffer[i] = 0;
                }
            }
        }
        
        /* Copy data into block buffer */
        for (uint32_t i = 0; i < to_write; i++) {
            block_buffer[block_offset + i] = src[bytes_written + i];
        }
        
        /* Write the block back to disk */
        if (write_block(fs->device, block_num, block_buffer, fs->block_size) != 0) {
            hal_uart_puts("ext2: Failed to write data block ");
            hal_uart_put_uint32(block_num);
            hal_uart_puts("\n");
            kfree(block_buffer);
            return -1;
        }
        
        bytes_written += to_write;
    }
    
    /* Update inode size if we wrote past the end */
    if (offset + bytes_written > inode->i_size) {
        inode->i_size = offset + bytes_written;
    }
    
    /* Update i_blocks (number of 512-byte blocks) */
    /* For simplicity, recalculate based on allocated blocks */
    uint32_t total_blocks = (inode->i_size + fs->block_size - 1) / fs->block_size;
    inode->i_blocks = (total_blocks * fs->block_size) / 512;
    
    kfree(block_buffer);
    return bytes_written;
}

/**
 * Create a new file in a directory (stub)
 */
int ext2_create_file(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name, uint32_t mode) {
    (void)fs;
    (void)dir_inode;
    (void)name;
    (void)mode;
    
    hal_uart_puts("ext2: create_file not yet implemented\n");
    return -1;
}

/**
 * Create a new directory (stub)
 */
int ext2_create_dir(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name, uint32_t mode) {
    (void)fs;
    (void)dir_inode;
    (void)name;
    (void)mode;
    
    hal_uart_puts("ext2: create_dir not yet implemented\n");
    return -1;
}

/**
 * Remove a file from a directory (stub)
 */
int ext2_remove_file(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name) {
    (void)fs;
    (void)dir_inode;
    (void)name;
    
    hal_uart_puts("ext2: remove_file not yet implemented\n");
    return -1;
}

/**
 * Remove a directory (stub)
 */
int ext2_remove_dir(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name) {
    (void)fs;
    (void)dir_inode;
    (void)name;
    
    hal_uart_puts("ext2: remove_dir not yet implemented\n");
    return -1;
}
