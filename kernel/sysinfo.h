struct sysinfo {
  uint64 freemem;   // amount of free memory (bytes)
  uint64 nproc;     // number of process
  //freemem字段应该设置为空闲内存的字节数，nproc字段应该设置为state字段不为UNUSED的进程数。
};
