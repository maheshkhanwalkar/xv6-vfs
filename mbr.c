#include "mbr.h"

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

int mbr_count(void* raw)
{
    struct mbr* mbr = raw;
    int count = 0;

    for(int i = 0; i < 4; i++) {
        if(mbr->p[i].count == 0) {
            break;
        }

        count++;
    }

    return count;
}

void mbr_get(void* raw, int part, struct mbr_part* out)
{
    struct mbr* mbr = raw;

    if(part < 0 || part >= 4) {
        return;
    }

    struct part* p = &(mbr->p[part]);

    out->type = p->type;
    out->start = p->f_lba;
    out->end = p->f_lba + p->count;
}
