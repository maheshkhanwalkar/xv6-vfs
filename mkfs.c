#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define VFS_BLOCK_SIZE 512

#define SFS_MAGIC 0x3F3C007
#define SFS_MAX_LENGTH 32
#define SFS_MAX_CHILDREN 16
#define SFS_MAX_INDIRECT_BLOCKS 64

struct superblock {
    int magic, root;
    int finode[4];
    int fblock[120];
};

enum sfs_type {
    SFS_INODE_DIR,
    SFS_INODE_FILE
};

struct inode {
    char name[SFS_MAX_LENGTH];
    int type, inum;

    int parent;

    int child[SFS_MAX_CHILDREN];
    int indir[SFS_MAX_INDIRECT_BLOCKS];
};

static inline void set_bit(int* map, int bit)
{
    *map |= (1 << bit);
}

static void write_inode(struct inode* ip, FILE* fp)
{
    int block = ip->inum + 1;

    fseek(fp, block * VFS_BLOCK_SIZE, 0);
    fwrite(ip, sizeof(*ip), 1, fp);
}

static struct inode* make_inode(const char* name, struct superblock* sb)
{
    struct inode* ip = calloc(1, sizeof(*ip));

    ip->inum = ffs(~(sb->finode[0])) - 1;
    set_bit(&sb->finode[0], ip->inum);

    ip->parent = 1;

    for(int i = 0; i < SFS_MAX_CHILDREN; i++) {
        ip->child[i] = -1;
    }

    strcpy(ip->name, name);
    return ip;
}

int main(int argc, const char* argv[])
{
    if(argc < 2) {
      printf("error. no disk image provided!\n");
      return -1;
    }

    FILE* fp = fopen(argv[1], "r+b");

    // create superblock
    struct superblock* sb = calloc(1, sizeof(*sb));
    sb->magic = SFS_MAGIC;
    sb->root = 1;

    // create root inode
    set_bit(&sb->finode[0], 0);
    set_bit(&sb->finode[0], 1);

    struct inode* root = calloc(1, sizeof(*root));
    strcpy(root->name, "/");

    root->type = SFS_INODE_DIR;
    root->inum = 1;
    root->parent = root->inum;

    // no children (-1 inode)
    for(int i = 0; i < SFS_MAX_CHILDREN; i++) {
      root->child[i] = -1;
    }

    int pos = 0;

    for(int i = 2; i < argc; i++) {
        // ignore the leading underscore
        const char* file = argv[i] + 1;
        struct inode* ip = make_inode(file, sb);

        write_inode(ip, fp);

        root->child[pos] = ip->inum;
        pos++;

        free(ip);
    }

    // write root inode
    write_inode(root, fp);

    // write superblock
    fseek(fp, VFS_BLOCK_SIZE, 0);
    fwrite(sb, sizeof(*sb), 1, fp);

    fflush(fp);
    fclose(fp);

    return 0;
}
