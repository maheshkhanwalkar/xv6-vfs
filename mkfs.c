#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define VFS_BLOCK_SIZE 512
#define SFS_MAGIC 0x3F3C007

struct superblock {
    int magic, root;
    int finode[4];
    int fblock[120];
};

int main(int argc, const char* argv[])
{
    if(argc < 2) {
      printf("error. no disk image provided!\n");
    }

    FILE* fp = fopen(argv[1], "r+b");
    fseek(fp, VFS_BLOCK_SIZE, 0);

    struct superblock* sb = calloc(1, sizeof(*sb));
    sb->magic = SFS_MAGIC;
    sb->root = 2;

    fwrite(sb, sizeof(*sb), 1, fp);




    fflush(fp);
    fclose(fp);

    return 0;
}
