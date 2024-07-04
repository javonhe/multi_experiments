#include <syscall.h>


int svc_write(int fd, const char *buf, int count)
{
    int result;
    // ARM64 assembly code to invoke the openat syscall
    asm volatile(
        "mov x8, %1\n"          // syscall number for write (0x40 on ARM64) 
        "mov x0, %2\n"          // fd (file descriptor)
        "mov x1, %3\n"          // buf (pointer to the string)
        "mov x2, %4\n"          // count
        "svc #0\n"              // Call the syscall
        "mov %0, x0\n"          // Store the result in 'result'
        : "=r" (result)         // 输出操作数列表
        : "i" (__NR_write), "r" (fd), "r" (buf), "r" (count)//输入操作数列表,对应占位符顺序
        : "x0", "x1", "x2", "x8"    // 告诉编译器在这段汇编代码中会被修改的寄存器, 顺序可以改变
    );

    return result;
}


int main()
{
    const char *buf = "hello\n";

	int result = svc_write(1, buf, 6);

    while(1);

    return 0;
}