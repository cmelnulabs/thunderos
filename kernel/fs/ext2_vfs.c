/*
 * ext2_vfs.c - ext2 filesystem VFS integration
 */

#include "../../include/fs/ext2.h"
#include "../../include/fs/vfs.h"
#include "../../include/mm/kmalloc.h"
#include "../../include/hal/hal_uart.h"
#include <stddef.h>

/* Forward declarations for ext2 VFS operations */
static int ext2_vfs_read(vfs_node_t *node, uint32_t offset, void *buffer, uint32_t size);
static int ext2_vfs_write(vfs_node_t *node, uint32_t offset, const void *buffer, uint32_t size);
static vfs_node_t *ext2_vfs_lookup(vfs_node_t *dir, const char *name);
static int ext2_vfs_readdir(vfs_node_t *dir, uint32_t index, char *name, uint32_t *inode);
static int ext2_vfs_create(vfs_node_t *dir, const char *name, uint32_t mode);
static int ext2_vfs_mkdir(vfs_node_t *dir, const char *name, uint32_t mode);
static int ext2_vfs_unlink(vfs_node_t *dir, const char *name);
static int ext2_vfs_rmdir(vfs_node_t *dir, const char *name);

/* ext2 VFS operations table */
static vfs_ops_t ext2_vfs_ops = {
    .read = ext2_vfs_read,
    .write = ext2_vfs_write,
    .open = NULL,   /* No special open handling needed */
    .close = NULL,  /* No special close handling needed */
    .lookup = ext2_vfs_lookup,
    .readdir = ext2_vfs_readdir,
    .create = ext2_vfs_create,
    .mkdir = ext2_vfs_mkdir,
    .unlink = ext2_vfs_unlink,
    .rmdir = ext2_vfs_rmdir,
};

/**
 * String copy
 */
static void strcpy_safe(char *dst, const char *src, uint32_t max_len) {
    uint32_t i = 0;
    while (i < max_len - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/**
 * Read from ext2 file via VFS
 */
static int ext2_vfs_read(vfs_node_t *node, uint32_t offset, void *buffer, uint32_t size) {
    if (!node || !node->fs || !node->fs->fs_data || !node->fs_data) {
        return -1;
    }
    
    ext2_fs_t *ext2_fs = (ext2_fs_t *)node->fs->fs_data;
    ext2_inode_t *inode = (ext2_inode_t *)node->fs_data;
    
    return ext2_read_file(ext2_fs, inode, offset, buffer, size);
}

/**
 * Write to ext2 file via VFS
 */
static int ext2_vfs_write(vfs_node_t *node, uint32_t offset, const void *buffer, uint32_t size) {
    if (!node || !node->fs || !node->fs->fs_data || !node->fs_data) {
        return -1;
    }
    
    ext2_fs_t *ext2_fs = (ext2_fs_t *)node->fs->fs_data;
    ext2_inode_t *inode = (ext2_inode_t *)node->fs_data;
    
    return ext2_write_file(ext2_fs, inode, offset, buffer, size);
}

/**
 * Lookup file in ext2 directory via VFS
 */
static vfs_node_t *ext2_vfs_lookup(vfs_node_t *dir, const char *name) {
    if (!dir || !dir->fs || !dir->fs->fs_data || !dir->fs_data) {
        return NULL;
    }
    
    ext2_fs_t *ext2_fs = (ext2_fs_t *)dir->fs->fs_data;
    ext2_inode_t *dir_inode = (ext2_inode_t *)dir->fs_data;
    
    /* Lookup inode number */
    uint32_t inode_num = ext2_lookup(ext2_fs, dir_inode, name);
    if (inode_num == 0) {
        return NULL;
    }
    
    /* Allocate and read inode */
    ext2_inode_t *inode = (ext2_inode_t *)kmalloc(sizeof(ext2_inode_t));
    if (!inode) {
        return NULL;
    }
    
    if (ext2_read_inode(ext2_fs, inode_num, inode) != 0) {
        kfree(inode);
        return NULL;
    }
    
    /* Create VFS node */
    vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!node) {
        kfree(inode);
        return NULL;
    }
    
    strcpy_safe(node->name, name, sizeof(node->name));
    node->inode = inode_num;
    node->size = inode->i_size;
    node->type = ((inode->i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR) ? 
                 VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
    node->flags = 0;
    node->fs = dir->fs;
    node->fs_data = inode;
    node->ops = &ext2_vfs_ops;
    
    return node;
}

/**
 * Read directory entry by index via VFS
 */
static int ext2_vfs_readdir(vfs_node_t *dir, uint32_t index, char *name, uint32_t *inode) {
    if (!dir || !dir->fs || !dir->fs->fs_data || !dir->fs_data || !name || !inode) {
        return -1;
    }
    
    ext2_fs_t *ext2_fs = (ext2_fs_t *)dir->fs->fs_data;
    ext2_inode_t *dir_inode = (ext2_inode_t *)dir->fs_data;
    
    /* Verify this is a directory */
    if ((dir_inode->i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
        return -1;
    }
    
    /* Allocate buffer for directory data */
    uint8_t *dir_buffer = (uint8_t *)kmalloc(dir_inode->i_size);
    if (!dir_buffer) {
        return -1;
    }
    
    /* Read directory contents */
    int ret = ext2_read_file(ext2_fs, dir_inode, 0, dir_buffer, dir_inode->i_size);
    if (ret < 0) {
        kfree(dir_buffer);
        return -1;
    }
    
    /* Find the entry at the requested index */
    uint32_t offset = 0;
    uint32_t current_index = 0;
    int found = 0;
    
    while (offset < dir_inode->i_size) {
        ext2_dirent_t *entry = (ext2_dirent_t *)(dir_buffer + offset);
        
        /* Check for invalid entry */
        if (entry->rec_len == 0) {
            break;
        }
        
        /* Count only valid entries */
        if (entry->inode != 0) {
            if (current_index == index) {
                /* Found the requested entry */
                /* Copy name and null-terminate */
                for (uint32_t i = 0; i < entry->name_len && i < 255; i++) {
                    name[i] = entry->name[i];
                }
                name[entry->name_len] = '\0';
                *inode = entry->inode;
                found = 1;
                break;
            }
            current_index++;
        }
        
        offset += entry->rec_len;
    }
    
    kfree(dir_buffer);
    return found ? 0 : -1;
}

/**
 * Create file in ext2 directory via VFS
 */
static int ext2_vfs_create(vfs_node_t *dir, const char *name, uint32_t mode) {
    if (!dir || !dir->fs || !dir->fs->fs_data) {
        return -1;
    }
    
    ext2_fs_t *ext2_fs = (ext2_fs_t *)dir->fs->fs_data;
    uint32_t dir_inode_num = dir->inode;
    
    uint32_t new_inode = ext2_create_file(ext2_fs, dir_inode_num, name, mode);
    return (new_inode == 0) ? -1 : 0;
}

/**
 * Create directory in ext2 via VFS
 */
static int ext2_vfs_mkdir(vfs_node_t *dir, const char *name, uint32_t mode) {
    if (!dir || !dir->fs || !dir->fs->fs_data) {
        return -1;
    }
    
    ext2_fs_t *ext2_fs = (ext2_fs_t *)dir->fs->fs_data;
    uint32_t dir_inode_num = dir->inode;
    
    uint32_t new_inode = ext2_create_dir(ext2_fs, dir_inode_num, name, mode);
    return (new_inode == 0) ? -1 : 0;
}

/**
 * Remove file from ext2 directory via VFS
 */
static int ext2_vfs_unlink(vfs_node_t *dir, const char *name) {
    if (!dir || !dir->fs || !dir->fs->fs_data) {
        return -1;
    }
    
    ext2_fs_t *ext2_fs = (ext2_fs_t *)dir->fs->fs_data;
    uint32_t dir_inode_num = dir->inode;
    
    return ext2_remove_file(ext2_fs, dir_inode_num, name);
}

/**
 * Remove directory from ext2 via VFS
 */
static int ext2_vfs_rmdir(vfs_node_t *dir, const char *name) {
    if (!dir || !dir->fs || !dir->fs->fs_data) {
        return -1;
    }
    
    ext2_fs_t *ext2_fs = (ext2_fs_t *)dir->fs->fs_data;
    uint32_t dir_inode_num = dir->inode;
    
    return ext2_remove_dir(ext2_fs, dir_inode_num, name);
}

/**
 * Mount ext2 filesystem into VFS
 */
vfs_filesystem_t *ext2_vfs_mount(ext2_fs_t *ext2_fs) {
    if (!ext2_fs) {
        return NULL;
    }
    
    /* Allocate VFS filesystem structure */
    vfs_filesystem_t *vfs_fs = (vfs_filesystem_t *)kmalloc(sizeof(vfs_filesystem_t));
    if (!vfs_fs) {
        return NULL;
    }
    
    /* Read root inode */
    ext2_inode_t *root_inode = (ext2_inode_t *)kmalloc(sizeof(ext2_inode_t));
    if (!root_inode) {
        kfree(vfs_fs);
        return NULL;
    }
    
    if (ext2_read_inode(ext2_fs, EXT2_ROOT_INO, root_inode) != 0) {
        kfree(root_inode);
        kfree(vfs_fs);
        return NULL;
    }
    
    /* Create root VFS node */
    vfs_node_t *root_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!root_node) {
        kfree(root_inode);
        kfree(vfs_fs);
        return NULL;
    }
    
    strcpy_safe(root_node->name, "/", sizeof(root_node->name));
    root_node->inode = EXT2_ROOT_INO;
    root_node->size = root_inode->i_size;
    root_node->type = VFS_TYPE_DIRECTORY;
    root_node->flags = 0;
    root_node->fs = vfs_fs;
    root_node->fs_data = root_inode;
    root_node->ops = &ext2_vfs_ops;
    
    /* Initialize filesystem structure */
    strcpy_safe(vfs_fs->name, "ext2", sizeof(vfs_fs->name));
    vfs_fs->fs_data = ext2_fs;
    vfs_fs->root = root_node;
    vfs_fs->ops = &ext2_vfs_ops;
    
    return vfs_fs;
}
