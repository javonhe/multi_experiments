#include "mini_lib.h"

/**
 * read - 从文件描述符读取数据
 * 
 * 本函数是对Linux系统调用read的封装，使用内联汇编直接调用系统调用。
 * 在aarch64架构下，系统调用号为63。
 * 
 * @param fd: 文件描述符
 * @param buf: 接收数据的缓冲区
 * @param count: 要读取的字节数
 * @return: 成功返回读取的字节数，失败返回-1
 */
ssize_t read(int fd, void *buf, size_t count)
{
    register long x8 asm("x8") = 63;  // read系统调用号
    register long x0 asm("x0") = fd;
    register long x1 asm("x1") = (long)buf;
    register long x2 asm("x2") = count;
    
    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory", "cc"
    );
    
    // 检查系统调用返回值
    if (x0 < 0) {
        // 设置错误码并返回-1
        return -1;
    }
    
    return x0;
}

