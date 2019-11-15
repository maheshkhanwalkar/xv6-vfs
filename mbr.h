#pragma once

struct mbr_part {
    int start, end;
    char type;
};

/**
 * Parses the MBR and returns the number of partitions
 *
 * @param mbr - buffer containing the MBR
 * @return the number of partitions
 */
int mbr_count(void* mbr);

/**
 * Parses the MBR and returns block start and end information
 * about the specified partition
 *
 * @param mbr - buffer containing the MBR
 * @param part - partition number to return information about
 * @param out - where to place the partition information (out variable)
 */
void mbr_get(void* mbr, int part, struct mbr_part* out);
