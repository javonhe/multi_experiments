#include "mini_lib.h"


int lseek(int fd, int offset, int whence)
{
    int result;
    asm volatile(
        "mov x8, #62\n"                 // syscall number for llseek
        "mov x0, %1\n"                  // fd
        "mov x1, %2\n"                  // offset
        "mov x2, %3\n"                  // whence
        "svc #0\n"                      // Call the syscall
        "mov %0, x0\n"                  // Store the result in 'result'
        : "=r"(result)                  // 输出操作数列表
        : "r"(fd), "r"(offset), "r"(whence)         // 输入操作数列表,对应占位符顺序
        : "x0", "x1", "x2", "x8"                    // 告诉编译器在这段汇编代码中会被修改的寄存器, 顺序可以改变
    );

    return result;
}