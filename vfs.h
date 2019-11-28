// VFS header

#pragma once
#include "stat.h"

/**
 * The size of a block that is assumed/used by the VFS
 *
 * This block size may or may not be larger than the actual block size on a particular
 * type of disk. Therefore, it is the job of the block_driver to translate VFS_BLOCK_SIZE
 * blocks (and block numbers) to internal block sizes and numbers.
 * 
 */
#define VFS_BLOCK_SIZE 512


#define VFS_INODE_FILE 0
#define VFS_INODE_DIR  1

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
     * Read a block from the block device
     *
     * @param self - pointer to the containing structure
     * @param buffer - destination buffer of the read
     * @param b_num - which block to read
     *
     * @return number of bytes read (-1 on failure)
     */
    int (*bread)(struct block_driver* self, void* buffer, int b_num);

    /**
     * Write a block to the block device
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
 * Character driver
 *
 * This struct specifies the required interface that all character drivers
 * need to conform to, so that they can be used by higher levels of the filesystem stack
 *
 * Specifically, two operations need to be supported: read and write, which allow for a
 * buffer to be read from and written to the underlying character device, respectively.
 */
struct char_driver {

    /**
     * Read bytes from the char device
     *
     * @param buffer - destination buffer of the read
     * @param size - number of bytes to read
     *
     * @return number of bytes read (-1 on failure)
     */
    int (*read)(char* buffer, int size);

    /**
     * Write bytes to the char device
     *
     * @param buffer - source buffer of the write
     * @param size - number of bytes to write
     *
     * @return number of bytes written (-1 on failure)
     */
    int (*write)(const char* buffer, int size);
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
 * Register a character driver
 *
 * Associate a character driver with the VFS, using a uniquely identifying name,
 * which allows it to be referenced.
 *
 *   e.g. vfs_register_char("console", &my_char_drv)
 *
 * This will be used later, if the particular character device is mounted onto the
 * root partition as a special device.
 */
void vfs_register_char(const char* name, struct char_driver* drv);

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


/**
 * Filesystem operations
 *
 * This struct contains all the necessary operations that a filesystem needs to support in
 * order to registered with the VFS subsystem.
 *
 * Each filesystem should provide some implementation for each of the functions in the struct.
 * However, it is up to the underlying filesystem to make it functional or a no-op, if it is not
 * used or supported.
 *
 */
struct fs_ops {
    struct superblock* (*readsb)(struct block_driver*);
    void (*writesb)(struct superblock*, struct block_driver*);

    struct inode* (*namei)(const char*, struct superblock*, struct block_driver*);
    struct inode* (*createi)(const char*, int type, struct superblock*, struct block_driver*);

    int (*writei)(struct inode*, struct superblock* sb, const char* src, int off, int size);
    int (*readi)(struct inode*, char* dst, int off, int size);

    void (*stati)(struct inode*, struct stat*);
    struct inode* (*childi)(struct inode*, int);
    const char* (*iname)(struct inode*, int full);
    struct inode* (*parenti)(struct inode*);
};

void vfs_register_fs(const char* name, struct fs_ops* ops);
void vfs_mount_fs(const char* path, const char* dev, const char* fs);

void vfs_mount_char(const char* path, const char* dev);
void vfs_mount_block(const char* path, const char* dev);

struct vfs_inode* vfs_namei(const char* path);
struct vfs_inode* vfs_createi(const char* path, int type);

int vfs_writei(struct vfs_inode* vi, char* src, int off, int size);
int vfs_readi(struct vfs_inode* vi, char* dst, int off, int size);

void vfs_stati(struct vfs_inode* vi, struct stat* st);
struct vfs_inode* vfs_childi(struct vfs_inode* vi, int child);
const char* vfs_iname(struct vfs_inode* vi, int full);

struct vfs_inode* vfs_parenti(struct vfs_inode* vi);
