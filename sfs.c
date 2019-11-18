#include "vfs.h"
#include "defs.h"

#define SFS_MAX_LENGTH 32
#define SFS_MAX_CHILDREN 16
#define SFS_MAX_INDIRECT_BLOCKS 64
#define SFS_MAGIC 0x3F3C007
#define SFS_SB_INODE_BITSIZE 4
#define SFS_SB_BLOCK_BITSIZE 120

enum sfs_type {
    SFS_INODE_DIR,
    SFS_INODE_FILE
};

struct superblock {
    int magic, root;
    int finode[SFS_SB_INODE_BITSIZE];
    int fblock[SFS_SB_BLOCK_BITSIZE];
};

struct inode {
    // on disk format
    char name[SFS_MAX_LENGTH];
    int type, inum;

    int parent;

    int child[SFS_MAX_CHILDREN];
    int indir[SFS_MAX_INDIRECT_BLOCKS];

    int n_child, size;

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

        for(int i = 0; i < root->n_child; i++)
        {
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

static inline int num_blocks(int size)
{
    return (size + VFS_BLOCK_SIZE - 1) / VFS_BLOCK_SIZE;
}

int sfs_readi(struct inode* ip, char* dst, int off, int size)
{
    // bad inode
    if(ip == 0 || ip->type != SFS_INODE_FILE) {
        return -1;
    }

    // figure out how many blocks to read
    int start = off / VFS_BLOCK_SIZE;
    size = (off + size) > ip->size ? ip->size - off : size;

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

    int i = 0;

    while(size > 0) {
        // already processed this block
        if(i == 0 && off != 0) {
            start++;
            i++;
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

//
// glibc implementation of ffs()
//   FIXME: this is GPL'd so it will have to be removed, since the rest
//   of this code is not under (nor do I want it to be) the GPL
//
static int ffs (int i)
{
  static const unsigned char table[] =
    {
      0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
      6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
      7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
      7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
      8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
      8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
      8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
      8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
    };
  unsigned int a;
  unsigned int x = i & -i;

  a = x <= 0xffff ? (x <= 0xff ? 0 : 8) : (x <= 0xffffff ?  16 : 24);

  return table[x >> a] + a;
}

static inline void set_bit(int* map, int bit)
{
    *map |= (1 << bit);
}

static void allocate_block(struct superblock* sb, struct inode* ip, int size)
{
    int fpos = 0;
    int bit = ffs(~(sb->fblock[fpos])) - 1;

    while(bit == -1) {
        fpos++;

        if(fpos >= SFS_SB_BLOCK_BITSIZE) {
            panic("sfs: out of file blocks\n");
        }

        bit = ffs(~(sb->fblock[fpos])) - 1;
    }

    set_bit(&sb->fblock[fpos], bit);
    int n_blocks = num_blocks(ip->size);

    ip->indir[n_blocks + 1] = fpos * 32 + bit + 128;
    ip->size = n_blocks * VFS_BLOCK_SIZE + size;
}

int sfs_writei(struct inode* ip, struct superblock* sb, const char* src, int off, int size)
{
    // bad inode
    if(ip == 0 || ip->type != SFS_INODE_FILE) {
        return -1;
    }

    int start = off / VFS_BLOCK_SIZE;
    int range = num_blocks(ip->size);

    int diff =  off - (start * VFS_BLOCK_SIZE);
    int amt = num_blocks(size - diff);

    // too large -- exceeds the number of blocks allowed
    if(start + amt > SFS_MAX_INDIRECT_BLOCKS) {
        return -1;
    }

    // allocate empty blocks (if necessary)
    while(start > num_blocks(ip->size)) {
        allocate_block(sb, ip, VFS_BLOCK_SIZE);
    }

    off = off - start * VFS_BLOCK_SIZE;

    int pos = 0;

    if(off != 0) {
        char block[VFS_BLOCK_SIZE];
        ip->drv->bread(ip->drv, block, ip->indir[start]);

        memmove(block + diff, src, VFS_BLOCK_SIZE - diff);
        ip->drv->bwrite(ip->drv, block, ip->indir[start]);

        pos += VFS_BLOCK_SIZE - diff;
        size -= VFS_BLOCK_SIZE - diff;
    }

    int i = 0;

    while(size > 0) {
        // already processed this block
        if(i == 0 && off != 0) {
            start++;
            i++;
            continue;
        }

        char block[VFS_BLOCK_SIZE];
        memset(block, 0, VFS_BLOCK_SIZE);

        int diff = size < VFS_BLOCK_SIZE ? size : VFS_BLOCK_SIZE;

        // partial write -- read block from disk
        if(start < range && diff < VFS_BLOCK_SIZE) {
            ip->drv->bread(ip->drv, block, ip->indir[start]);
        }

        memmove(block, src + pos, diff);
        ip->drv->bwrite(ip->drv, block, ip->indir[start]);

        start++;
        pos += diff;
        size -= diff;
    }

    ip->size += pos;
    return pos;
}

static struct fs_ops ops = {
    .readsb = sfs_readsb,
    .writesb = sfs_writesb,

    .namei = sfs_namei,
    .writei = sfs_writei,
    .readi = sfs_readi
};

void sfs_init()
{
    vfs_register_fs("sfs", &ops);
}
