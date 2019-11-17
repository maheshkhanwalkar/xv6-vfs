#include "vfs.h"
#include "defs.h"

#define SFS_MAX_LENGTH 32
#define SFS_MAX_CHILDREN 16
#define SFS_MAX_INDIRECT_BLOCKS 64
#define SFS_MAGIC 0x3F3C007

enum sfs_type {
    SFS_INODE_DIR,
    SFS_INODE_FILE
};

struct superblock {
    int magic, root;
    int finode[4];
    int fblock[120];
};

struct inode {
    // on disk format
    char name[SFS_MAX_LENGTH];
    int type, inum;

    int parent;

    int child[SFS_MAX_CHILDREN];
    int indir[SFS_MAX_INDIRECT_BLOCKS];

    // additional in-memory fields
    struct block_driver* drv;
    int valid;
};

struct superblock* sfs_readsb(struct block_driver* drv)
{
    struct superblock* sb = (void*)kalloc();
    drv->bread(drv, sb, 0);

    // bad magic value (not SFS or corrupted!)
    if(sb->magic != SFS_MAGIC) {
        kfree((void*)sb);
        return 0;
    }

    return sb;
}

void sfs_writesb(struct superblock* sb, struct block_driver* drv)
{
    drv->bwrite(drv, sb, 0);
}

static int slen(const char* path)
{
    int count = 0;

    while(*path != '/' && *path != '\0') {
        path++;
        count++;
    }

    return count;
}

struct inode* sfs_namei(const char* path, struct superblock* sb, struct block_driver* drv)
{
    struct inode* root = (void*)kalloc();
    struct inode* tmp = root + 1;

    drv->bread(drv, root, sb->root);

    // sfs_namei("/", ...)
    if(path[0] == '/' && path[1] == '\0') {
        root->drv = drv;
        root->valid = 1;

        return root;
    }

    path++;

    while(1)
    {
        int len = slen(path);
        struct inode* old = root;

        for(int i = 0; i < SFS_MAX_CHILDREN; i++)
        {
            if(root->child[i] == -1)
                break;

            drv->bread(drv, tmp, root->child[i]);
            int diff = strncmp(path, tmp->name, len);

            // found a partial match
            if(diff == 0 && strlen(tmp->name) == len)
            {
                // partial --> full match
                if(path[len] == '\0') {
                    root = tmp;

                    root->drv = drv;
                    root->valid = 1;

                    return root;
                }

                root = tmp;
                path += len;
            }
        }

        // no matches
        if(old == root) {
            break;
        }
    }

    return 0;
}

int sfs_readi(struct inode* ip, char* dst, int off, int size)
{
    // bad inode
    if(ip == 0) {
        return -1;
    }

    // figure out how many blocks to read
    int start = off / VFS_BLOCK_SIZE;
    int amt = (size + VFS_BLOCK_SIZE - 1) / VFS_BLOCK_SIZE;

    off = off - start * VFS_BLOCK_SIZE;
    int pos = 0;

    if(off != 0) {
        char block[VFS_BLOCK_SIZE];
        ip->drv->bread(ip->drv, block, ip->indir[start]);

        int diff = size > VFS_BLOCK_SIZE ? VFS_BLOCK_SIZE - off : size;
        memmove(dst, block + off, diff);

        pos += diff;
        size -= diff;
    }

    for(int i = 0; i < amt; i++) {
        // already processed this block
        if(i == 0 && off != 0) {
            start++;
            continue;
        }

        char block[VFS_BLOCK_SIZE];
        ip->drv->bread(ip->drv, block, ip->indir[start]);

        int diff = size < VFS_BLOCK_SIZE ? size : VFS_BLOCK_SIZE;
        memmove(dst + pos, block, diff);

        start++;
        pos += diff;
        size -= diff;
    }

    return pos;
}

static struct fs_ops ops = {
    .readsb = sfs_readsb,
    .writesb = sfs_writesb,

    .namei = sfs_namei,
    .writei = 0,
    .readi = sfs_readi
};

void sfs_init()
{
    vfs_register_fs("sfs", &ops);
}
