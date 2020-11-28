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
} kmems[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++) {
    initlock(&kmems[i].lock, "kmem");
    kmems[i].freelist = 0;
  }
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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int this_cpu = cpuid();
  acquire(&kmems[this_cpu].lock);
  r->next = kmems[this_cpu].freelist;
  kmems[this_cpu].freelist = r;
  release(&kmems[this_cpu].lock);
  pop_off();
}

struct run *
kalloc_on(int cpu)
{
  struct run *r = 0;
  acquire(&kmems[cpu].lock);
  r = kmems[cpu].freelist;
  if(r)
    kmems[cpu].freelist = r->next;
  release(&kmems[cpu].lock);
  return r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int this_cpu = cpuid();
  r = kalloc_on(this_cpu);
  // steal from other cpu
  for(int other_cpu = 0; !r && other_cpu < NCPU; other_cpu++) {
    if(other_cpu == this_cpu)
      continue;
    r = kalloc_on(other_cpu);
  }
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
