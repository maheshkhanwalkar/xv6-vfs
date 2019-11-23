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
    int n_blocks;

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
                path += len + 1;
                len = slen(path);
                i = -1;
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

static void allocate_block(struct superblock* sb, struct inode* ip)
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

    ip->indir[ip->n_blocks] = fpos * 32 + bit + 128;
    ip->n_blocks++;
}

int sfs_writei(struct inode* ip, struct superblock* sb, const char* src, int off, int size)
{
    // bad inode
    if(ip == 0 || ip->type != SFS_INODE_FILE) {
        return -1;
    }

    // compute how much additional byte-space is needed
    int left = ip->n_blocks * VFS_BLOCK_SIZE - ip->size;
    int n_size = size - left;

    if(n_size < 0) {
        n_size = 0;
    }

    int blocks = num_blocks(n_size);
    int start = ip->n_blocks - 1;
    int special = 0;

    if(start < 0) {
        start = 0;
        special = 1;
    }

    // allocate the new blocks
    for(int i = 0; i < blocks; i++) {
        allocate_block(sb, ip);
    }

    // handle left-over space in pre-existing block
    int pos = 0;

    if(left != 0) {
        char block[VFS_BLOCK_SIZE];
        ip->drv->bread(ip->drv, block, ip->indir[start]);

        int bytes = left > size ? size : left;
        memmove(block + VFS_BLOCK_SIZE - left, src, bytes);

        ip->drv->bwrite(ip->drv, block, ip->indir[start]);

        start++;
        pos += bytes;
        size -= bytes;
    }
    else if (!special) {
        start++;
    }

    while(size > 0)
    {
        char block[VFS_BLOCK_SIZE];
        int bytes = VFS_BLOCK_SIZE > size ? size : VFS_BLOCK_SIZE;

        memmove(block, src + pos, bytes);
        ip->drv->bwrite(ip->drv, block, ip->indir[start]);

        start++;
        pos += bytes;
        size -= bytes;
    }

    // update inode on disk
    ip->size += pos;
    ip->drv->bwrite(ip->drv, ip, ip->inum);

    return pos;
}

static int last_slash(const char* path)
{
    const char* ptr = path;
    int pos = 0;

    while(*ptr != '\0')
    {
        if(*ptr == '/') {
            pos = ptr - path;
        }

        ptr++;
    }

    return pos;
}

static struct inode* allocate_inode(struct superblock* sb, const char* name)
{
    int fpos = 0;
    int bit = ffs(~(sb->finode[fpos])) - 1;

    while(bit == -1) {
        fpos++;

        if(fpos >= SFS_SB_INODE_BITSIZE) {
            panic("sfs: out of inode blocks\n");
        }

        bit = ffs(~(sb->finode[fpos])) - 1;
    }

    set_bit(&sb->finode[fpos], bit);

    struct inode* ip = (void*)kalloc();

    ip->inum = fpos * 32 + bit;
    ip->n_child = 0;
    ip->size = 0;
    ip->n_blocks = 0;

    int len = strlen(name);
    strncpy(ip->name, name, len);
    ip->name[len] = '\0';

    return ip;
}

struct inode* sfs_createi(const char* path, int type, struct superblock* sb, struct block_driver* drv)
{
    int pos = last_slash(path);
    char* buffer = kalloc();

    // keep leading '/'
    if(pos == 0) {
        pos++;
    }

    memmove(buffer, path, pos);
    buffer[pos] = '\0';

    struct inode* parent = sfs_namei(buffer, sb, drv);

    // bad parent path
    if(parent == 0) {
        return 0;
    }

    if(pos != 1) {
        pos++;
    }

    struct inode* ip = allocate_inode(sb, path + pos);
    ip->type = (type == VFS_INODE_FILE) ? SFS_INODE_FILE : SFS_INODE_DIR;
    ip->parent = parent->inum;
    ip->drv = drv;

    parent->child[parent->n_child] = ip->inum;
    parent->n_child++;

    // update inodes on disk
    drv->bwrite(drv, ip, ip->inum);
    drv->bwrite(drv, parent, parent->inum);

    return ip;
}

static struct fs_ops ops = {
    .readsb = sfs_readsb,
    .writesb = sfs_writesb,

    .namei = sfs_namei,
    .createi = sfs_createi,
    .writei = sfs_writei,
    .readi = sfs_readi
};

void sfs_init()
{
    vfs_register_fs("sfs", &ops);
}
