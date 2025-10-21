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
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;要修改成13个链表
  // 哈希表：13 个桶，每个桶有自己的锁和链表
  struct {
    struct spinlock lock;
    struct buf head;
  } bucket[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  char name[16];

    // 初始化每个桶的锁和链表
    for(int i = 0; i < NBUCKET; i++) {
      snprintf(name, sizeof(name), "bcache.bucket%d", i);  // ← 注意这里
      initlock(&bcache.bucket[i].lock, name);
      
      // 初始化这个桶的链表为空
      bcache.bucket[i].head.prev = &bcache.bucket[i].head;
      bcache.bucket[i].head.next = &bcache.bucket[i].head;
    }


    for(b = bcache.buf; b < bcache.buf+NBUF; b++){
      int i = (b - bcache.buf) % NBUCKET;  // 计算应该放到哪个桶
      b->next = bcache.bucket[i].head.next;
      b->prev = &bcache.bucket[i].head;
      initsleeplock(&b->lock, "buffer");
      bcache.bucket[i].head.next->prev = b;
      bcache.bucket[i].head.next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int hash = blockno % NBUCKET;  // 计算哈希值


  acquire(&bcache.bucket[hash].lock);

  // Is the block already cached?
  for(b = bcache.bucket[hash].head.next; b != &bcache.bucket[hash].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->timestamp = ticks;  // 更新时间戳
      release(&bcache.bucket[hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // 先在当前桶中找一个空闲的 buffer
  struct buf *victim = 0;
  uint min_time = 0xffffffff;  // 最大值
  
  for(b = bcache.bucket[hash].head.next; b != &bcache.bucket[hash].head; b = b->next){
    if(b->refcnt == 0 && b->timestamp < min_time) {
      victim = b;
      min_time = b->timestamp;
    }
  }
  
  // 如果当前桶找到了空闲buffer
  if(victim) {
    victim->dev = dev;
    victim->blockno = blockno;
    victim->valid = 0;
    victim->refcnt = 1;
    victim->timestamp = ticks;
    release(&bcache.bucket[hash].lock);
    acquiresleep(&victim->lock);
    return victim;
  }
  
  // 当前桶没有空闲buffer，需要从其他桶窃取
  // 先释放当前桶的锁
  release(&bcache.bucket[hash].lock);
  
  // 遍历所有桶，找到最旧的空闲buffer
  min_time = 0xffffffff;
  int victim_bucket = -1;
  
  for(int i = 0; i < NBUCKET; i++) {
    acquire(&bcache.bucket[i].lock);
    for(b = bcache.bucket[i].head.next; b != &bcache.bucket[i].head; b = b->next){
      if(b->refcnt == 0 && b->timestamp < min_time) {
        victim = b;
        min_time = b->timestamp;
        victim_bucket = i;
      }
    }
    release(&bcache.bucket[i].lock);
  }
  
  if(victim_bucket == -1) {
    panic("bget: no buffers");
  }
  
  // 获取两个桶的锁（按顺序避免死锁）
  if(hash < victim_bucket) {
    acquire(&bcache.bucket[hash].lock);
    acquire(&bcache.bucket[victim_bucket].lock);
  } else if(hash > victim_bucket) {
    acquire(&bcache.bucket[victim_bucket].lock);
    acquire(&bcache.bucket[hash].lock);
  } else {
    // 同一个桶，只获取一次
    acquire(&bcache.bucket[hash].lock);
  }
  
  // 从原来的桶移除
  victim->next->prev = victim->prev;
  victim->prev->next = victim->next;
  
  // 插入到目标桶
  victim->next = bcache.bucket[hash].head.next;
  victim->prev = &bcache.bucket[hash].head;
  bcache.bucket[hash].head.next->prev = victim;
  bcache.bucket[hash].head.next = victim;
  
  // 更新buffer信息
  victim->dev = dev;
  victim->blockno = blockno;
  victim->valid = 0;
  victim->refcnt = 1;
  victim->timestamp = ticks;
  
  // 释放锁
  if(hash != victim_bucket) {
    release(&bcache.bucket[victim_bucket].lock);
  }
  release(&bcache.bucket[hash].lock);
  
  acquiresleep(&victim->lock);
  return victim;
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

  int hash = b->blockno % NBUCKET;  // 计算这个buf在哪个桶

  acquire(&bcache.bucket[hash].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // 更新时间戳，不需要移动链表
    b->timestamp = ticks;
  }
  
  release(&bcache.bucket[hash].lock);
}

void
bpin(struct buf *b) {
  int hash = b->blockno % NBUCKET;  // 计算这个buf在哪个桶
  acquire(&bcache.bucket[hash].lock);
  b->refcnt++;
  release(&bcache.bucket[hash].lock);
}

void
bunpin(struct buf *b) {
  int hash = b->blockno % NBUCKET;  // 计算这个buf在哪个桶
  acquire(&bcache.bucket[hash].lock);
  b->refcnt--;
  release(&bcache.bucket[hash].lock);
}


