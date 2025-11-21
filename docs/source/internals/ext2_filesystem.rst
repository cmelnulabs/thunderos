ext2 Filesystem Implementation
==============================

Overview
--------

ThunderOS implements the ext2 (Second Extended Filesystem) for persistent storage. ext2 is a mature, well-documented filesystem that provides a good balance between simplicity and features. It's widely used in Linux and has excellent tooling support (``mkfs.ext2``, ``debugfs``, ``e2fsck``).

The implementation consists of two main components:

- **Core ext2 driver** (``kernel/fs/ext2.c``): Handles on-disk structures, block I/O, and inode management
- **VFS integration** (``kernel/fs/ext2_vfs.c``): Adapts ext2 to ThunderOS's Virtual Filesystem layer

Architecture
------------

On-Disk Layout
~~~~~~~~~~~~~~

An ext2 filesystem is divided into block groups. ThunderOS currently uses a simplified single-block-group layout:

.. code-block:: text

    ┌────────────────────────────────────────────────────┐
    │ Block 0: Boot Block (1024 bytes, unused)          │
    ├────────────────────────────────────────────────────┤
    │ Block 1: Superblock                               │
    │   - Magic number (0xEF53)                         │
    │   - Block size (1024, 2048, or 4096)              │
    │   - Total blocks, inodes                          │
    │   - Free blocks, free inodes                      │
    │   - Blocks per group, inodes per group            │
    │   - First data block                              │
    ├────────────────────────────────────────────────────┤
    │ Block 2+: Block Group Descriptor Table            │
    │   - Block bitmap location                         │
    │   - Inode bitmap location                         │
    │   - Inode table location                          │
    │   - Free blocks/inodes count                      │
    ├────────────────────────────────────────────────────┤
    │ Block Bitmap (1 block)                            │
    │   - 1 bit per block (1=used, 0=free)              │
    ├────────────────────────────────────────────────────┤
    │ Inode Bitmap (1 block)                            │
    │   - 1 bit per inode (1=used, 0=free)              │
    ├────────────────────────────────────────────────────┤
    │ Inode Table                                       │
    │   - Array of 128-byte inode structures            │
    │   - Inodes 1-10 reserved (2=root directory)       │
    ├────────────────────────────────────────────────────┤
    │ Data Blocks                                       │
    │   - File contents and directory entries           │
    └────────────────────────────────────────────────────┘

Key Data Structures
-------------------

Superblock
~~~~~~~~~~

The superblock contains global filesystem metadata:

.. code-block:: c

    struct ext2_superblock {
        uint32_t s_inodes_count;        // Total inodes
        uint32_t s_blocks_count;        // Total blocks
        uint32_t s_r_blocks_count;      // Reserved blocks
        uint32_t s_free_blocks_count;   // Free blocks
        uint32_t s_free_inodes_count;   // Free inodes
        uint32_t s_first_data_block;    // First data block (0 or 1)
        uint32_t s_log_block_size;      // Block size = 1024 << s_log_block_size
        uint32_t s_log_frag_size;       // Fragment size
        uint32_t s_blocks_per_group;    // Blocks per block group
        uint32_t s_frags_per_group;     // Fragments per group
        uint32_t s_inodes_per_group;    // Inodes per group
        uint32_t s_mtime;               // Last mount time
        uint32_t s_wtime;               // Last write time
        uint16_t s_mnt_count;           // Mount count
        uint16_t s_max_mnt_count;       // Max mount count before fsck
        uint16_t s_magic;               // Magic signature (0xEF53)
        uint16_t s_state;               // Filesystem state
        uint16_t s_errors;              // Error handling behavior
        uint16_t s_minor_rev_level;     // Minor revision level
        uint32_t s_lastcheck;           // Last check time
        uint32_t s_checkinterval;       // Check interval
        uint32_t s_creator_os;          // OS that created filesystem
        uint32_t s_rev_level;           // Revision level
        uint16_t s_def_resuid;          // Default UID for reserved blocks
        uint16_t s_def_resgid;          // Default GID for reserved blocks
        // Extended fields (rev 1+)
        uint32_t s_first_ino;           // First non-reserved inode
        uint16_t s_inode_size;          // Inode structure size
        // ... additional fields
    };

**Key Fields:**

- ``s_magic``: Must be ``0xEF53`` for valid ext2
- ``s_log_block_size``: Block size = ``1024 << s_log_block_size`` (typically 4096 bytes)
- ``s_first_data_block``: 0 for 2KB+ blocks, 1 for 1KB blocks
- ``s_first_ino``: First usable inode (typically 11)
- ``s_inode_size``: Size of inode structure (typically 128 or 256 bytes)

Error Handling
~~~~~~~~~~~~~~

The ext2 implementation uses ThunderOS's errno system for error reporting. All ext2 functions that can fail set ``errno`` with specific error codes before returning.

**ext2-Specific Error Codes:**

.. code-block:: c

    #define THUNDEROS_EFS_CORRUPT   30  /* Filesystem corruption detected */
    #define THUNDEROS_EFS_INVAL     31  /* Invalid filesystem structure */
    #define THUNDEROS_EFS_BADBLK    32  /* Invalid block number */
    #define THUNDEROS_EFS_NOINODE   33  /* No free inodes available */
    #define THUNDEROS_EFS_NOBLK     34  /* No free blocks available */
    #define THUNDEROS_EFS_BADINO    35  /* Invalid inode number */
    #define THUNDEROS_EFS_NOTMNT    41  /* Filesystem not mounted */

**Common Error Conditions:**

1. **Invalid inode number** (``THUNDEROS_EFS_BADINO``):
   - Inode number must be >= 1 and <= ``s_inodes_count``
   - Inode 0 is reserved and never valid
   - Inodes 1-10 are system-reserved

2. **Invalid block number** (``THUNDEROS_EFS_BADBLK``):
   - Block number must be >= ``s_first_data_block``
   - Block number must be < ``s_blocks_count``
   - Out-of-range blocks indicate filesystem corruption

3. **Filesystem corruption** (``THUNDEROS_EFS_CORRUPT``):
   - Magic number mismatch (not 0xEF53)
   - Invalid superblock parameters
   - Inconsistent block group descriptors

**Example Error Handling:**

.. code-block:: c

    // Reading an inode with validation
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) < 0) {
        int err = get_errno();
        if (err == THUNDEROS_EFS_BADINO) {
            kprintf("Invalid inode number: %u\n", inode_num);
        } else if (err == THUNDEROS_EFS_CORRUPT) {
            kprintf("Filesystem corruption detected\n");
        }
        return -1;  // errno already set
    }
    
    // Block allocation
    uint32_t block = ext2_alloc_block(fs, 0);
    if (block == 0) {
        int err = get_errno();
        if (err == THUNDEROS_EFS_NOBLK) {
            kprintf("Disk full: no free blocks\n");
        }
        return -1;
    }

See ``docs/source/internals/errno.rst`` for complete errno documentation and best practices.

Inode
~~~~~

Inodes store file metadata and block pointers:

.. code-block:: c

    struct ext2_inode {
        uint16_t i_mode;        // File mode (permissions + type)
        uint16_t i_uid;         // Owner UID
        uint32_t i_size;        // File size (low 32 bits)
        uint32_t i_atime;       // Access time
        uint32_t i_ctime;       // Creation time
        uint32_t i_mtime;       // Modification time
        uint32_t i_dtime;       // Deletion time
        uint16_t i_gid;         // Group GID
        uint16_t i_links_count; // Hard link count
        uint32_t i_blocks;      // 512-byte block count
        uint32_t i_flags;       // File flags
        uint32_t i_osd1;        // OS-dependent
        uint32_t i_block[15];   // Block pointers (see below)
        uint32_t i_generation;  // File version (for NFS)
        uint32_t i_file_acl;    // Extended attribute block
        uint32_t i_dir_acl;     // Directory ACL / size high
        uint32_t i_faddr;       // Fragment address
        uint8_t  i_osd2[12];    // OS-dependent
    };

**File Mode Bits:**

.. code-block:: c

    #define EXT2_S_IFSOCK   0xC000  // Socket
    #define EXT2_S_IFLNK    0xA000  // Symbolic link
    #define EXT2_S_IFREG    0x8000  // Regular file
    #define EXT2_S_IFBLK    0x6000  // Block device
    #define EXT2_S_IFDIR    0x4000  // Directory
    #define EXT2_S_IFCHR    0x2000  // Character device
    #define EXT2_S_IFIFO    0x1000  // FIFO
    
    #define EXT2_S_ISUID    0x0800  // Set UID
    #define EXT2_S_ISGID    0x0400  // Set GID
    #define EXT2_S_ISVTX    0x0200  // Sticky bit
    
    #define EXT2_S_IRUSR    0x0100  // Owner read
    #define EXT2_S_IWUSR    0x0080  // Owner write
    #define EXT2_S_IXUSR    0x0040  // Owner execute
    #define EXT2_S_IRGRP    0x0020  // Group read
    #define EXT2_S_IWGRP    0x0010  // Group write
    #define EXT2_S_IXGRP    0x0008  // Group execute
    #define EXT2_S_IROTH    0x0004  // Other read
    #define EXT2_S_IWOTH    0x0002  // Other write
    #define EXT2_S_IXOTH    0x0001  // Other execute

Block Addressing
~~~~~~~~~~~~~~~~

The ``i_block[15]`` array provides multi-level indexing for large files:

.. code-block:: text

    i_block[0..11]   → Direct blocks (12 blocks)
    i_block[12]      → Single indirect block
                        └→ Block of block numbers (1 level)
    i_block[13]      → Double indirect block
                        └→ Block of indirect blocks (2 levels)
    i_block[14]      → Triple indirect block
                        └→ Block of double indirect blocks (3 levels)

**Maximum File Size** (4KB blocks):

- Direct: 12 × 4KB = 48 KB
- Single indirect: 1024 × 4KB = 4 MB
- Double indirect: 1024 × 1024 × 4KB = 4 GB
- Triple indirect: 1024 × 1024 × 1024 × 4KB = 4 TB

ThunderOS currently implements direct and single indirect blocks.

Directory Entry
~~~~~~~~~~~~~~~

Directories are files containing a linked list of directory entries:

.. code-block:: c

    struct ext2_dir_entry {
        uint32_t inode;         // Inode number (0 = unused entry)
        uint16_t rec_len;       // Directory entry length
        uint8_t  name_len;      // Name length (actual)
        uint8_t  file_type;     // File type (0=unknown, 1=reg, 2=dir, ...)
        char     name[255];     // File name (variable length)
    };

**File Types:**

.. code-block:: c

    #define EXT2_FT_UNKNOWN     0   // Unknown
    #define EXT2_FT_REG_FILE    1   // Regular file
    #define EXT2_FT_DIR         2   // Directory
    #define EXT2_FT_CHRDEV      3   // Character device
    #define EXT2_FT_BLKDEV      4   // Block device
    #define EXT2_FT_FIFO        5   // FIFO
    #define EXT2_FT_SOCK        6   // Socket
    #define EXT2_FT_SYMLINK     7   // Symbolic link

**Entry Layout:**

.. code-block:: text

    ┌─────────────────────────────────────────┐
    │ Entry 1: inode=15, rec_len=24, "hello"  │
    ├─────────────────────────────────────────┤
    │ Entry 2: inode=23, rec_len=20, "test"   │
    ├─────────────────────────────────────────┤
    │ Entry 3: inode=0, rec_len=remaining     │  (Unused)
    └─────────────────────────────────────────┘

Entries are 4-byte aligned. The ``rec_len`` field allows skipping to the next entry.

Core Operations
---------------

Mounting
~~~~~~~~

The mount operation validates the superblock and reads filesystem metadata:

.. code-block:: c

    struct ext2_fs* ext2_mount(struct block_device *dev) {
        struct ext2_fs *fs = kmalloc(sizeof(struct ext2_fs));
        fs->block_dev = dev;
        
        // 1. Read superblock from block 1 (offset 1024)
        char buffer[1024];
        if (dev->read(dev, 2, buffer, 2) != 0) {  // 2 sectors @ offset 1024
            return NULL;
        }
        
        struct ext2_superblock *sb = (struct ext2_superblock*)buffer;
        
        // 2. Validate magic number
        if (sb->s_magic != EXT2_SUPER_MAGIC) {
            kprintf("Invalid ext2 magic: 0x%x\n", sb->s_magic);
            return NULL;
        }
        
        // 3. Calculate block size
        fs->block_size = 1024 << sb->s_log_block_size;  // Typically 4096
        
        // 4. Store critical metadata
        fs->total_blocks = sb->s_blocks_count;
        fs->total_inodes = sb->s_inodes_count;
        fs->blocks_per_group = sb->s_blocks_per_group;
        fs->inodes_per_group = sb->s_inodes_per_group;
        fs->first_data_block = sb->s_first_data_block;
        fs->inode_size = (sb->s_rev_level == 0) ? 128 : sb->s_inode_size;
        
        // 5. Read block group descriptor table
        uint32_t bgdt_block = fs->first_data_block + 1;
        fs->group_desc = kmalloc(fs->block_size);
        read_block(fs, bgdt_block, fs->group_desc);
        
        // 6. Cache inode table location
        fs->inode_table_block = fs->group_desc[0].bg_inode_table;
        
        return fs;
    }

Reading an Inode
~~~~~~~~~~~~~~~~

.. code-block:: c

    int ext2_read_inode(struct ext2_fs *fs, uint32_t inode_num,
                        struct ext2_inode *inode) {
        // 1. Calculate which block group contains this inode
        uint32_t group = (inode_num - 1) / fs->inodes_per_group;
        uint32_t index = (inode_num - 1) % fs->inodes_per_group;
        
        // 2. Calculate byte offset within inode table
        uint32_t inode_table_block = fs->group_desc[group].bg_inode_table;
        uint32_t byte_offset = index * fs->inode_size;
        uint32_t block_offset = byte_offset / fs->block_size;
        uint32_t offset_in_block = byte_offset % fs->block_size;
        
        // 3. Read the block containing the inode
        char *buffer = kmalloc(fs->block_size);
        if (read_block(fs, inode_table_block + block_offset, buffer) != 0) {
            kfree(buffer);
            return -1;
        }
        
        // 4. Copy inode structure
        memcpy(inode, buffer + offset_in_block, sizeof(struct ext2_inode));
        
        kfree(buffer);
        return 0;
    }

Reading File Data
~~~~~~~~~~~~~~~~~

.. code-block:: c

    ssize_t ext2_read_file(struct ext2_fs *fs, struct ext2_inode *inode,
                           uint64_t offset, void *buffer, size_t size) {
        // 1. Clamp size to file bounds
        if (offset >= inode->i_size) {
            return 0;  // EOF
        }
        if (offset + size > inode->i_size) {
            size = inode->i_size - offset;
        }
        
        // 2. Calculate starting block and offset within block
        uint32_t block_index = offset / fs->block_size;
        uint32_t block_offset = offset % fs->block_size;
        size_t bytes_read = 0;
        
        // 3. Allocate block buffer
        char *block_buffer = kmalloc(fs->block_size);
        
        while (bytes_read < size) {
            // 4. Get physical block number
            uint32_t phys_block = ext2_get_block(fs, inode, block_index);
            if (phys_block == 0) {
                break;  // Sparse file or error
            }
            
            // 5. Read block from disk
            if (read_block(fs, phys_block, block_buffer) != 0) {
                break;
            }
            
            // 6. Copy data to user buffer
            size_t bytes_in_block = fs->block_size - block_offset;
            if (bytes_read + bytes_in_block > size) {
                bytes_in_block = size - bytes_read;
            }
            
            memcpy((char*)buffer + bytes_read,
                   block_buffer + block_offset,
                   bytes_in_block);
            
            bytes_read += bytes_in_block;
            block_index++;
            block_offset = 0;  // Subsequent blocks start at offset 0
        }
        
        kfree(block_buffer);
        return bytes_read;
    }

Block Number Resolution
~~~~~~~~~~~~~~~~~~~~~~~

Translates file block index to physical block number:

.. code-block:: c

    uint32_t ext2_get_block(struct ext2_fs *fs, struct ext2_inode *inode,
                            uint32_t block_index) {
        // 1. Direct blocks (0-11)
        if (block_index < 12) {
            return inode->i_block[block_index];
        }
        
        // 2. Single indirect blocks (12 - 12+1023)
        uint32_t ptrs_per_block = fs->block_size / 4;  // 1024 for 4KB blocks
        
        if (block_index < 12 + ptrs_per_block) {
            // Read indirect block
            uint32_t indirect_block = inode->i_block[12];
            if (indirect_block == 0) {
                return 0;  // Not allocated
            }
            
            uint32_t *indirect_data = kmalloc(fs->block_size);
            read_block(fs, indirect_block, indirect_data);
            
            uint32_t index = block_index - 12;
            uint32_t phys_block = indirect_data[index];
            
            kfree(indirect_data);
            return phys_block;
        }
        
        // 3. Double indirect (not yet implemented)
        return 0;
    }

Directory Operations
~~~~~~~~~~~~~~~~~~~~

Listing directory contents:

.. code-block:: c

    int ext2_readdir(struct ext2_fs *fs, struct ext2_inode *dir_inode,
                     void (*callback)(const char *name, uint32_t inode)) {
        if ((dir_inode->i_mode & 0xF000) != EXT2_S_IFDIR) {
            return -1;  // Not a directory
        }
        
        // 1. Allocate buffer for directory data
        char *dir_data = kmalloc(dir_inode->i_size);
        ext2_read_file(fs, dir_inode, 0, dir_data, dir_inode->i_size);
        
        // 2. Iterate through directory entries
        uint32_t offset = 0;
        while (offset < dir_inode->i_size) {
            struct ext2_dir_entry *entry =
                (struct ext2_dir_entry*)(dir_data + offset);
            
            // 3. Skip unused entries
            if (entry->inode != 0) {
                // 4. Null-terminate name (it's not stored null-terminated)
                char name[256];
                memcpy(name, entry->name, entry->name_len);
                name[entry->name_len] = '\0';
                
                // 5. Invoke callback
                callback(name, entry->inode);
            }
            
            // 6. Move to next entry
            offset += entry->rec_len;
            
            // 7. Safety check for corrupted directory
            if (entry->rec_len == 0) {
                break;
            }
        }
        
        kfree(dir_data);
        return 0;
    }

Path Resolution
~~~~~~~~~~~~~~~

Convert a path like ``/bin/hello`` to an inode number:

.. code-block:: c

    uint32_t ext2_path_to_inode(struct ext2_fs *fs, const char *path) {
        // 1. Start at root inode (always inode 2)
        uint32_t current_inode = 2;
        
        // 2. Handle root directory
        if (strcmp(path, "/") == 0) {
            return 2;
        }
        
        // 3. Skip leading slash
        if (path[0] == '/') {
            path++;
        }
        
        // 4. Traverse path components
        char component[256];
        while (*path != '\0') {
            // 5. Extract next path component
            const char *slash = strchr(path, '/');
            size_t len;
            if (slash != NULL) {
                len = slash - path;
            } else {
                len = strlen(path);
            }
            
            memcpy(component, path, len);
            component[len] = '\0';
            
            // 6. Look up component in current directory
            struct ext2_inode inode;
            ext2_read_inode(fs, current_inode, &inode);
            
            current_inode = ext2_lookup_entry(fs, &inode, component);
            if (current_inode == 0) {
                return 0;  // Not found
            }
            
            // 7. Advance to next component
            path += len;
            if (*path == '/') {
                path++;
            }
        }
        
        return current_inode;
    }

VFS Integration
---------------

ThunderOS uses a Virtual Filesystem (VFS) layer to abstract different filesystems. ext2 registers itself with VFS:

.. code-block:: c

    static struct vfs_operations ext2_vfs_ops = {
        .open = ext2_vfs_open,
        .close = ext2_vfs_close,
        .read = ext2_vfs_read,
        .write = ext2_vfs_write,
        .readdir = ext2_vfs_readdir,
        .stat = ext2_vfs_stat,
    };
    
    void ext2_register_with_vfs(struct ext2_fs *fs) {
        vfs_mount("/", &ext2_vfs_ops, fs);
    }

**VFS Operations:**

.. code-block:: c

    int ext2_vfs_open(void *fs_data, const char *path, int flags) {
        struct ext2_fs *fs = (struct ext2_fs*)fs_data;
        
        // 1. Resolve path to inode
        uint32_t inode_num = ext2_path_to_inode(fs, path);
        if (inode_num == 0) {
            return -1;  // File not found
        }
        
        // 2. Allocate file descriptor
        int fd = vfs_alloc_fd();
        struct vfs_file *file = &vfs_file_table[fd];
        
        // 3. Read inode
        struct ext2_inode *inode = kmalloc(sizeof(struct ext2_inode));
        ext2_read_inode(fs, inode_num, inode);
        
        // 4. Store file state
        file->fs_data = fs;
        file->private_data = inode;
        file->inode_num = inode_num;
        file->offset = 0;
        file->flags = flags;
        
        return fd;
    }
    
    ssize_t ext2_vfs_read(int fd, void *buffer, size_t size) {
        struct vfs_file *file = &vfs_file_table[fd];
        struct ext2_fs *fs = file->fs_data;
        struct ext2_inode *inode = file->private_data;
        
        // 1. Read data at current offset
        ssize_t bytes_read = ext2_read_file(fs, inode, file->offset,
                                             buffer, size);
        
        // 2. Update file offset
        file->offset += bytes_read;
        
        return bytes_read;
    }

Writing Files
-------------

Writing is more complex as it requires allocating new blocks:

.. code-block:: c

    ssize_t ext2_write_file(struct ext2_fs *fs, struct ext2_inode *inode,
                            uint64_t offset, const void *buffer, size_t size) {
        uint32_t block_index = offset / fs->block_size;
        uint32_t block_offset = offset % fs->block_size;
        size_t bytes_written = 0;
        
        char *block_buffer = kmalloc(fs->block_size);
        
        while (bytes_written < size) {
            // 1. Get or allocate physical block
            uint32_t phys_block = ext2_get_block(fs, inode, block_index);
            if (phys_block == 0) {
                phys_block = ext2_alloc_block(fs);
                if (phys_block == 0) {
                    break;  // No space
                }
                ext2_set_block(fs, inode, block_index, phys_block);
            }
            
            // 2. Handle partial block write
            if (block_offset != 0 || size - bytes_written < fs->block_size) {
                // Read-modify-write for partial block
                read_block(fs, phys_block, block_buffer);
            }
            
            // 3. Copy data to block buffer
            size_t bytes_in_block = fs->block_size - block_offset;
            if (bytes_written + bytes_in_block > size) {
                bytes_in_block = size - bytes_written;
            }
            
            memcpy(block_buffer + block_offset,
                   (const char*)buffer + bytes_written,
                   bytes_in_block);
            
            // 4. Write block to disk
            write_block(fs, phys_block, block_buffer);
            
            bytes_written += bytes_in_block;
            block_index++;
            block_offset = 0;
        }
        
        // 5. Update inode size if extended
        if (offset + bytes_written > inode->i_size) {
            inode->i_size = offset + bytes_written;
            ext2_write_inode(fs, inode);
        }
        
        kfree(block_buffer);
        return bytes_written;
    }

Block Allocation
~~~~~~~~~~~~~~~~

.. code-block:: c

    uint32_t ext2_alloc_block(struct ext2_fs *fs) {
        // 1. Read block bitmap
        uint32_t bitmap_block = fs->group_desc[0].bg_block_bitmap;
        uint8_t *bitmap = kmalloc(fs->block_size);
        read_block(fs, bitmap_block, bitmap);
        
        // 2. Find first free block
        for (uint32_t i = 0; i < fs->blocks_per_group; i++) {
            uint32_t byte_index = i / 8;
            uint32_t bit_index = i % 8;
            
            if (!(bitmap[byte_index] & (1 << bit_index))) {
                // 3. Mark block as used
                bitmap[byte_index] |= (1 << bit_index);
                write_block(fs, bitmap_block, bitmap);
                
                // 4. Update superblock free count
                fs->superblock.s_free_blocks_count--;
                ext2_write_superblock(fs);
                
                kfree(bitmap);
                return fs->first_data_block + i;
            }
        }
        
        kfree(bitmap);
        return 0;  // No free blocks
    }
    write_blocks(fs, start_block, buffer, num_blocks);

Reduces VirtIO notification overhead.

Limitations
-----------

Current Implementation
~~~~~~~~~~~~~~~~~~~~~~

- **Single Block Group**: Only supports filesystems with one block group
- **No Double/Triple Indirect**: Files limited to ~4 MB (12 direct + 1024 indirect blocks)
- **No Journaling**: No transaction support (not ext3/ext4)
- **No Extended Attributes**: No xattr support
- **Synchronous I/O**: Every operation waits for disk
- **No Block Preallocation**: Blocks allocated one at a time

Compatibility
~~~~~~~~~~~~~

ThunderOS ext2 driver is compatible with standard ext2 filesystems created by ``mkfs.ext2``:

.. code-block:: bash

    # Create compatible filesystem
    mkfs.ext2 -b 4096 -F disk.img 10M

**Supported Features:**

- ✓ Block sizes: 1024, 2048, 4096 bytes
- ✓ Standard inodes (128 or 256 bytes)
- ✓ Regular files and directories
- ✓ File permissions and ownership
- ✓ Hard links (multiple directory entries for same inode)

**Unsupported Features:**

- ✗ Symbolic links
- ✗ Special files (devices, FIFOs, sockets)
- ✗ Extended attributes
- ✗ Directory hashing (linear search only)
- ✗ Large files (>4MB currently)

Testing
-------

Creating a Test Filesystem
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Create 10MB disk image
    dd if=/dev/zero of=test.img bs=1M count=10
    
    # Format with ext2 (4KB blocks)
    mkfs.ext2 -b 4096 -F test.img
    
    # Mount temporarily to add files
    mkdir -p mnt
    sudo mount -o loop test.img mnt
    
    # Add test files
    echo "Hello, ext2!" > mnt/test.txt
    mkdir mnt/bin
    cp program.elf mnt/bin/program
    
    # Unmount
    sudo umount mnt
    
    # Run ThunderOS with disk
    qemu-system-riscv64 \
        -machine virt -m 128M -nographic \
        -kernel build/thunderos.elf \
        -drive file=test.img,if=none,format=raw,id=hd0 \
        -device virtio-blk-device,drive=hd0

Verification
~~~~~~~~~~~~

In ThunderOS shell:

.. code-block:: text

    ThunderOS> ls /
    .
    ..
    lost+found
    test.txt
    bin
    
    ThunderOS> cat /test.txt
    Hello, ext2!
    
    ThunderOS> ls /bin
    .
    ..
    program

Debugging
---------

Common Issues
~~~~~~~~~~~~~

**Invalid Magic Number:**

.. code-block:: c

    if (sb->s_magic != 0xEF53) {
        kprintf("Not an ext2 filesystem\n");
        kprintf("Magic: 0x%x (expected 0xEF53)\n", sb->s_magic);
    }

Check disk image is actually ext2.

**Wrong Block Size:**

.. code-block:: c

    kprintf("Block size: %d bytes\n", 1024 << sb->s_log_block_size);

Should be 1024, 2048, or 4096. VirtIO reads in 512-byte sectors, so convert accordingly.

**Inode Not Found:**

.. code-block:: c

    kprintf("Inode table at block %d\n", fs->inode_table_block);
    kprintf("Looking for inode %d\n", inode_num);
    kprintf("Inodes per group: %d\n", fs->inodes_per_group);

Check inode number is valid (< ``s_inodes_count``).

**Corrupted Directory:**

.. code-block:: c

    while (offset < dir_size) {
        struct ext2_dir_entry *entry = ...;
        
        if (entry->rec_len == 0 || entry->rec_len > dir_size - offset) {
            kprintf("Corrupted directory entry at offset %d\n", offset);
            break;
        }
        
        offset += entry->rec_len;
    }

Directory entry ``rec_len`` must be > 0 and aligned to 4 bytes.

Tools
~~~~~

**debugfs** - Interactive ext2 debugger:

.. code-block:: bash

    debugfs test.img
    
    debugfs: stats          # Show filesystem statistics
    debugfs: ls -l /        # List root directory
    debugfs: stat <2>       # Show inode 2 (root directory)
    debugfs: blocks <15>    # Show blocks used by inode 15
    debugfs: dump /test.txt /tmp/out  # Extract file

**e2fsck** - Filesystem checker:

.. code-block:: bash

    e2fsck -f test.img      # Force check
    e2fsck -p test.img      # Automatic repair

References
----------

- `ext2 Specification <https://www.nongnu.org/ext2-doc/ext2.html>`_
- `The Second Extended Filesystem (book) <https://www.kernel.org/doc/html/latest/filesystems/ext2.html>`_
- `Linux ext2 driver source <https://github.com/torvalds/linux/tree/master/fs/ext2>`_
- ThunderOS VFS: ``docs/source/internals/vfs.rst``
- ThunderOS VirtIO driver: ``docs/source/internals/virtio_block.rst``

Implementation Files
--------------------

- ``kernel/fs/ext2.c`` - Core ext2 implementation
- ``kernel/fs/ext2_vfs.c`` - VFS integration layer
- ``include/fs/ext2.h`` - ext2 data structures and constants
- ``kernel/fs/vfs.c`` - Virtual Filesystem layer
