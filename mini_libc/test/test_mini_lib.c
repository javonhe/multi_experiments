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

    // 测试sbrk
    void *p = sbrk(1024);
    printf("sbrk p: 0x%lx\n", (long)p);

    // 测试mmap
    void *mmap_p = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mmap_p == NULL)
    {
        printf("mmap failed\n");
    }
    else
    {
        printf("mmap p: 0x%lx\n", (long)mmap_p);
    }

    while(1);

    return 0;
}