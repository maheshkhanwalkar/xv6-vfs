#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/wait.h>

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

    int n_child, size;
    int n_blocks;
};

static inline void set_bit(int* map, int bit)
{
    *map |= (1 << bit);
}

static void write_inode(struct inode* ip, FILE* fp, int off)
{
    int block = ip->inum + 1;

    fseek(fp, off + block * VFS_BLOCK_SIZE, 0);
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
    ip->type = SFS_INODE_FILE;
    ip->n_child = 0;
    ip->size = 0;
    ip->n_blocks = 0;

    strcpy(ip->name, name);
    return ip;
}

static void write_blocks(struct inode* ip, struct superblock* sb, const char* file, FILE* fsp, int off)
{
    FILE* fp = fopen(file, "rb");

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);

    ip->size = sz;
    int count = (sz + VFS_BLOCK_SIZE - 1) / VFS_BLOCK_SIZE;

    if(count > SFS_MAX_INDIRECT_BLOCKS) {
        printf("warning. file is too big, skipping\n");
        fclose(fp);

        return;
    }

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

        ip->indir[i] = fpos * 32 + bit + 128;
        set_bit(&sb->fblock[fpos], bit);

        fread(block, VFS_BLOCK_SIZE, 1, fp);

        fseek(fsp, off + VFS_BLOCK_SIZE * (ip->indir[i] + 1), 0);
        fwrite(block, VFS_BLOCK_SIZE, 1, fsp);
    }

    ip->n_blocks += count;
    fclose(fp);
}

#define DISK_SIZE (512 * 1024)

void make_disk(const char* path)
{
    // create a blank disk image
    FILE* fp = fopen(path, "wb");

    char block[VFS_BLOCK_SIZE];
    memset(block, 0, VFS_BLOCK_SIZE);

    for(int i = 0; i < DISK_SIZE / VFS_BLOCK_SIZE; i++) {
        fwrite(block, VFS_BLOCK_SIZE, 1, fp);
    }

    fflush(fp);
    fclose(fp);

    // format the disk image using a script
    int res = system("./prep.sh");

    if(res == -1) {
        printf("error. script failed\n");
        exit(-1);
    }
}

// MBR related structures
struct part {
    char status;
    char f_chs[3]; // CHS of first sector
    char type;
    char l_chs[3]; // CHS of last sector
    int f_lba;     // LBA of first sector
    int count;     // Number of sectors
}__attribute__((packed));

struct mbr {
    char boot0[218];
    char timestamp[6];
    char boot1[216];
    int disk_sig;
    short prot;
    struct part p[4];
    short boot; // 0x55AA
}__attribute__((packed));


struct mbr* mbr_read(FILE* fp)
{
    struct mbr* mbr = malloc(sizeof(*mbr));

    rewind(fp);
    fread(mbr, sizeof(*mbr), 1, fp);

    return mbr;
}

int mbr_getoffset(struct mbr* mbr, int part)
{
    // bad or empty partition
    if(part >= 4 || mbr->p[part].count == 0) {
        return -1;
    }

    return (mbr->p[part].f_lba - 1) * 512;
}

int main(int argc, const char* argv[])
{
    if(argc < 3) {
      printf("error. no disk image and/or partition provided!\n");
      return -1;
    }

    FILE* fp = fopen(argv[1], "r+b");

    // create a new disk image (if it doesn't exist)
    if(fp == 0) {
        make_disk(argv[1]);
        fp = fopen(argv[1], "r+b");

        // give up at this point
        if(fp == 0) {
            printf("still can't open disk image! giving up\n");
            return -1;
        }
    }

    // partition to format
    int part = atoi(argv[2]);
    struct mbr* mbr = mbr_read(fp);
    int off = mbr_getoffset(mbr, part);

    if(off == -1) {
        printf("error. bad partition number specified\n");
        return -1;
    }

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

    int pos = 0;

    for(int i = 3; i < argc; i++) {
        // ignore the leading underscore
        const char* file = argv[i][0] == '_' ? argv[i] + 1 : argv[i];
        struct inode* ip = make_inode(file, sb);

        // write out the blocks
        write_blocks(ip, sb, argv[i], fp, off);

        // write the inode
        write_inode(ip, fp, off);

        root->child[pos] = ip->inum;
        pos++;

        free(ip);
    }

    root->n_child = pos;

    // write root inode
    write_inode(root, fp, off);

    // write superblock
    fseek(fp, off + VFS_BLOCK_SIZE, 0);
    fwrite(sb, sizeof(*sb), 1, fp);

    fflush(fp);
    fclose(fp);

    free(root);
    free(sb);

    return 0;
}
