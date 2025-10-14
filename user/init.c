// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    mknod("statistics", STATS, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    printf("init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }

    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      wpid = wait((int *) 0);
      if(wpid == pid){
        // the shell exited; restart it.
        break;
      } else if(wpid < 0){
        printf("init: wait returned an error\n");
        exit(1);
      } else {
        // it was a parentless process; do nothing.
      }
    }
  }
}
/*页面内容分析
Page 0 (虚拟地址 0x0000-0x0FFF):
内容: .text 段 (0x0000-0x0922) + .rodata 段 (0x0928-0x09e4)
具体内容:
init 程序的机器代码（main函数等）
字符串常量（如 "sh", "console" 等）
权限: 0x1f = 可读、可写、可执行
Page 1 (虚拟地址 0x1000-0x1FFF):
内容: .data 段 (0x09e8-0x09f8) + .sbss 段 (0x09f8-0x0a00)
具体内容:
已初始化的全局变量（如 char *argv[] = { "sh", 0 };）
小BSS段（未初始化的静态变量）
权限: 0x0f = 可读、可写，但不可执行
Page 2 (虚拟地址 0x2000-0x2FFF):
内容: .bss 段 (0x0a00-0x0a10)
具体内容: 未初始化的全局变量
权限: 0x1f = 可读、可写、可执行
*/