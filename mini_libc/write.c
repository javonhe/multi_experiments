int write(int fd, const void *buf, int count)
{
    int result;
    asm volatile(
        "mov x8, #0x40\n"               // syscall number for write (0x40 on ARM64)
        "mov x0, %1\n"                  // fd (file descriptor)
        "mov x1, %2\n"                  // buf (pointer to the string)
        "mov x2, %3\n"                  // count
        "svc #0\n"                      // Call the syscall
        "mov %0, x0\n"                  // Store the result in 'result'
        : "=r"(result)                  // 输出操作数列表
        : "r"(fd), "r"(buf), "r"(count) // 输入操作数列表,对应占位符顺序
        : "x0", "x1", "x2", "x8"        // 告诉编译器在这段汇编代码中会被修改的寄存器, 顺序可以改变
    );

    return result;
}