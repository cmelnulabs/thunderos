/*
 * ext2_inode.c - ext2 inode operations
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
    (void)device;  /* Device parameter unused - we use global device */
    
    /* Calculate sector number (sectors are 512 bytes) */
    uint32_t sector = (block_num * block_size) / 512;
    uint32_t num_sectors = block_size / 512;
    
    /* Read sectors */
    for (uint32_t i = 0; i < num_sectors; i++) {
        int ret = virtio_blk_read(sector + i, 
                                  (uint8_t *)buffer + (i * 512), 1);
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
 * Read an inode from disk
 */
int ext2_read_inode(ext2_fs_t *fs, uint32_t inode_num, ext2_inode_t *inode) {
    if (!fs || !inode || inode_num == 0) {
        hal_uart_puts("ext2: Invalid parameters to ext2_read_inode\n");
        return -1;
    }
    
    if (inode_num > fs->superblock->s_inodes_count) {
        hal_uart_puts("ext2: Inode number ");
        hal_uart_put_uint32(inode_num);
        hal_uart_puts(" out of range\n");
        return -1;
    }
    
    /* Inodes are numbered starting from 1, so subtract 1 for index */
    uint32_t inode_index = inode_num - 1;
    
    /* Determine which block group contains this inode */
    uint32_t group = inode_index / fs->superblock->s_inodes_per_group;
    
    if (group >= fs->num_groups) {
        hal_uart_puts("ext2: Block group ");
        hal_uart_put_uint32(group);
        hal_uart_puts(" out of range\n");
        return -1;
    }
    
    /* Get the group descriptor */
    ext2_group_desc_t *gd = &fs->group_desc[group];
    
    /* Calculate inode index within the group */
    uint32_t inode_table_index = inode_index % fs->superblock->s_inodes_per_group;
    
    /* Calculate which block in the inode table contains this inode */
    uint32_t inode_block = gd->bg_inode_table + (inode_table_index / fs->inodes_per_block);
    
    /* Calculate offset within the block */
    uint32_t inode_size = fs->superblock->s_inode_size > 0 ? 
                          fs->superblock->s_inode_size : EXT2_INODE_SIZE;
    uint32_t block_offset = (inode_table_index % fs->inodes_per_block) * inode_size;
    
    /* Allocate temporary buffer for the block */
    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) {
        hal_uart_puts("ext2: Failed to allocate block buffer for inode\n");
        return -1;
    }
    
    /* Read the block containing the inode */
    int ret = read_block(fs->device, inode_block, block_buffer, fs->block_size);
    if (ret != 0) {
        hal_uart_puts("ext2: Failed to read inode block ");
        hal_uart_put_uint32(inode_block);
        hal_uart_puts("\n");
        kfree(block_buffer);
        return -1;
    }
    
    /* Copy the inode data */
    uint8_t *src = block_buffer + block_offset;
    uint8_t *dst = (uint8_t *)inode;
    for (uint32_t i = 0; i < sizeof(ext2_inode_t); i++) {
        dst[i] = src[i];
    }
    
    kfree(block_buffer);
    return 0;
}

/**
 * Write an inode back to disk
 */
int ext2_write_inode(ext2_fs_t *fs, uint32_t inode_num, ext2_inode_t *inode) {
    if (!fs || !inode || inode_num == 0) {
        hal_uart_puts("ext2: Invalid parameters to ext2_write_inode\n");
        return -1;
    }
    
    if (inode_num > fs->superblock->s_inodes_count) {
        hal_uart_puts("ext2: Inode number ");
        hal_uart_put_uint32(inode_num);
        hal_uart_puts(" out of range\n");
        return -1;
    }
    
    /* Inodes are numbered starting from 1, so subtract 1 for index */
    uint32_t inode_index = inode_num - 1;
    
    /* Determine which block group contains this inode */
    uint32_t group = inode_index / fs->superblock->s_inodes_per_group;
    
    if (group >= fs->num_groups) {
        hal_uart_puts("ext2: Block group ");
        hal_uart_put_uint32(group);
        hal_uart_puts(" out of range\n");
        return -1;
    }
    
    /* Get the group descriptor */
    ext2_group_desc_t *gd = &fs->group_desc[group];
    
    /* Calculate inode index within the group */
    uint32_t inode_table_index = inode_index % fs->superblock->s_inodes_per_group;
    
    /* Calculate which block in the inode table contains this inode */
    uint32_t inode_block = gd->bg_inode_table + (inode_table_index / fs->inodes_per_block);
    
    /* Calculate offset within the block */
    uint32_t inode_size = fs->superblock->s_inode_size > 0 ? 
                          fs->superblock->s_inode_size : EXT2_INODE_SIZE;
    uint32_t block_offset = (inode_table_index % fs->inodes_per_block) * inode_size;
    
    /* Allocate temporary buffer for the block */
    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) {
        hal_uart_puts("ext2: Failed to allocate block buffer for inode write\n");
        return -1;
    }
    
    /* Read the block containing the inode first */
    int ret = read_block(fs->device, inode_block, block_buffer, fs->block_size);
    if (ret != 0) {
        hal_uart_puts("ext2: Failed to read inode block for write ");
        hal_uart_put_uint32(inode_block);
        hal_uart_puts("\n");
        kfree(block_buffer);
        return -1;
    }
    
    /* Update the inode data in the buffer */
    uint8_t *src = (uint8_t *)inode;
    uint8_t *dst = block_buffer + block_offset;
    for (uint32_t i = 0; i < sizeof(ext2_inode_t); i++) {
        dst[i] = src[i];
    }
    
    /* Write the block back to disk */
    ret = write_block(fs->device, inode_block, block_buffer, fs->block_size);
    if (ret != 0) {
        hal_uart_puts("ext2: Failed to write inode block ");
        hal_uart_put_uint32(inode_block);
        hal_uart_puts("\n");
        kfree(block_buffer);
        return -1;
    }
    
    kfree(block_buffer);
    return 0;
}

