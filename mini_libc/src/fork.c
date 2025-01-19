#include "mini_lib.h"  // 包含公共头文件

/**
 * fork.c - aarch64平台fork系统调用实现
 * 
 * 本文件实现了fork系统调用的包装函数。在Linux中，
 * fork实际上是通过clone系统调用实现的，需要传递
 * 正确的标志位来模拟fork的行为。
 */

// 手动定义系统调用号
#define __NR_getpid 172  // aarch64平台的getpid系统调用号

#define SIGCHLD     17   // 子进程结束时向父进程发送SIGCHLD信号

/* fork使用的标志位组合 */
#define FORK_FLAGS (SIGCHLD)

/**
 * 创建一个新的进程
 * 
 * 通过clone系统调用实现fork功能。
 * 
 * fork()会创建一个新的进程,新进程是调用进程的一个副本。
 * 新进程(子进程)会复制父进程的数据段、堆和栈。
 * 子进程会继承父进程打开的文件描述符。
 * 
 * 子进程与父进程的区别:
 * - 子进程有自己的进程ID
 * - 子进程的父进程ID是创建它的进程的ID
 * - 子进程的资源利用率和CPU时间会被设置为0
 * - 子进程不会继承父进程的内存锁
 * - 子进程不会继承父进程的异步I/O操作
 * 
 * @return: 在父进程中返回子进程ID
 *          在子进程中返回0
 *          失败时返回-1
 */
int fork(void)
{
    // 调用clone实现fork
    // 对于fork，我们传递NULL作为栈和其他参数
    //int ret = clone(FORK_FLAGS, NULL, NULL, NULL, NULL, NULL, NULL);
    int ret = clone(NULL, NULL, FORK_FLAGS, NULL);
    if (ret < 0)
    {
        return -1;
    }
    
    return ret;
}

/**
 * 获取当前进程的PID
 * @return 当前进程的PID
 */
pid_t getpid(void)
{
    register long x8 asm("x8") = __NR_getpid;
    register long x0 asm("x0") = 0;  // 无参数
    
    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8)
        : "memory", "cc"
    );
    
    return x0;
}