#include "mini_lib.h"


int main(int argc, char *argv[])
{
    int fd = open(argv[1], O_CREAT | O_APPEND | O_RDWR, 0644);

    printf("open file %s, fd = %d\n", argv[1], fd);

    if (fd > 0)
    {
        write(fd, "hello world", 12);

        // 测试lseek，打印文件长度
        int len = lseek(fd, 0, SEEK_END);
        printf("file length: %d\n", len);

        close(fd);
    }

    return 0;
}