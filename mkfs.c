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
#define SFS_SB_INODE_BITSIZE 4
#define SFS_SB_BLOCK_BITSIZE 120

struct superblock {
    int magic, root;
    int finode[SFS_SB_INODE_BITSIZE];
    int fblock[SFS_SB_BLOCK_BITSIZE];
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

    int fpos = 0;
    ip->inum = ffs(~(sb->finode[fpos])) - 1;

    while(ip->inum == -1) {
        fpos++;

        if(fpos >= SFS_SB_INODE_BITSIZE) {
            printf("error. out of inodes!\n");
            exit(-1);
        }

        ip->inum = ffs(~(sb->finode[fpos])) - 1;
    }

    set_bit(&sb->finode[fpos], ip->inum);

    ip->parent = 1;

    for(int i = 0; i < SFS_MAX_CHILDREN; i++) {
        ip->child[i] = -1;
    }

    strcpy(ip->name, name);
    return ip;
}

static void write_blocks(struct inode* ip, struct superblock* sb, const char* file, FILE* fsp)
{
    FILE* fp = fopen(file, "rb");

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);

    int count = (sz + VFS_BLOCK_SIZE - 1) / VFS_BLOCK_SIZE;

    for(int i = 0; i < count; i++) {
        char block[VFS_BLOCK_SIZE];
        memset(block, 0, VFS_BLOCK_SIZE);

        int fpos = 0;
        int bit = ffs(~(sb->fblock[fpos])) - 1;

        while(bit == -1) {
            fpos++;

            if(fpos >= SFS_SB_BLOCK_BITSIZE) {
                printf("error. out of file blocks\n");
                exit(-1);
            }

            bit = ffs(~(sb->fblock[fpos])) - 1;
        }

        ip->indir[i] = bit + 128;
        set_bit(&sb->fblock[fpos], bit);

        fread(block, VFS_BLOCK_SIZE, 1, fp);

        fseek(fsp, VFS_BLOCK_SIZE * (ip->indir[i] + 1), 0);
        fwrite(block, VFS_BLOCK_SIZE, 1, fsp);
    }

    fclose(fp);
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

        // write out the blocks
        write_blocks(ip, sb, file - 1, fp);

        // write the inode
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
