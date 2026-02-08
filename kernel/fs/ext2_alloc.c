/*
 * ext2_alloc.c - ext2 block and inode allocation
 */

#include "../include/fs/ext2.h"
#include "../include/drivers/virtio_blk.h"
#include "../include/mm/kmalloc.h"
#include "../include/hal/hal_uart.h"
#include "../include/kernel/errno.h"
#include "../include/kernel/constants.h"
#include <stddef.h>

/**
 * Read a block from the block device
 */
static int read_block(void *device, uint32_t block_num, void *buffer, uint32_t block_size) {
    (void)device;
    
    uint32_t sector = (block_num * block_size) / SECTOR_SIZE;
    uint32_t num_sectors = block_size / SECTOR_SIZE;
    
    for (uint32_t i = 0; i < num_sectors; i++) {
        int ret = virtio_blk_read(sector + i, (uint8_t *)buffer + ((size_t)i * SECTOR_SIZE), 1);
        if (ret != 1) {
            set_errno(THUNDEROS_EIO);
            return -1;
        }
    }
    
    clear_errno();
    return 0;
}

/**
 * Write a block to the block device
 */
static int write_block(void *device, uint32_t block_num, const void *buffer, uint32_t block_size) {
    (void)device;
    
    uint32_t sector = (block_num * block_size) / SECTOR_SIZE;
    uint32_t num_sectors = block_size / SECTOR_SIZE;
    
    for (uint32_t i = 0; i < num_sectors; i++) {
        int ret = virtio_blk_write(sector + i, (const uint8_t *)buffer + ((size_t)i * SECTOR_SIZE), 1);
        if (ret != 1) {
            set_errno(THUNDEROS_EIO);
            return -1;
        }
    }
    
    clear_errno();
    return 0;
}

/**
 * Allocate a block from block bitmap
 * Returns block number, or 0 on failure
 */
uint32_t ext2_alloc_block(ext2_fs_t *fs, uint32_t group) {
    if (!fs || group >= fs->num_groups) {
        set_errno(THUNDEROS_EINVAL);
        return 0;
    }
    
    ext2_group_desc_t *gd = &fs->group_desc[group];
    
    /* Check if group has free blocks */
    if (gd->bg_free_blocks_count == 0) {
        set_errno(THUNDEROS_EFS_NOBLK);
        return 0;
    }
    
    /* Read block bitmap */
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) {
        set_errno(THUNDEROS_ENOMEM);
        return 0;
    }
    
    if (read_block(fs->device, gd->bg_block_bitmap, bitmap, fs->block_size) != 0) {
        kfree(bitmap);
        /* errno already set by read_block */
        return 0;
    }
    
    /* Find first free block */
    uint32_t blocks_per_group = fs->superblock->s_blocks_per_group;
    for (uint32_t i = 0; i < blocks_per_group; i++) {
        uint32_t byte = i / BITS_PER_BYTE;
        uint32_t bit = i % BITS_PER_BYTE;
        
        if (!(bitmap[byte] & (1 << bit))) {
            /* Found free block */
            bitmap[byte] |= (1 << bit);
            
            /* Write bitmap back */
            if (write_block(fs->device, gd->bg_block_bitmap, bitmap, fs->block_size) != 0) {
                kfree(bitmap);
                /* errno already set by write_block */
                return 0;
            }
            
            /* Update group descriptor */
            gd->bg_free_blocks_count--;
            
            /* Update superblock */
            fs->superblock->s_free_blocks_count--;
            
            kfree(bitmap);
            clear_errno();
            return group * blocks_per_group + i;
        }
    }
    
    kfree(bitmap);
    set_errno(THUNDEROS_EFS_NOBLK);
    return 0;
}

/**
 * Free a block to block bitmap
 */
int ext2_free_block(ext2_fs_t *fs, uint32_t block_num) {
    if (!fs || block_num == 0) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Determine which group contains this block */
    uint32_t blocks_per_group = fs->superblock->s_blocks_per_group;
    uint32_t group = block_num / blocks_per_group;
    uint32_t offset = block_num % blocks_per_group;
    
    if (group >= fs->num_groups) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    ext2_group_desc_t *gd = &fs->group_desc[group];
    
    /* Read block bitmap */
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    if (read_block(fs->device, gd->bg_block_bitmap, bitmap, fs->block_size) != 0) {
        kfree(bitmap);
        /* errno already set by read_block */
        return -1;
    }
    
    /* Clear the bit */
    uint32_t byte = offset / BITS_PER_BYTE;
    uint32_t bit = offset % BITS_PER_BYTE;
    bitmap[byte] &= ~(1 << bit);
    
    /* Write bitmap back */
    if (write_block(fs->device, gd->bg_block_bitmap, bitmap, fs->block_size) != 0) {
        kfree(bitmap);
        /* errno already set by write_block */
        return -1;
    }
    
    /* Update group descriptor */
    gd->bg_free_blocks_count++;
    
    /* Update superblock */
    fs->superblock->s_free_blocks_count++;
    
    kfree(bitmap);
    clear_errno();
    return 0;
}

/**
 * Allocate an inode from inode bitmap
 * Returns inode number, or 0 on failure
 */
uint32_t ext2_alloc_inode(ext2_fs_t *fs, uint32_t group) {
    if (!fs || group >= fs->num_groups) {
        set_errno(THUNDEROS_EINVAL);
        return 0;
    }
    
    ext2_group_desc_t *gd = &fs->group_desc[group];
    
    /* Check if group has free inodes */
    if (gd->bg_free_inodes_count == 0) {
        set_errno(THUNDEROS_EFS_NOINODE);
        return 0;
    }
    
    /* Read inode bitmap */
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) {
        set_errno(THUNDEROS_ENOMEM);
        return 0;
    }
    
    if (read_block(fs->device, gd->bg_inode_bitmap, bitmap, fs->block_size) != 0) {
        kfree(bitmap);
        /* errno already set by read_block */
        return 0;
    }
    
    /* Find first free inode */
    uint32_t inodes_per_group = fs->superblock->s_inodes_per_group;
    for (uint32_t i = 0; i < inodes_per_group; i++) {
        uint32_t byte = i / BITS_PER_BYTE;
        uint32_t bit = i % BITS_PER_BYTE;
        
        if (!(bitmap[byte] & (1 << bit))) {
            /* Found free inode */
            bitmap[byte] |= (1 << bit);
            
            /* Write bitmap back */
            if (write_block(fs->device, gd->bg_inode_bitmap, bitmap, fs->block_size) != 0) {
                kfree(bitmap);
                /* errno already set by write_block */
                return 0;
            }
            
            /* Update group descriptor */
            gd->bg_free_inodes_count--;
            
            /* Update superblock */
            fs->superblock->s_free_inodes_count--;
            
            kfree(bitmap);
            clear_errno();
            return group * inodes_per_group + i + 1;  /* Inodes are 1-indexed */
        }
    }
    
    kfree(bitmap);
    set_errno(THUNDEROS_EFS_NOINODE);
    return 0;
}

/**
 * Free an inode to inode bitmap
 */
int ext2_free_inode(ext2_fs_t *fs, uint32_t inode_num) {
    if (!fs || inode_num == 0) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Inodes are 1-indexed */
    uint32_t inode_index = inode_num - 1;
    
    /* Determine which group contains this inode */
    uint32_t inodes_per_group = fs->superblock->s_inodes_per_group;
    uint32_t group = inode_index / inodes_per_group;
    uint32_t offset = inode_index % inodes_per_group;
    
    if (group >= fs->num_groups) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    ext2_group_desc_t *gd = &fs->group_desc[group];
    
    /* Read inode bitmap */
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    if (read_block(fs->device, gd->bg_inode_bitmap, bitmap, fs->block_size) != 0) {
        kfree(bitmap);
        /* errno already set by read_block */
        return -1;
    }
    
    /* Clear the bit */
    uint32_t byte = offset / BITS_PER_BYTE;
    uint32_t bit = offset % BITS_PER_BYTE;
    bitmap[byte] &= ~(1 << bit);
    
    /* Write bitmap back */
    if (write_block(fs->device, gd->bg_inode_bitmap, bitmap, fs->block_size) != 0) {
        kfree(bitmap);
        /* errno already set by write_block */
        return -1;
    }
    
    /* Update group descriptor */
    gd->bg_free_inodes_count++;
    
    /* Update superblock */
    fs->superblock->s_free_inodes_count++;
    
    kfree(bitmap);
    clear_errno();
    return 0;
}
