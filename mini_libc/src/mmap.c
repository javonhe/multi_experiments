#include "mini_lib.h"


static inline void *__mmap(void *addr, long size, int prot, int flags, int fd, long offset)
{
    void *result;
    asm volatile(
        "mov x8, #222\n"                // syscall number for mmap2
        "mov x0, %1\n"                  // addr
        "mov x1, %2\n"                  // size
        "mov x2, %3\n"                  // prot
        "mov x3, %4\n"                  // flags
        "mov x4, %5\n"                  // fd
        "mov x5, %6\n"                  // offset
        "svc #0\n"                      // Call the syscall
        "mov %0, x0\n"                  // Store the result in 'result'
        : "=r"(result)                  // 输出操作数列表
        : "r"(addr), "r"(size), "r"(prot), "r"(flags), "r"(fd), "r"(offset)         // 输入操作数列表,对应占位符顺序
        : "x0", "x1", "x2", "x3", "x4", "x5", "x8"                    // 告诉编译器在这段汇编代码中会被修改的寄存器, 顺序可以改变
    );

    return result;
}


void *mmap(void *addr, long size, int prot, int flags, int fd, long offset)
{
    if (offset < 0)
    {
        return NULL;
    }

    // 4k 对齐
    long rounded = __MINI_ALIGN(size, 4096);
    if (rounded < size)
    {
        return NULL;
    }

    // mmap申请返回的内存大小是4k的整数倍
    return __mmap(addr, size, prot, flags, fd, offset);
}


int munmap(void *addr, long size)
{
    int result;

    asm volatile(
        "mov x8, #215\n"                    // syscall number for munmap
        "mov x0, %1\n"                      // addr
        "mov x1, %2\n"                      // size
        "svc #0\n"                          // Call the syscall
        "mov %0, x0\n"                      // Store the result in 'result'
        : "=r"(result)                      // 输出操作数列表
        : "r"(addr), "r"(size)              // 输入操作数列表,对应占位符顺序
        : "x0", "x1", "x8"                  // 告诉编译器在这段汇编代码中会被修改的寄存器, 顺序可以改变
    );

    return result;
}