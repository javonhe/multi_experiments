#include "mini_lib.h"

int open(const char *pathname, int flags, int mode)
{
    int result;
    int dirfd = AT_FDCWD;
    asm volatile(
        "mov x8, #56\n"                 // syscall number for openat
        "mov x0, %1\n"                  // dirfd
        "mov x1, %2\n"                  // file path
        "mov x2, %3\n"                  // flags
        "mov x3, %4\n"                  // mode
        "svc #0\n"                      // Call the syscall
        "mov %0, x0\n"                  // Store the result in 'result'
        : "=r"(result)                  // 输出操作数列表
        : "r"(dirfd), "r"(pathname), "r"(flags) , "r"(mode)// 输入操作数列表,对应占位符顺序
        : "x0", "x1", "x2", "x3", "x8"        // 告诉编译器在这段汇编代码中会被修改的寄存器, 顺序可以改变
    );

    return result;
}