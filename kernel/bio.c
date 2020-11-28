// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
  struct spinlock locks[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf heads[NBUCKET];
} bcache;

uint
bhash(uint dev, uint blockno) {
  // 2^32 mod 13 = 9
  // return (9 * (dev % NBUCKET) + (blockno % NBUCKET)) % NBUCKET;
  return ((uint64)dev << 32 | (uint32)blockno) % NBUCKET;
}

void
binit(void)
{
  struct buf *b;

  for(int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.locks[i], "bcache");

    // Create linked list of buffers
    bcache.heads[i].prev = &bcache.heads[i];
    bcache.heads[i].next = &bcache.heads[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.heads[0].next;
    b->prev = &bcache.heads[0];
    initsleeplock(&b->lock, "buffer");
    bcache.heads[0].next->prev = b;
    bcache.heads[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint hash = bhash(dev, blockno);

  acquire(&bcache.locks[hash]);

  // Is the block already cached?
  for(b = bcache.heads[hash].next; b != &bcache.heads[hash]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.locks[hash]);

  // Not cached; recycle an unused buffer.
  for(int buck = 0; buck < NBUCKET; buck++) {
    if(buck == hash)
      continue;
    uint found = 0;
    acquire(&bcache.locks[buck]);
    for(b = bcache.heads[buck].prev; b != &bcache.heads[buck]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->next->prev = b->prev;
        b->prev->next = b->next;
        found = 1;
        break;
      }
    }
    release(&bcache.locks[buck]);
    if(!found)
      continue;
    acquire(&bcache.locks[hash]);
    b->next = bcache.heads[hash].next;
    b->prev = &bcache.heads[hash];
    bcache.heads[hash].next->prev = b;
    bcache.heads[hash].next = b;
    release(&bcache.locks[hash]);
    acquiresleep(&b->lock);
    return b;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint hash = bhash(b->dev, b-> blockno);

  acquire(&bcache.locks[hash]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.heads[hash].next;
    b->prev = &bcache.heads[hash];
    bcache.heads[hash].next->prev = b;
    bcache.heads[hash].next = b;
  }
  
  release(&bcache.locks[hash]);
}

void
bpin(struct buf *b) {
  uint hash = bhash(b->dev, b->blockno);
  acquire(&bcache.locks[hash]);
  b->refcnt++;
  release(&bcache.locks[hash]);
}

void
bunpin(struct buf *b) {
  uint hash = bhash(b->dev, b->blockno);
  acquire(&bcache.locks[hash]);
  b->refcnt--;
  release(&bcache.locks[hash]);
}


