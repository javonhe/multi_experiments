/**
 * @brief 写入数据到文件描述符
 *
 * 使用 ARM64 汇编代码调用 write 系统调用来将数据写入到指定的文件描述符中。
 *
 * @param fd 文件描述符
 * @param buf 数据缓冲区指针
 * @param count 要写入的字节数
 *
 * @return 写入成功返回实际写入的字节数，写入失败返回 -1
 */
int svc_write(int fd, const char *buf, int count)
{
    int result;
    // ARM64 assembly code to invoke the openat syscall
    asm volatile(
        "mov x8, #0x40\n"       // syscall number for write (0x40 on ARM64) 
        "mov x0, %1\n"          // fd (file descriptor)
        "mov x1, %2\n"          // buf (pointer to the string)
        "mov x2, %3\n"          // count
        "svc #0\n"              // Call the syscall
        "mov %0, x0\n"          // Store the result in 'result'
        : "=r" (result)         // 输出操作数列表
        : "r" (fd), "r" (buf), "r" (count)//输入操作数列表,对应占位符顺序
        : "x0", "x1", "x2", "x8"    // 告诉编译器在这段汇编代码中会被修改的寄存器, 顺序可以改变
    );

    return result;
}


int main(int argc, char *argv[])
{
    char c_argc[3] = {argc + 48, '\n', 0}; // 将argc转换为ascii字符

	svc_write(1, argv[1], 6);
    svc_write(1, "\n", 2);
	svc_write(1, c_argc, 3);

    return 0;
}