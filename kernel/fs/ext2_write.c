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
 * String length
 */
static uint32_t strlen(const char *str) {
    uint32_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/**
 * String copy
 */
static void strncpy(char *dst, const char *src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = src[i];
        if (src[i] == '\0') {
            break;
        }
    }
}

/**
 * Add a directory entry to a directory
 * Returns inode number of new entry on success, 0 on failure
 */
static int add_dir_entry(ext2_fs_t *fs, ext2_inode_t *dir_inode, uint32_t dir_inode_num,
                         const char *name, uint32_t inode_num, uint8_t file_type) {
    uint32_t name_len = strlen(name);
    if (name_len == 0 || name_len > EXT2_NAME_LEN) {
        hal_uart_puts("ext2: Invalid directory entry name length\n");
        return -1;
    }
    
    /* Calculate required entry size (aligned to 4 bytes) */
    uint32_t required_len = 8 + name_len;  /* inode(4) + rec_len(2) + name_len(1) + type(1) + name */
    required_len = (required_len + 3) & ~3;  /* Align to 4 bytes */
    
    /* Allocate buffer for directory data */
    uint32_t dir_size = dir_inode->i_size;
    uint8_t *dir_buffer = (uint8_t *)kmalloc(dir_size + required_len + 512);  /* Extra space */
    if (!dir_buffer) {
        hal_uart_puts("ext2: Failed to allocate directory buffer\n");
        return -1;
    }
    
    /* Read current directory contents */
    if (dir_size > 0) {
        int ret = ext2_read_file(fs, dir_inode, 0, dir_buffer, dir_size);
        if (ret < 0) {
            hal_uart_puts("ext2: Failed to read directory\n");
            kfree(dir_buffer);
            return -1;
        }
    }
    
    /* Find space for new entry */
    uint32_t offset = 0;
    ext2_dirent_t *new_entry = NULL;
    
    while (offset < dir_size) {
        ext2_dirent_t *entry = (ext2_dirent_t *)(dir_buffer + offset);
        
        if (entry->rec_len == 0) {
            break;
        }
        
        /* Calculate actual size used by this entry */
        uint32_t actual_len = 8 + entry->name_len;
        actual_len = (actual_len + 3) & ~3;
        
        /* Check if there's space in this entry */
        if (entry->inode != 0 && entry->rec_len >= actual_len + required_len) {
            /* Split this entry */
            uint32_t old_rec_len = entry->rec_len;
            entry->rec_len = actual_len;
            
            /* Create new entry after this one */
            new_entry = (ext2_dirent_t *)(dir_buffer + offset + actual_len);
            new_entry->inode = inode_num;
            new_entry->rec_len = old_rec_len - actual_len;
            new_entry->name_len = name_len;
            new_entry->file_type = file_type;
            strncpy(new_entry->name, name, name_len);
            break;
        }
        
        offset += entry->rec_len;
    }
    
    /* If no space found, append at end */
    if (!new_entry) {
        new_entry = (ext2_dirent_t *)(dir_buffer + dir_size);
        new_entry->inode = inode_num;
        new_entry->rec_len = fs->block_size - (dir_size % fs->block_size);
        if (new_entry->rec_len == fs->block_size && dir_size > 0) {
            new_entry->rec_len = fs->block_size;
        }
        if (dir_size == 0) {
            new_entry->rec_len = fs->block_size;
        }
        new_entry->name_len = name_len;
        new_entry->file_type = file_type;
        strncpy(new_entry->name, name, name_len);
        
        dir_size += new_entry->rec_len;
    }
    
    /* Write directory back */
    int ret = ext2_write_file(fs, dir_inode, 0, dir_buffer, dir_size);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to write directory\n");
        kfree(dir_buffer);
        return -1;
    }
    
    /* Update directory inode */
    ret = ext2_write_inode(fs, dir_inode_num, dir_inode);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to update directory inode\n");
        kfree(dir_buffer);
        return -1;
    }
    
    kfree(dir_buffer);
    return 0;
}

/**
 * Create a new file in a directory
 * Returns inode number on success, 0 on error
 */
uint32_t ext2_create_file(ext2_fs_t *fs, uint32_t dir_inode_num, const char *name, uint32_t mode) {
    if (!fs || !name || dir_inode_num == 0) {
        hal_uart_puts("ext2: Invalid parameters to ext2_create_file\n");
        return 0;
    }
    
    /* Read parent directory inode */
    ext2_inode_t dir_inode;
    int ret = ext2_read_inode(fs, dir_inode_num, &dir_inode);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to read parent directory inode\n");
        return 0;
    }
    
    /* Verify directory inode */
    if ((dir_inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
        hal_uart_puts("ext2: Parent is not a directory\n");
        return 0;
    }
    
    /* Check if file already exists */
    uint32_t existing = ext2_lookup(fs, &dir_inode, name);
    if (existing != 0) {
        hal_uart_puts("ext2: File already exists\n");
        return 0;
    }
    
    /* Allocate new inode */
    uint32_t new_inode_num = ext2_alloc_inode(fs, 0);
    if (new_inode_num == 0) {
        hal_uart_puts("ext2: Failed to allocate inode\n");
        return 0;
    }
    
    /* Initialize file inode */
    ext2_inode_t new_inode;
    for (uint32_t i = 0; i < sizeof(ext2_inode_t); i++) {
        ((uint8_t *)&new_inode)[i] = 0;
    }
    
    new_inode.i_mode = EXT2_S_IFREG | (mode & 0xFFF);
    new_inode.i_uid = 0;
    new_inode.i_size = 0;
    new_inode.i_atime = 0;
    new_inode.i_ctime = 0;
    new_inode.i_mtime = 0;
    new_inode.i_dtime = 0;
    new_inode.i_gid = 0;
    new_inode.i_links_count = 1;
    new_inode.i_blocks = 0;
    
    /* Write inode to disk */
    ret = ext2_write_inode(fs, new_inode_num, &new_inode);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to write new inode\n");
        ext2_free_inode(fs, new_inode_num);
        return 0;
    }
    
    /* Add directory entry */
    ret = add_dir_entry(fs, &dir_inode, dir_inode_num, name, new_inode_num, EXT2_FT_REG_FILE);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to add directory entry\n");
        ext2_free_inode(fs, new_inode_num);
        return 0;
    }
    
    return new_inode_num;
}

/**
 * Create a new directory
 * Returns inode number on success, 0 on error
 */
uint32_t ext2_create_dir(ext2_fs_t *fs, uint32_t dir_inode_num, const char *name, uint32_t mode) {
    if (!fs || !name || dir_inode_num == 0) {
        hal_uart_puts("ext2: Invalid parameters to ext2_create_dir\n");
        return 0;
    }
    
    /* Read parent directory inode */
    ext2_inode_t dir_inode;
    int ret = ext2_read_inode(fs, dir_inode_num, &dir_inode);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to read parent directory inode\n");
        return 0;
    }
    
    /* Verify parent is a directory */
    if ((dir_inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
        hal_uart_puts("ext2: Parent is not a directory\n");
        return 0;
    }
    
    /* Check if directory already exists */
    uint32_t existing = ext2_lookup(fs, &dir_inode, name);
    if (existing != 0) {
        hal_uart_puts("ext2: Directory already exists\n");
        return 0;
    }
    
    /* Allocate new inode for directory */
    uint32_t new_inode_num = ext2_alloc_inode(fs, 0);
    if (new_inode_num == 0) {
        hal_uart_puts("ext2: Failed to allocate inode\n");
        return 0;
    }
    
    /* Initialize directory inode */
    ext2_inode_t new_inode;
    for (uint32_t i = 0; i < sizeof(ext2_inode_t); i++) {
        ((uint8_t *)&new_inode)[i] = 0;
    }
    
    new_inode.i_mode = EXT2_S_IFDIR | (mode & 0xFFF);
    new_inode.i_uid = 0;
    new_inode.i_size = fs->block_size;  /* Initially one block */
    new_inode.i_atime = 0;
    new_inode.i_ctime = 0;
    new_inode.i_mtime = 0;
    new_inode.i_dtime = 0;
    new_inode.i_gid = 0;
    new_inode.i_links_count = 2;  /* . and parent's link */
    new_inode.i_blocks = (fs->block_size / 512);
    
    /* Allocate first block for directory */
    uint32_t dir_block = ext2_alloc_block(fs, 0);
    if (dir_block == 0) {
        hal_uart_puts("ext2: Failed to allocate block for directory\n");
        ext2_free_inode(fs, new_inode_num);
        return 0;
    }
    
    new_inode.i_block[0] = dir_block;
    
    /* Create . and .. entries */
    uint8_t *dir_data = (uint8_t *)kmalloc(fs->block_size);
    if (!dir_data) {
        hal_uart_puts("ext2: Failed to allocate directory data buffer\n");
        ext2_free_block(fs, dir_block);
        ext2_free_inode(fs, new_inode_num);
        return 0;
    }
    
    /* Zero the block */
    for (uint32_t i = 0; i < fs->block_size; i++) {
        dir_data[i] = 0;
    }
    
    /* Create "." entry */
    ext2_dirent_t *dot = (ext2_dirent_t *)dir_data;
    dot->inode = new_inode_num;
    dot->rec_len = 12;  /* 8 + 1 (name) + padding */
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';
    
    /* Create ".." entry */
    ext2_dirent_t *dotdot = (ext2_dirent_t *)(dir_data + 12);
    dotdot->inode = dir_inode_num;
    dotdot->rec_len = fs->block_size - 12;  /* Rest of block */
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    
    /* Write directory data */
    ret = ext2_write_file(fs, &new_inode, 0, dir_data, fs->block_size);
    kfree(dir_data);
    
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to write directory data\n");
        ext2_free_block(fs, dir_block);
        ext2_free_inode(fs, new_inode_num);
        return 0;
    }
    
    /* Write new directory inode */
    ret = ext2_write_inode(fs, new_inode_num, &new_inode);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to write new directory inode\n");
        ext2_free_block(fs, dir_block);
        ext2_free_inode(fs, new_inode_num);
        return 0;
    }
    
    /* Add entry in parent directory */
    ret = add_dir_entry(fs, &dir_inode, dir_inode_num, name, new_inode_num, EXT2_FT_DIR);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to add directory entry in parent\n");
        ext2_free_block(fs, dir_block);
        ext2_free_inode(fs, new_inode_num);
        return 0;
    }
    
    /* Update parent directory link count */
    dir_inode.i_links_count++;
    ret = ext2_write_inode(fs, dir_inode_num, &dir_inode);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to update parent directory inode\n");
    }
    
    return new_inode_num;
}

/**
 * Remove a file from a directory
 */
int ext2_remove_file(ext2_fs_t *fs, uint32_t dir_inode_num, const char *name) {
    (void)fs;
    (void)dir_inode_num;
    (void)name;
    
    hal_uart_puts("ext2: remove_file not yet implemented\n");
    return -1;
}

/**
 * Remove a directory
 */
int ext2_remove_dir(ext2_fs_t *fs, uint32_t dir_inode_num, const char *name) {
    (void)fs;
    (void)dir_inode_num;
    (void)name;
    
    hal_uart_puts("ext2: remove_dir not yet implemented\n");
    return -1;
}
