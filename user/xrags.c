#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 执行程序函数
void run_program(char *program, char *args_array[]) {
    if(fork() == 0) {
        // 子进程：变成目标程序
        exec(program, args_array);
        exit(0);  // 只有exec失败才会到这里
    }
    // 父进程直接返回
}

int main(int argc, char *argv[]) {
    // 内存池：存放从stdin读取的所有参数字符串
    char memory_pool[2048]; 
    int memory_index = 0;  // 当前写入位置
    
    // 参数数组：存放所有参数的"地址"
    char *all_arguments[128];
    int args_count = 0;
    
    // 1. 先把命令行参数加入到参数数组中
    for(int i = 1; i < argc; i++) {
        all_arguments[args_count] = argv[i];
        args_count++;
    }
    
    int start_args_count = args_count;  // 记录命令行参数的数量
    
    // 当前正在读取的参数在memory_pool中的开始位置
    int current_arg_start = 0;
    
    // 2. 从标准输入读取数据
    char current_char;
    while(read(0, &current_char, 1) > 0) {
        if(current_char == ' ' || current_char == '\n') {
            // 一个参数结束
            memory_pool[memory_index] = '\0';  // 结束当前字符串
            
            // 把这个参数加入到参数数组中
            all_arguments[args_count] = &memory_pool[current_arg_start];
            args_count++;
            
            // 准备下一个参数
            current_arg_start = memory_index + 1;
            
            if(current_char == '\n') {
                // 一行结束，执行命令
                all_arguments[args_count] = 0;  // 参数数组结束标记
                run_program(argv[1], all_arguments);
                
                // 重置参数数组（但保留前面的命令行参数）
                args_count = start_args_count;
            }
        } else {
            // 普通字符，存入内存池
            memory_pool[memory_index] = current_char;
        }
        
        memory_index++;
    }
    
    // 3. 处理最后一行（如果没有换行符结尾）
    if(args_count > start_args_count) {
        memory_pool[memory_index] = '\0';
        all_arguments[args_count] = &memory_pool[current_arg_start];
        args_count++;
        all_arguments[args_count] = 0;
        run_program(argv[1], all_arguments);
    }
    
    // 4. 等待所有子进程结束
    while(wait(0) != -1) {
        // 空循环，等待所有子进程
    }
    
    exit(0);
}