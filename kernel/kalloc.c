// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#define PGNUM ((PHYSTOP - KERNBASE) / PGSIZE)

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 refcnt[PGNUM];
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  for(int i = 0; i < PGNUM; ++i)
    kmem.refcnt[i] = 0;
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{  
  acquire(&kmem.lock);
  refcntSub((uint64)pa);
  if(refcntQuery((uint64)pa) != 0) {
    release(&kmem.lock);
    return;
  }
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    refcntInit((uint64)r);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

int refcntAdd(uint64 pa) {
  if(pa < (uint64)end || pa > PHYSTOP) {
    printf("refcntAdd error: address out of range");
    return -1;
  }
  int k = (pa - (uint64)end) / PGSIZE;
  acquire(&kmem.lock);
  ++kmem.refcnt[k];
  release(&kmem.lock);
  return 0;
}

int refcntSub(uint64 pa) {
  if(pa < (uint64)end || pa > PHYSTOP) {
    printf("refcntSub error: address out of range");
    return -1;
  }
  int k = (pa - (uint64)end) / PGSIZE;
  if(kmem.refcnt[k] > 0)
    --kmem.refcnt[k];
  return 0;
}

int refcntInit(uint64 pa) {
  if(pa < (uint64)end || pa > PHYSTOP) {
    printf("refcntInit error: address out of range");
    return -1;
  }
  int k = (pa - (uint64)end) / PGSIZE;
  kmem.refcnt[k] = 1;
  return 0;
}

int refcntQuery(uint64 pa) {
  if(pa < (uint64)end || pa > PHYSTOP) {
    printf("refcntQuery error: address out of range");
    return -1;
  }
  int k = (pa - (uint64)end) / PGSIZE;
  return kmem.refcnt[k];
}