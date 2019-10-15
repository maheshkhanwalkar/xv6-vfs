// VFS header

#pragma once

/**
 * Block driver
 * 
 * This struct specifies the required interface that all block drivers
 * need to conform to, so that they can be used by higher levels of the filesystem stack
 * 
 * Specifically, two operations need to be supported: bread and bwrite, which allow for a
 * block to be read from and written to the underlying device, respectively.
 */
struct block_driver {
    /**
     * Read a sector from the block device
     * 
     * @param buffer - destination buffer of the read
     * @param device - device to read from
     * @param b_num - which block to read
     * 
     * @return number of bytes read (-1 on failure)
     */
    int (*bread)(void* buffer, int device, int b_num);

    /**
     * Write a sector to the block device
     * 
     * @param buffer - data to write to the sector
     * @param device - device to write to
     * @param b_num - which block to write
     * 
     * @return number of bytes written (-1 on failure)
     */ 
    int (*bwrite)(void* buffer, int device, int b_num);
};

