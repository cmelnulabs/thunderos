/*
 * ext2.h - ext2 filesystem structures and constants
 *
 * Based on the ext2 specification (Second Extended Filesystem)
 * Reference: "Design and Implementation of the Second Extended Filesystem"
 */

#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <stddef.h>

/* ext2 magic number */
#define EXT2_SUPER_MAGIC 0xEF53

/* Superblock location (always at byte offset 1024) */
#define EXT2_SUPERBLOCK_OFFSET 1024
#define EXT2_SUPERBLOCK_SIZE 1024

/* Block sizes */
#define EXT2_MIN_BLOCK_SIZE 1024
#define EXT2_MAX_BLOCK_SIZE 4096

/* Inode constants */
#define EXT2_ROOT_INO 2           /* Root directory inode */
#define EXT2_GOOD_OLD_FIRST_INO 11 /* First non-reserved inode */
#define EXT2_INODE_SIZE 128       /* Standard inode size */

/* Number of block pointers in inode */
#define EXT2_NDIR_BLOCKS 12      /* Direct blocks */
#define EXT2_IND_BLOCK 12        /* Indirect block */
#define EXT2_DIND_BLOCK 13       /* Double indirect block */
#define EXT2_TIND_BLOCK 14       /* Triple indirect block */
#define EXT2_N_BLOCKS 15         /* Total block pointers */

/* File types for inode mode */
#define EXT2_S_IFSOCK 0xC000  /* Socket */
#define EXT2_S_IFLNK  0xA000  /* Symbolic link */
#define EXT2_S_IFREG  0x8000  /* Regular file */
#define EXT2_S_IFBLK  0x6000  /* Block device */
#define EXT2_S_IFDIR  0x4000  /* Directory */
#define EXT2_S_IFCHR  0x2000  /* Character device */
#define EXT2_S_IFIFO  0x1000  /* FIFO */

/* File type mask */
#define EXT2_S_IFMT   0xF000

/* Permission bits */
#define EXT2_S_ISUID  0x0800  /* SUID */
#define EXT2_S_ISGID  0x0400  /* SGID */
#define EXT2_S_ISVTX  0x0200  /* Sticky bit */
#define EXT2_S_IRUSR  0x0100  /* User read */
#define EXT2_S_IWUSR  0x0080  /* User write */
#define EXT2_S_IXUSR  0x0040  /* User execute */
#define EXT2_S_IRGRP  0x0020  /* Group read */
#define EXT2_S_IWGRP  0x0010  /* Group write */
#define EXT2_S_IXGRP  0x0008  /* Group execute */
#define EXT2_S_IROTH  0x0004  /* Others read */
#define EXT2_S_IWOTH  0x0002  /* Others write */
#define EXT2_S_IXOTH  0x0001  /* Others execute */

/* Directory entry file types */
#define EXT2_FT_UNKNOWN  0  /* Unknown file type */
#define EXT2_FT_REG_FILE 1  /* Regular file */
#define EXT2_FT_DIR      2  /* Directory */
#define EXT2_FT_CHRDEV   3  /* Character device */
#define EXT2_FT_BLKDEV   4  /* Block device */
#define EXT2_FT_FIFO     5  /* FIFO */
#define EXT2_FT_SOCK     6  /* Socket */
#define EXT2_FT_SYMLINK  7  /* Symbolic link */

/* Filesystem states */
#define EXT2_VALID_FS 1   /* Unmounted cleanly */
#define EXT2_ERROR_FS 2   /* Errors detected */

/* Error handling methods */
#define EXT2_ERRORS_CONTINUE 1  /* Continue execution */
#define EXT2_ERRORS_RO       2  /* Remount read-only */
#define EXT2_ERRORS_PANIC    3  /* Panic */

/* Maximum filename length */
#define EXT2_NAME_LEN 255

/**
 * ext2 superblock structure
 * Located at byte offset 1024 from start of partition
 */
typedef struct {
    uint32_t s_inodes_count;        /* Total number of inodes */
    uint32_t s_blocks_count;        /* Total number of blocks */
    uint32_t s_r_blocks_count;      /* Number of reserved blocks */
    uint32_t s_free_blocks_count;   /* Number of free blocks */
    uint32_t s_free_inodes_count;   /* Number of free inodes */
    uint32_t s_first_data_block;    /* First data block (0 or 1) */
    uint32_t s_log_block_size;      /* Block size = 1024 << s_log_block_size */
    uint32_t s_log_frag_size;       /* Fragment size */
    uint32_t s_blocks_per_group;    /* Blocks per block group */
    uint32_t s_frags_per_group;     /* Fragments per group */
    uint32_t s_inodes_per_group;    /* Inodes per group */
    uint32_t s_mtime;               /* Mount time */
    uint32_t s_wtime;               /* Write time */
    uint16_t s_mnt_count;           /* Mount count */
    uint16_t s_max_mnt_count;       /* Maximum mount count */
    uint16_t s_magic;               /* Magic signature (0xEF53) */
    uint16_t s_state;               /* Filesystem state */
    uint16_t s_errors;              /* Error handling method */
    uint16_t s_minor_rev_level;     /* Minor revision level */
    uint32_t s_lastcheck;           /* Time of last check */
    uint32_t s_checkinterval;       /* Max time between checks */
    uint32_t s_creator_os;          /* OS that created filesystem */
    uint32_t s_rev_level;           /* Revision level */
    uint16_t s_def_resuid;          /* Default reserved UID */
    uint16_t s_def_resgid;          /* Default reserved GID */
    
    /* Extended fields (revision >= 1) */
    uint32_t s_first_ino;           /* First non-reserved inode */
    uint16_t s_inode_size;          /* Size of inode structure */
    uint16_t s_block_group_nr;      /* Block group number of this superblock */
    uint32_t s_feature_compat;      /* Compatible feature set */
    uint32_t s_feature_incompat;    /* Incompatible feature set */
    uint32_t s_feature_ro_compat;   /* Read-only compatible features */
    uint8_t  s_uuid[16];            /* Filesystem UUID */
    char     s_volume_name[16];     /* Volume name */
    char     s_last_mounted[64];    /* Directory last mounted on */
    uint32_t s_algorithm_usage_bitmap; /* For compression */
    
    /* Performance hints */
    uint8_t  s_prealloc_blocks;     /* Number of blocks to preallocate */
    uint8_t  s_prealloc_dir_blocks; /* Number to preallocate for dirs */
    uint16_t s_padding1;
    
    /* Journaling support (ext3) - we'll ignore these for now */
    uint8_t  s_journal_uuid[16];    /* UUID of journal superblock */
    uint32_t s_journal_inum;        /* Inode number of journal file */
    uint32_t s_journal_dev;         /* Device number of journal file */
    uint32_t s_last_orphan;         /* Head of orphan inode list */
    
    uint32_t s_reserved[197];       /* Padding to 1024 bytes */
} __attribute__((packed)) ext2_superblock_t;

/**
 * ext2 block group descriptor
 * Located in block(s) immediately after superblock
 */
typedef struct {
    uint32_t bg_block_bitmap;       /* Block number of block bitmap */
    uint32_t bg_inode_bitmap;       /* Block number of inode bitmap */
    uint32_t bg_inode_table;        /* Block number of inode table */
    uint16_t bg_free_blocks_count;  /* Number of free blocks */
    uint16_t bg_free_inodes_count;  /* Number of free inodes */
    uint16_t bg_used_dirs_count;    /* Number of directories */
    uint16_t bg_pad;                /* Padding */
    uint32_t bg_reserved[3];        /* Reserved for future use */
} __attribute__((packed)) ext2_group_desc_t;

/**
 * ext2 inode structure
 * Describes a single file or directory
 */
typedef struct {
    uint16_t i_mode;                /* File mode (type + permissions) */
    uint16_t i_uid;                 /* Owner UID */
    uint32_t i_size;                /* File size in bytes */
    uint32_t i_atime;               /* Access time */
    uint32_t i_ctime;               /* Creation time */
    uint32_t i_mtime;               /* Modification time */
    uint32_t i_dtime;               /* Deletion time */
    uint16_t i_gid;                 /* Group ID */
    uint16_t i_links_count;         /* Hard link count */
    uint32_t i_blocks;              /* Number of 512-byte blocks */
    uint32_t i_flags;               /* File flags */
    uint32_t i_osd1;                /* OS-dependent 1 */
    uint32_t i_block[EXT2_N_BLOCKS]; /* Block pointers */
    uint32_t i_generation;          /* File version (for NFS) */
    uint32_t i_file_acl;            /* File ACL */
    uint32_t i_dir_acl;             /* Directory ACL */
    uint32_t i_faddr;               /* Fragment address */
    uint8_t  i_osd2[12];            /* OS-dependent 2 */
} __attribute__((packed)) ext2_inode_t;

/**
 * ext2 directory entry
 * Variable length structure
 */
typedef struct {
    uint32_t inode;                 /* Inode number (0 if unused) */
    uint16_t rec_len;               /* Directory entry length */
    uint8_t  name_len;              /* Name length */
    uint8_t  file_type;             /* File type (if supported) */
    char     name[EXT2_NAME_LEN];   /* File name (not null-terminated) */
} __attribute__((packed)) ext2_dirent_t;

/**
 * ext2 filesystem context
 * Runtime information for mounted filesystem
 */
typedef struct {
    ext2_superblock_t *superblock;  /* Cached superblock */
    ext2_group_desc_t *group_desc;  /* Cached group descriptors */
    uint32_t block_size;            /* Actual block size in bytes */
    uint32_t num_groups;            /* Number of block groups */
    uint32_t inodes_per_block;      /* Inodes that fit in one block */
    uint32_t desc_per_block;        /* Group descriptors per block */
    void *device;                   /* Block device handle */
} ext2_fs_t;

/* Function declarations */

/**
 * Initialize and mount an ext2 filesystem
 * Returns 0 on success, -1 on error
 */
int ext2_mount(ext2_fs_t *fs, void *device);

/**
 * Unmount and clean up ext2 filesystem
 */
void ext2_unmount(ext2_fs_t *fs);

/**
 * Read an inode from disk
 * Returns 0 on success, -1 on error
 */
int ext2_read_inode(ext2_fs_t *fs, uint32_t inode_num, ext2_inode_t *inode);

/**
 * Write an inode back to disk
 * Returns 0 on success, -1 on error
 */
int ext2_write_inode(ext2_fs_t *fs, uint32_t inode_num, ext2_inode_t *inode);

/**
 * Read data from a file
 * Returns number of bytes read, or -1 on error
 */
int ext2_read_file(ext2_fs_t *fs, ext2_inode_t *inode, uint32_t offset, 
                   void *buffer, uint32_t size);

/**
 * Lookup a file in a directory by name
 * Returns inode number on success, 0 if not found
 */
uint32_t ext2_lookup(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name);

/**
 * List directory contents
 * Calls callback for each entry
 * Returns 0 on success, -1 on error
 */
typedef void (*ext2_dir_callback_t)(const char *name, uint32_t inode, uint8_t type);
int ext2_list_dir(ext2_fs_t *fs, ext2_inode_t *dir_inode, ext2_dir_callback_t callback);

/* Write operations */

/**
 * Allocate a block from block bitmap
 * Returns block number, or 0 on failure
 */
uint32_t ext2_alloc_block(ext2_fs_t *fs, uint32_t group);

/**
 * Free a block to block bitmap
 * Returns 0 on success, -1 on error
 */
int ext2_free_block(ext2_fs_t *fs, uint32_t block_num);

/**
 * Allocate an inode from inode bitmap
 * Returns inode number, or 0 on failure
 */
uint32_t ext2_alloc_inode(ext2_fs_t *fs, uint32_t group);

/**
 * Free an inode to inode bitmap
 * Returns 0 on success, -1 on error
 */
int ext2_free_inode(ext2_fs_t *fs, uint32_t inode_num);

/**
 * Write data to a file
 * Returns number of bytes written, or -1 on error
 */
int ext2_write_file(ext2_fs_t *fs, ext2_inode_t *inode, uint32_t offset,
                    const void *buffer, uint32_t size);

/**
 * Create a new file in a directory
 * Returns 0 on success, -1 on error
 */
int ext2_create_file(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name, uint32_t mode);

/**
 * Create a new directory
 * Returns 0 on success, -1 on error
 */
int ext2_create_dir(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name, uint32_t mode);

/**
 * Remove a file from a directory
 * Returns 0 on success, -1 on error
 */
int ext2_remove_file(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name);

/**
 * Remove a directory
 * Returns 0 on success, -1 on error
 */
int ext2_remove_dir(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name);

/* VFS integration */

struct vfs_filesystem;
/**
 * Mount ext2 filesystem into VFS
 * Returns VFS filesystem structure, or NULL on error
 */
struct vfs_filesystem *ext2_vfs_mount(ext2_fs_t *ext2_fs);

#endif /* EXT2_H */
