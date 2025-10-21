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
  struct spinlock lock;      // 每个CPU独立的锁
  struct run *freelist;      // 每个CPU独立的空闲链表
} kmem[NCPU];               // 注意：改为数组，大小为CPU数量

void
kinit()
{
  char lockname[8];
  for(int i = 0; i < NCPU; i++) {
    snprintf(lockname, sizeof(lockname), "kmem%d", i);
    initlock(&kmem[i].lock, lockname);
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

  push_off();                    // 关闭中断
  int c = cpuid();               // 获取当前CPU编号
  pop_off();                     // 恢复中断

  acquire(&kmem[c].lock);
  r->next = kmem[c].freelist;
  kmem[c].freelist = r;
  release(&kmem[c].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();                    // 关闭中断
  int c = cpuid();               // 获取当前CPU编号
  pop_off();                     // 恢复中断

  acquire(&kmem[c].lock);
  r = kmem[c].freelist;
  if(r)
    kmem[c].freelist = r->next;
  release(&kmem[c].lock);

  if(r == 0) {                   // 如果当前CPU没有空闲内存
    // 尝试从其他CPU窃取
    for(int i = 0; i < NCPU; i++) {
      if(i == c)                 // 跳过当前CPU（已经检查过了）
        continue;
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r)
        kmem[i].freelist = r->next;
      release(&kmem[i].lock);
      if(r)                      // 如果窃取成功，跳出循环
        break;
    }
  } 

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
