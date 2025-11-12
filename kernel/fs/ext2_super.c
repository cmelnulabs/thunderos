/*
 * ext2_super.c - ext2 superblock and mount/unmount operations
 */

#include "../include/fs/ext2.h"
#include "../include/drivers/virtio_blk.h"
#include "../include/mm/kmalloc.h"
#include "../include/mm/dma.h"
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
 * Initialize and mount an ext2 filesystem
 */
int ext2_mount(ext2_fs_t *fs, void *device) {
    if (!fs || !device) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Initialize filesystem context */
    fs->device = device;
    fs->superblock = NULL;
    fs->group_desc = NULL;
    
    /* Allocate buffer for superblock (1024 bytes) */
    fs->superblock = (ext2_superblock_t *)kmalloc(EXT2_SUPERBLOCK_SIZE);
    if (!fs->superblock) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Read first two sectors (superblock starts at byte 1024) */
    /* Sector 0 = bytes 0-511, Sector 1 = bytes 512-1023, Sector 2 = bytes 1024-1535 */
    uint8_t *sb_buffer = (uint8_t *)fs->superblock;
    
    /* Read sector 0 first to see if device works */
    int ret = virtio_blk_read(0, sb_buffer, 1);
    if (ret != 1) {
        kfree(fs->superblock);
        fs->superblock = NULL;
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Read sector 2 (offset 1024) into first 512 bytes of superblock */
    ret = virtio_blk_read(2, sb_buffer, 1);
    if (ret != 1) {
        kfree(fs->superblock);
        fs->superblock = NULL;
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Read sector 3 (offset 1536) into second 512 bytes of superblock */
    ret = virtio_blk_read(3, sb_buffer + 512, 1);
    if (ret != 1) {
        kfree(fs->superblock);
        fs->superblock = NULL;
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Verify magic number */
    if (fs->superblock->s_magic != EXT2_SUPER_MAGIC) {
        kfree(fs->superblock);
        fs->superblock = NULL;
        RETURN_ERRNO(THUNDEROS_EFS_BADSUPER);
    }
    
    /* Calculate block size */
    fs->block_size = 1024 << fs->superblock->s_log_block_size;
    
    /* Validate block size */
    if (fs->block_size < EXT2_MIN_BLOCK_SIZE || fs->block_size > EXT2_MAX_BLOCK_SIZE) {
        kfree(fs->superblock);
        fs->superblock = NULL;
        RETURN_ERRNO(THUNDEROS_EFS_INVAL);
    }
    
    /* Calculate number of block groups */
    fs->num_groups = (fs->superblock->s_blocks_count + fs->superblock->s_blocks_per_group - 1) 
                     / fs->superblock->s_blocks_per_group;
    
    /* Calculate inodes per block */
    uint32_t inode_size = fs->superblock->s_inode_size > 0 ? 
                          fs->superblock->s_inode_size : EXT2_INODE_SIZE;
    fs->inodes_per_block = fs->block_size / inode_size;
    
    /* Calculate group descriptors per block */
    fs->desc_per_block = fs->block_size / sizeof(ext2_group_desc_t);
    
    /* Allocate buffer for group descriptors */
    uint32_t gdt_blocks = (fs->num_groups + fs->desc_per_block - 1) / fs->desc_per_block;
    uint32_t gdt_size = gdt_blocks * fs->block_size;
    fs->group_desc = (ext2_group_desc_t *)kmalloc(gdt_size);
    if (!fs->group_desc) {
        kfree(fs->superblock);
        fs->superblock = NULL;
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Read group descriptor table (starts in block after superblock) */
    uint32_t gdt_block = fs->superblock->s_first_data_block + 1;
    for (uint32_t i = 0; i < gdt_blocks; i++) {
        ret = read_block(device, gdt_block + i, 
                        (uint8_t *)fs->group_desc + (i * fs->block_size),
                        fs->block_size);
        if (ret != 0) {
            kfree(fs->group_desc);
            kfree(fs->superblock);
            fs->group_desc = NULL;
            fs->superblock = NULL;
            /* errno already set by read_block */
            return -1;
        }
    }
    
    clear_errno();
    return 0;
}

/**
 * Unmount and clean up ext2 filesystem
 */
void ext2_unmount(ext2_fs_t *fs) {
    if (!fs) {
        return;
    }
    
    if (fs->group_desc) {
        kfree(fs->group_desc);
        fs->group_desc = NULL;
    }
    
    if (fs->superblock) {
        kfree(fs->superblock);
        fs->superblock = NULL;
    }
    
    fs->device = NULL;
    fs->num_groups = 0;
    fs->block_size = 0;
}
