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

#define M 13
struct {
  struct spinlock lock[M];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[M];
} bcache;

char lock_name[M][20];
void
binit(void)
{
  struct buf *b;
  for(int i = 0; i < M; ++i) {
    snprintf(lock_name[i], 20, "bcache.bucket%d", i);
    initlock(&bcache.lock[i], (char*)lock_name[i]);
  }

  for (int i = 0; i < M; ++i) {
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  int i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++, i = (i + 1) % M) {
    b->next = bcache.head[i].next;
    b->prev = &bcache.head[i];
    initsleeplock(&b->lock, "buffer");
    bcache.head[i].next->prev = b;
    bcache.head[i].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint idx = blockno % M;

  acquire(&bcache.lock[idx]);
  // Is the block already cached?
  for(b = bcache.head[idx].next; b != &bcache.head[idx]; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.head[idx].prev; b != &bcache.head[idx]; b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Steal
  for (int k = (idx + 1) % M; k != idx; k = (k + 1) % M) {
    acquire(&bcache.lock[k]);
    for (b = bcache.head[k].prev; b != &bcache.head[k]; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->prev->next = b->next;
        b->next->prev = b->prev;
        release(&bcache.lock[k]);
        b->next = bcache.head[idx].next;
        b->prev = &bcache.head[idx];
        release(&bcache.lock[idx]);
        b->next->prev = b;
        b->prev->next = b;
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[k]);
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
    virtio_disk_rw(b, 0);
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
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint idx = b->blockno % M;
  acquire(&bcache.lock[idx]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[idx].next;
    b->prev = &bcache.head[idx];
    bcache.head[idx].next->prev = b;
    bcache.head[idx].next = b;
  }
  
  release(&bcache.lock[idx]);
}

void
bpin(struct buf *b) {
  uint idx = b->blockno % M;
  acquire(&bcache.lock[idx]);
  b->refcnt++;
  release(&bcache.lock[idx]);
}

void
bunpin(struct buf *b) {
  uint idx = b->blockno % M;
  acquire(&bcache.lock[idx]);
  b->refcnt--;
  release(&bcache.lock[idx]);
}


