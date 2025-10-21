struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uint timestamp;      // ← 新增这一行
  uchar data[BSIZE];
};

//buffer cache的LRU算法
//当一个缓冲区被使用时，它的timestamp被更新为当前时间
//当一个缓冲区被替换时，它的timestamp最小的缓冲区被替换
//当一个缓冲区被替换时，它的timestamp被更新为当前时间
//当一个缓冲区被替换时，它的timestamp被更新为当前时间