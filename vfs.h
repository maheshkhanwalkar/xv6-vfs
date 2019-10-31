// VFS header

#pragma once

/**
 * The size of a block that is assumed/used by the VFS
 *
 * This block size may or may not be larger than the actual block size on a particular
 * type of disk. Therefore, it is the job of the block_driver to translate VFS_BLOCK_SIZE
 * blocks (and block numbers) to internal block sizes and numbers.
 * 
 */
#define VFS_BLOCK_SIZE 512


/**
 * Partition information
 *
 * This struct contains the minimal information needed to represent a partition:
 *   + b_start: the start block number on-disk for the partition
 *   + b_end: the end block number on-disk for the partition
 */
struct partition {
    int b_start, b_end;
};

/**
 * Block driver
 *
 * This struct specifies the required interface that all block drivers
 * need to conform to, so that they can be used by higher levels of the filesystem stack
 * 
 * Specifically, two operations need to be supported: bread and bwrite, which allow for a
 * block to be read from and written to the underlying device, respectively.
 */
struct block_driver {

    /**
     * Partition information
     *
     * Which partition this block driver is operating on. Each partition on a disk should have
     * its own block_driver structure ``instance''
     *
     * This information will be used to create an offset for block numbers, so the underlying bread
     * and bwrite operations can exist per type-of-disk, rather than per-disk or per-partition
     */
    struct partition info;

    /**
     * Device number
     *
     * This is a device-specific (and driver specific) number, not something specified by the
     * VFS layer. It can be used to identify a particular device, in case there are more than one
     * of a particular type.
     *
     */
    int device;

    /**
     * Read a sector from the block device
     *
     * @param self - pointer to the containing structure
     * @param buffer - destination buffer of the read
     * @param b_num - which block to read
     *
     * @return number of bytes read (-1 on failure)
     */
    int (*bread)(struct block_driver* self, void* buffer, int b_num);

    /**
     * Write a sector to the block device
     *
     * @param self - pointer to the containing structure
     * @param buffer - data to write to the sector
     * @param b_num - which block to write
     *
     * @return number of bytes written (-1 on failure)
     */ 
    int (*bwrite)(struct block_driver* self, void* buffer, int b_num);
};

/**
 * Initialise the VFS subsystem
 *
 * This handles any initialisation and setup of data structures used by
 * the VFS to maintain mappings and functionality.
 */
void vfs_init();


/**
 * Register a block driver
 *
 * Associate a block driver with the VFS, using a uniquely identifying name,
 * which allows it to be referenced.
 *
 *   e.g. vfs_register_block("sda0", &my_block_drv)
 *
 * This name will be used later when a filesystem will be mounted, since the
 * partition that will be read will be done via the block driver.
 *
 */
void vfs_register_block(const char* name, struct block_driver* drv);


/**
 * inode struct declaration
 *
 * The actual definition -- layout -- obviously depends on the filesystem and will
 * be defined within its source code.
 *
 * However, the VFS still operates on inodes, which it can modify and retrieve using
 * fs-specific hooks that are provided to it when a filesystem is registered.
 *
 */
struct inode;


/**
 * VFS inode
 *
 * This is the inode structure that the VFS exposes out to the rest of the kernel,
 * in that vfs_inodes are manipulated by the public VFS functions.
 *
 * The vfs_inode exists as a thin wrapper over the actual filesystem inode and any
 * operations are dispatched to the underlying filesystem.
 *
 */
struct vfs_inode;


struct fs_ops {
    struct inode* (*namei)(const char*, struct block_driver*);
    void (*writei)(struct inode*, const char* src, int size);
    void (*readi)(struct inode*, char* dst, int off, int size);
};

void vfs_register_fs(const char* name, struct fs_ops* ops);
void vfs_mount_fs(const char* path, const char* dev, const char* fs);

struct vfs_inode* vfs_namei(const char* path);
void vfs_writei(struct vfs_inode* vi, char* src, int size);
void vfs_readi(struct vfs_inode* vi, char* dst, int off, int size);
