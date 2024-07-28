#include "mini_lib.h"


int close(int fd)
{
    int result;
    asm volatile(
        "mov x8, #57\n"                 // syscall number for close
        "mov x0, %1\n"                  // fd (file descriptor)
        "svc #0\n"                      // Call the syscall
        "mov %0, x0\n"                  // Store the result in 'result'
        : "=r"(result)                  // 输出操作数列表
        : "r"(fd)                       // 输入操作数列表,对应占位符顺序
        : "x0", "x8"                    // 告诉编译器在这段汇编代码中会被修改的寄存器, 顺序可以改变
    );

    return result;
}