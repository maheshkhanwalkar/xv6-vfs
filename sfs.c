#include "vfs.h"
#include "defs.h"

/*struct fs_ops {
    struct inode* (*namei)(const char*, struct block_driver*);
    void (*writei)(struct inode*, const char* src, int size, struct block_driver*);
    void (*readi)(struct inode*, char* dst, int off, int size, struct block_driver*);
};*/

struct inode {


    /*---------------------*/
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

static struct fs_ops ops = {
    .namei = sfs_namei,
    .writei = 0,
    .readi = 0
};


void sfs_init()
{
    vfs_register_fs("sfs", &ops);
}


