#include "mini_lib.h"

/**
 * Fork功能测试
 */
static void test_fork(void)
{
    pid_t pid = fork();
    if (pid < 0) 
    {
        printf("fork failed\n");
        return;
    }
    
    if (pid == 0) 
    {
        // 子进程
        for (int i = 0; i < 100000000; i++); // 子进程忙等待，模拟延时
        printf("fork child, PID: %d\n", getpid());
    } 
    else 
    {
        // 父进程
        printf("fork parent, PID: %d, child PID: %d\n", getpid(), pid);
    }
}

int main(int argc, char *argv[])
{
    // 文件操作测试
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

    // 测试malloc和free
    char *str = (char *)malloc(256);
    if (str != NULL) 
    {
        printf("malloc success, addr: 0x%lx\n", (long)str);
        
        memset(str, 'A', 255);
        str[255] = '\0';
        printf("after memset, str: %s\n", str);
        
        free(str);
        printf("free success\n");
    } 
    else 
    {
        printf("malloc failed\n");
    }

    // 测试mmap
    void *mmap_p = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mmap_p == MAP_FAILED || mmap_p == NULL)
    {
        printf("mmap failed\n");
    }
    else
    {
        printf("mmap p: 0x%lx\n", (long)mmap_p);
    }

    // 测试fork功能
    test_fork();

    while(1);

    return 0;
}