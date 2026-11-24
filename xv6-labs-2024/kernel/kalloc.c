// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

#define NPAGE ((PHYSTOP - KERNBASE) / PGSIZE)

struct {
  struct spinlock lock;
  int count[NPAGE];
} ref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref");

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;

  p = (char*)PGROUNDUP((uint64)pa_start);

  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    ref.count[((uint64)p - KERNBASE) / PGSIZE] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)

void
kfree(void *pa)
{
  struct run *r;
  int index;

  if(((uint64)pa % PGSIZE) != 0 ||
     (char*)pa < end ||
     (uint64)pa >= PHYSTOP)
    panic("kfree");

  index = ((uint64)pa - KERNBASE) / PGSIZE;

  acquire(&ref.lock);

  if(ref.count[index] < 1)
    panic("kfree ref");

  ref.count[index]--;

  if(ref.count[index] > 0) {
    release(&ref.lock);
    return;
  }

  release(&ref.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
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
  int index;

  acquire(&kmem.lock);

  r = kmem.freelist;

  if(r)
    kmem.freelist = r->next;

  release(&kmem.lock);

  if(r) {
    index = ((uint64)r - KERNBASE) / PGSIZE;

    acquire(&ref.lock);
    ref.count[index] = 1;
    release(&ref.lock);

    memset((char*)r, 5, PGSIZE);
  }

  return (void*)r;
}


void
krefinc(uint64 pa)
{
  int index;

  if(pa % PGSIZE != 0 ||
     pa < KERNBASE ||
     pa >= PHYSTOP)
    panic("krefinc");

  index = (pa - KERNBASE) / PGSIZE;

  acquire(&ref.lock);

  if(ref.count[index] < 1)
    panic("krefinc ref");

  ref.count[index]++;

  release(&ref.lock);
}
