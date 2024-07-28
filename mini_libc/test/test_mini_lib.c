#include "mini_lib.h"


int main(int argc, char *argv[])
{

    // 测试strlen，打印参数
    write(1, argv[1], strlen(argv[1]));
    write(1, "\n", 1);
    
    int fd = open(argv[1], O_CREAT | O_APPEND | O_RDWR, 0644);

    // 测试itoa，打印文件描述符
    char fd_str[4] = {0};
    itoa(fd, fd_str, 10);
    write(1, fd_str, strlen(fd_str));
    write(1, "\n", 1);

    if (fd > 0)
    {
        write(fd, "hello world", 12);
        close(fd);
    }

    return 0;
}