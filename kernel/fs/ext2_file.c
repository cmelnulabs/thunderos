/*
 * ext2_file.c - ext2 file read operations
 */

#include "../include/fs/ext2.h"
#include "../include/drivers/virtio_blk.h"
#include "../include/mm/kmalloc.h"
#include "../include/hal/hal_uart.h"
#include "../include/kernel/errno.h"
#include <stddef.h>

/**
 * Read a block from the block device
 */
static int read_block(void *device, uint32_t block_num, void *buffer, uint32_t block_size) {
    (void)device;  /* Device parameter unused - we use global device */
    
    /* Calculate sector number (sectors are 512 bytes) */
    uint32_t sector = (block_num * block_size) / 512;
    uint32_t num_sectors = block_size / 512;
    
    /* Read sectors */
    for (uint32_t i = 0; i < num_sectors; i++) {
        int ret = virtio_blk_read(sector + i, 
                                  (uint8_t *)buffer + (i * 512), 1);
        if (ret != 1) {
            set_errno(THUNDEROS_EIO);
            return -1;
        }
    }
    
    clear_errno();
    return 0;
}

/**
 * Get the block number for a given file block index
 * Handles direct, indirect, double-indirect, and triple-indirect blocks
 */
static uint32_t get_block_number(ext2_fs_t *fs, ext2_inode_t *inode, uint32_t file_block) {
    uint32_t *indirect_buffer = NULL;
    uint32_t *dindirect_buffer = NULL;
    uint32_t block_num = 0;
    uint32_t ptrs_per_block = fs->block_size / sizeof(uint32_t);
    
    /* Direct blocks */
    if (file_block < EXT2_NDIR_BLOCKS) {
        return inode->i_block[file_block];
    }
    
    file_block -= EXT2_NDIR_BLOCKS;
    
    /* Indirect block */
    if (file_block < ptrs_per_block) {
        if (inode->i_block[EXT2_IND_BLOCK] == 0) {
            return 0;
        }
        
        indirect_buffer = (uint32_t *)kmalloc(fs->block_size);
        if (!indirect_buffer) {
            set_errno(THUNDEROS_ENOMEM);
            return 0;
        }
        
        if (read_block(fs->device, inode->i_block[EXT2_IND_BLOCK], 
                      indirect_buffer, fs->block_size) != 0) {
            kfree(indirect_buffer);
            /* errno already set by read_block */
            return 0;
        }
        
        block_num = indirect_buffer[file_block];
        kfree(indirect_buffer);
        return block_num;
    }
    
    file_block -= ptrs_per_block;
    
    /* Double-indirect block */
    if (file_block < ptrs_per_block * ptrs_per_block) {
        if (inode->i_block[EXT2_DIND_BLOCK] == 0) {
            return 0;
        }
        
        /* Read double-indirect block */
        dindirect_buffer = (uint32_t *)kmalloc(fs->block_size);
        if (!dindirect_buffer) {
            set_errno(THUNDEROS_ENOMEM);
            return 0;
        }
        
        if (read_block(fs->device, inode->i_block[EXT2_DIND_BLOCK], 
                      dindirect_buffer, fs->block_size) != 0) {
            kfree(dindirect_buffer);
            /* errno already set by read_block */
            return 0;
        }
        
        /* Get indirect block number */
        uint32_t indirect_index = file_block / ptrs_per_block;
        uint32_t indirect_block_num = dindirect_buffer[indirect_index];
        kfree(dindirect_buffer);
        
        if (indirect_block_num == 0) {
            return 0;
        }
        
        /* Read indirect block */
        indirect_buffer = (uint32_t *)kmalloc(fs->block_size);
        if (!indirect_buffer) {
            set_errno(THUNDEROS_ENOMEM);
            return 0;
        }
        
        if (read_block(fs->device, indirect_block_num, 
                      indirect_buffer, fs->block_size) != 0) {
            kfree(indirect_buffer);
            /* errno already set by read_block */
            return 0;
        }
        
        /* Get data block number */
        uint32_t data_index = file_block % ptrs_per_block;
        block_num = indirect_buffer[data_index];
        kfree(indirect_buffer);
        return block_num;
    }
    
    /* Triple-indirect block - not implemented for now */
    /* Most files won't need this (would need to be > 4GB on 1KB blocks) */
    hal_uart_puts("ext2: Triple-indirect blocks not yet supported\n");
    set_errno(THUNDEROS_EFBIG);
    return 0;
}

/**
 * Read data from a file
 */
int ext2_read_file(ext2_fs_t *fs, ext2_inode_t *inode, uint32_t offset, 
                   void *buffer, uint32_t size) {
    if (!fs || !inode || !buffer) {
        hal_uart_puts("ext2: Invalid parameters to ext2_read_file\n");
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Check if offset is beyond file size */
    if (offset >= inode->i_size) {
        clear_errno();
        return 0;
    }
    
    /* Adjust size if it would read past end of file */
    if (offset + size > inode->i_size) {
        size = inode->i_size - offset;
    }
    
    uint8_t *dest = (uint8_t *)buffer;
    uint32_t bytes_read = 0;
    
    /* Allocate temporary buffer for block reads */
    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) {
        hal_uart_puts("ext2: Failed to allocate block buffer\n");
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    while (bytes_read < size) {
        /* Calculate which file block we need */
        uint32_t file_block = (offset + bytes_read) / fs->block_size;
        uint32_t block_offset = (offset + bytes_read) % fs->block_size;
        
        /* Get the actual block number on disk */
        uint32_t block_num = get_block_number(fs, inode, file_block);
        if (block_num == 0) {
            /* Sparse file - zero block */
            uint32_t to_copy = fs->block_size - block_offset;
            if (to_copy > size - bytes_read) {
                to_copy = size - bytes_read;
            }
            for (uint32_t i = 0; i < to_copy; i++) {
                dest[bytes_read + i] = 0;
            }
            bytes_read += to_copy;
            continue;
        }
        
        /* Read the block */
        if (read_block(fs->device, block_num, block_buffer, fs->block_size) != 0) {
            hal_uart_puts("ext2: Failed to read data block ");
            hal_uart_put_uint32(block_num);
            hal_uart_puts("\n");
            kfree(block_buffer);
            /* errno already set by read_block */
            return -1;
        }
        
        /* Copy the requested portion of the block */
        uint32_t to_copy = fs->block_size - block_offset;
        if (to_copy > size - bytes_read) {
            to_copy = size - bytes_read;
        }
        
        for (uint32_t i = 0; i < to_copy; i++) {
            dest[bytes_read + i] = block_buffer[block_offset + i];
        }
        
        bytes_read += to_copy;
    }
    
    kfree(block_buffer);
    clear_errno();
    return bytes_read;
}
