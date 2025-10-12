#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//
// 使用管道递归地进行素数筛选
//
void sieve(int p_left[2]) // 参数p_left是左邻居（父进程）传来的管道
{
    // 每个sieve函数都只负责处理一个素数，剩下的交给子进程
    // 关闭此管道的写端，因为我们只从左邻居读取
    close(p_left[1]);

    int first_num;
    read(p_left[0], &first_num, sizeof(first_num));
    // 从管道中读取第一个数。如果管道已空（或上游关闭），read会返回0或-1
    if ( first_num == -1) {
        // 没有更多的数字了，直接退出
        // 关闭最后的读端并退出。
        close(p_left[0]);
        exit(0);
    }
    
    // 读到的第一个数必然是素数，打印它
    printf("prime %d\n", first_num);

    // 创建一个通往右邻居（子进程）的新管道
    int p_right[2];
    pipe(p_right);

    // 创建子进程作为流水线的下一个环节
    if (fork() == 0) {
        // 子进程逻辑
        // 子进程不需要与它的“上上游”通信，所以关闭旧管道的读端
        //close(p_right[1]);递归函数sieve里第一行有了
        close(p_left[0]);//这个需要吗？？我也不清楚子进程会不会有p_left
        // 当 fork() 执行时，子进程会获得父进程文件描述符表的一个副本。因为在 fork() 的那一刻，父进程中的 p_left[1] 描述符已经是关闭状态，所以子进程继承过来的 p_left[1] 自然也是关闭的。
        // 递归调用，子进程把自己变成一个新的筛子
        // 它将从p_right管道中读取数据
        sieve(p_right);
    } else {
        // 父进程逻辑
        // 父进程不需要从新管道中读，所以关闭其读端
        close(p_right[0]);

        int buf;
        // 继续从左邻居读取剩下的数
        while (read(p_left[0], &buf, sizeof(buf)) && buf != -1) {
            // 筛选核心：如果这个数不是当前素数(first_num)的倍数
            if (buf % first_num != 0) {
                // 就把它写入新管道，传递给右邻居（子进程）
                write(p_right[1], &buf, sizeof(buf));
            }
        }
        buf = -1;
        // 向右邻居发送EOF信号，表示没有更多的数字了
        write(p_right[1], &buf, sizeof(buf));
        //父进程关闭所有剩下的管道描述符
        close(p_left[0]);
        close(p_right[1]);

        // 等待子进程结束
        wait(0);
        exit(0);
    }
}

//
// 主函数，程序的入口
//
int main(int argc, char *argv[])
{
    // 创建第一个管道
    int p[2];
    pipe(p);

    if (fork() == 0) {
        // 子进程：成为第一个筛子
        //递归函数里关掉了close(p[1]); // 关闭写端，因为子进程只读
        sieve(p);
        exit(0);
    } else {
        // 父进程（main）：作为数字的源头
        // 关闭第一个管道的读端，因为main只负责写
        close(p[0]);
        int  i;
        // 将数字 2 到 35 写入管道
        for (i = 2; i <= 35; i++) {
            if (write(p[1], &i, sizeof(i)) != sizeof(i)) {
                fprintf(2, "write error\n");
                exit(1);
            }
        }
        
        // // 写完后，关闭写端。这会向上面的子进程发出一个EOF信号
        // close(p[1]);这个在递归函数里有
        i= -1;
        write(p[1], &i, sizeof(i)); // 发送一个特殊的结束标志

        close(p[1]);

        
    }
    // 等待第一个子进程完成所有工作
        wait(0);
        exit(0);
}