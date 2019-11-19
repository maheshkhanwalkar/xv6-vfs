// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "vfs.h"
#include "queue.h"
#include "mbr.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

struct block {
  void* buffer;
  int device, start;
  int op, done;
};

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

static struct spinlock idelock;
static struct buf *idequeue;
static queue_t q;

static int havedisk1;
static void idestart(struct buf*);

// Hooks for VFS
static int ide_bread(struct block_driver* self, void* buffer, int b_num);
static int ide_bwrite(struct block_driver* self, void* buffer, int b_num);

// Wait for IDE disk to become ready.
static int
idewait(int checkerr)
{
  int r;

  while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}

void
ideinit(void)
{
  int i;

  initlock(&idelock, "ide");
  ioapicenable(IRQ_IDE, ncpu - 1);
  idewait(0);

  // initialise the queue
  q = queue_create();

  struct block_driver* drv = (void*)kalloc();

  // FIXME: actually find out big the disk is!!
  //   TODO: look into how to get the number of sectors (in LBA) that the disk
  //   supports and use that for b_end
  drv->info.b_start = 0;
  drv->info.b_end = 65535;
  drv->device = 1;

  // Add function hooks
  drv->bread = ide_bread;
  drv->bwrite = ide_bwrite;

  // Read MBR from disk
  char* mbr = kalloc();
  ide_bread(drv, mbr, 0);

  // Parse MBR partition information
  int count = mbr_count(mbr);

  static const char* names[] = {
    "sda0",
    "sda1",
    "sda2",
    "sda3"
  };

  cprintf("%d\n", count);

  for(int i = 0; i < count; i++) {
    struct mbr_part curr;
    mbr_get(mbr, i, &curr);

    // Add function hooks
    drv->bread = ide_bread;
    drv->bwrite = ide_bwrite;

    // Partition information
    drv->info.b_start = curr.start;
    drv->info.b_end = curr.end;
    drv->device = 1;

    // Register with VFS
    vfs_register_block(names[i], drv);
    drv++;
  }

  // Check if disk 1 is present
  outb(0x1f6, 0xe0 | (1<<4));
  for(i=0; i<1000; i++){
    if(inb(0x1f7) != 0){
      havedisk1 = 1;
      break;
    }
  }

  // Switch back to disk 0.
  outb(0x1f6, 0xe0 | (0<<4));
}

// Start the request for b.  Caller must hold idelock.
static void
idestart(struct buf *b)
{
  if(b == 0)
    panic("idestart");
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
  int sector_per_block =  BSIZE/SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  if (sector_per_block > 7) panic("idestart");

  idewait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, sector_per_block);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));
  if(b->flags & B_DIRTY){
    outb(0x1f7, write_cmd);
    outsl(0x1f0, b->data, BSIZE/4);
  } else {
    outb(0x1f7, read_cmd);
  }
}

static void
ide_commit(struct block* b)
{
  if(b == 0) {
    panic("ide_commit");
  }

  int s_bcount = VFS_BLOCK_SIZE / SECTOR_SIZE;
  int sector = b->start * s_bcount;

  int read_cmd = (s_bcount == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (s_bcount == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  if (s_bcount > 7) {
      panic("ide_commit: too many sectors per block");
  }

  idewait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, s_bcount);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | ((b->device&1)<<4) | ((sector>>24)&0x0f));

  if(b->op == IDE_CMD_WRITE) {
    outb(0x1f7, write_cmd);
    outsl(0x1f0, b->buffer, VFS_BLOCK_SIZE / sizeof(long));
  }
  else {
    outb(0x1f7, read_cmd);
  }
}

static void busywait(struct spinlock* lk, int* cond)
{
    while(*cond != 1) {
        release(lk);
        for(int i = 0; i < 1000; i++);
        acquire(lk);
    }
}

// Interrupt handler.
void
ideintr(void)
{
  struct block* b;

  acquire(&idelock);

  if((b = queue_deq(q)) == 0){
    release(&idelock);
    return;
  }

  if(b->op == IDE_CMD_READ) {
    insl(0x1f0, b->buffer, VFS_BLOCK_SIZE / 4);
  }

  b->done = 1;
  wakeup(b);

  if((b = queue_deq(q)) != 0) {
    ide_commit(b);
    busywait(&idelock, &(b->done));
  }

  release(&idelock);

  /*struct buf *b;

  // First queued buffer is the active request.
  acquire(&idelock);

  if((b = idequeue) == 0){
    release(&idelock);
    return;
  }
  idequeue = b->qnext;

  // Read data if needed.
  if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
    insl(0x1f0, b->data, BSIZE/4);

  // Wake process waiting for this buf.
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  wakeup(b);

  // Start disk on next buf in queue.
  if(idequeue != 0)
    idestart(idequeue);

  release(&idelock);*/
}

//PAGEBREAK!
// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
void
iderw(struct buf *b)
{
  struct buf **pp;

  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
    panic("iderw: nothing to do");
  if(b->dev != 0 && !havedisk1)
    panic("iderw: ide disk 1 not present");

  acquire(&idelock);  //DOC:acquire-lock

  // Append b to idequeue.
  b->qnext = 0;
  for(pp=&idequeue; *pp; pp=&(*pp)->qnext)  //DOC:insert-queue
    ;
  *pp = b;

  // Start disk if necessary.
  if(idequeue == b)
    idestart(b);

  // Wait for request to finish.
  while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){
    sleep(b, &idelock);
  }


  release(&idelock);
}

static int ide_bread(struct block_driver* self, void* buffer, int b_num)
{
  struct block* b = (void*)kalloc();

  b->buffer = buffer;
  b->device = self->device;
  b->start = self->info.b_start + b_num;
  b->op = IDE_CMD_READ;
  b->done = 0;

  acquire(&idelock);
  queue_enq(q, b);

  // process now, if its the only item in the queue
  if(queue_peek(q) == b) {
    ide_commit(b);
  }

  busywait(&idelock, &(b->done));
  /*if(myproc() == 0) {
    busywait(&idelock, &(b->done));
  }
  else
  {
    while(!b->done) {
      sleep(b, &idelock);
    }
  }*/

  kfree((void*)b);
  release(&idelock);

  return 0;
}

static int ide_bwrite(struct block_driver* self, void* buffer, int b_num)
{
  struct block* b = (void*)kalloc();

  b->buffer = buffer;
  b->device = self->device;
  b->start = self->info.b_start + b_num;
  b->op = IDE_CMD_WRITE;
  b->done = 0;

  acquire(&idelock);
  queue_enq(q, b);

  // process now, if its the only item in the queue
  if(queue_peek(q) == b) {
    ide_commit(b);
    busywait(&idelock, &(b->done));
  }

  kfree((void*)b);
  release(&idelock);

  return 0;
}
