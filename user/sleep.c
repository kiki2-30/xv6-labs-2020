#include "kernel/types.h"  // 包含 XV6 的基本类型定义，如 uint, int 等
#include "kernel/stat.h"   // 包含文件状态等定义 (虽然此程序没用到，但包含进来是好习惯)
#include "user/user.h"     // 核心！包含了所有用户程序能用的函数声明 (printf, sleep, atoi, exit等)

int main(int argc , char**argv) {
  if (argc != 2) {
    printf("Usage: sleep <ticks>\n");//具体实现见printf.c
    exit(0);
  }

  int ticks = atoi(argv[1]);
  if (ticks <= 0) {
    printf("Error: ticks must be a positive integer\n");
    exit(0);
  }

  sleep(ticks);
  exit(0);
}

