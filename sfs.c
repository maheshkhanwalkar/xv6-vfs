#include "vfs.h"
#include "defs.h"

#define SFS_MAX_CHILDREN 16
#define SFS_MAX_INDIRECT_BLOCKS 108

enum sfs_type {
    SFS_INODE_DIR,
    SFS_INODE_FILE
};

struct inode {
    // on disk format
    int type, num;
    int child[SFS_MAX_CHILDREN];
    int parent, self;
    int indir[108];

    // additional in-memory fields
    struct block_driver* drv;
    int valid;
};

struct inode* sfs_namei(const char* path, struct block_driver* drv)
{
    // lazy initialisation -- don't actually read from disk yet
    struct inode* ip = (void*)kalloc();
    ip->drv = drv;
    ip->valid = 0;

    return ip;
}

int sfs_readi(struct inode* ip, char* dst, int off, int size)
{
    return 0;
}

static struct fs_ops ops = {
    .namei = sfs_namei,
    .writei = 0,
    .readi = sfs_readi
};


void sfs_init()
{
    vfs_register_fs("sfs", &ops);
}


