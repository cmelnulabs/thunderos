/*
 * ext2_dir.c - ext2 directory operations
 */

#include "../include/fs/ext2.h"
#include "../include/hal/hal_uart.h"
#include "../include/mm/kmalloc.h"
#include "../include/kernel/errno.h"
#include <stddef.h>

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
 * String comparison (up to n characters)
 */
static int strncmp(const char *s1, const char *s2, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

/**
 * Lookup a file in a directory by name
 */
uint32_t ext2_lookup(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name) {
    if (!fs || !dir_inode || !name) {
        hal_uart_puts("ext2: Invalid parameters to ext2_lookup\n");
        set_errno(THUNDEROS_EINVAL);
        return 0;
    }
    
    /* Verify this is a directory */
    if ((dir_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
        hal_uart_puts("ext2: Inode is not a directory\n");
        set_errno(THUNDEROS_EFS_BADDIR);
        return 0;
    }
    
    uint32_t name_len = strlen(name);
    if (name_len == 0 || name_len > EXT2_NAME_LEN) {
        hal_uart_puts("ext2: Invalid filename length\n");
        set_errno(THUNDEROS_EINVAL);
        return 0;
    }
    
    /* Allocate buffer for directory data */
    uint8_t *dir_buffer = (uint8_t *)kmalloc(dir_inode->i_size);
    if (!dir_buffer) {
        hal_uart_puts("ext2: Failed to allocate directory buffer\n");
        set_errno(THUNDEROS_ENOMEM);
        return 0;
    }
    
    /* Read directory contents */
    int ret = ext2_read_file(fs, dir_inode, 0, dir_buffer, dir_inode->i_size);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to read directory\n");
        kfree(dir_buffer);
        /* errno already set by ext2_read_file */
        return 0;
    }
    
    /* Parse directory entries */
    uint32_t offset = 0;
    while (offset < dir_inode->i_size) {
        ext2_dirent_t *entry = (ext2_dirent_t *)(dir_buffer + offset);
        
        /* Check for invalid entry */
        if (entry->rec_len == 0) {
            break;
        }
        
        /* Check if this entry matches the name we're looking for */
        if (entry->inode != 0 && entry->name_len == name_len) {
            if (strncmp(entry->name, name, name_len) == 0) {
                uint32_t inode_num = entry->inode;
                kfree(dir_buffer);
                clear_errno();
                return inode_num;
            }
        }
        
        offset += entry->rec_len;
    }
    
    kfree(dir_buffer);
    set_errno(THUNDEROS_ENOENT);
    return 0;  /* Not found */
}

/**
 * List directory contents
 */
int ext2_list_dir(ext2_fs_t *fs, ext2_inode_t *dir_inode, ext2_dir_callback_t callback) {
    if (!fs || !dir_inode || !callback) {
        hal_uart_puts("ext2: Invalid parameters to ext2_list_dir\n");
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Verify this is a directory */
    if ((dir_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
        hal_uart_puts("ext2: Inode is not a directory\n");
        RETURN_ERRNO(THUNDEROS_EFS_BADDIR);
    }
    
    /* Allocate buffer for directory data */
    uint8_t *dir_buffer = (uint8_t *)kmalloc(dir_inode->i_size);
    if (!dir_buffer) {
        hal_uart_puts("ext2: Failed to allocate directory buffer\n");
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Read directory contents */
    int ret = ext2_read_file(fs, dir_inode, 0, dir_buffer, dir_inode->i_size);
    if (ret < 0) {
        hal_uart_puts("ext2: Failed to read directory\n");
        kfree(dir_buffer);
        /* errno already set by ext2_read_file */
        return -1;
    }
    
    /* Allocate buffer for null-terminated name */
    char *name_buffer = (char *)kmalloc(EXT2_NAME_LEN + 1);
    if (!name_buffer) {
        hal_uart_puts("ext2: Failed to allocate name buffer\n");
        kfree(dir_buffer);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Parse directory entries */
    uint32_t offset = 0;
    while (offset < dir_inode->i_size) {
        ext2_dirent_t *entry = (ext2_dirent_t *)(dir_buffer + offset);
        
        /* Check for invalid entry */
        if (entry->rec_len == 0) {
            break;
        }
        
        /* Process valid entries */
        if (entry->inode != 0) {
            /* Copy name and null-terminate */
            for (uint32_t i = 0; i < entry->name_len; i++) {
                name_buffer[i] = entry->name[i];
            }
            name_buffer[entry->name_len] = '\0';
            
            /* Call callback with entry information */
            callback(name_buffer, entry->inode, entry->file_type);
        }
        
        offset += entry->rec_len;
    }
    
    kfree(name_buffer);
    kfree(dir_buffer);
    clear_errno();
    return 0;
}
