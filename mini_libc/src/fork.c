#include "mini_lib.h"  // 包含公共头文件

/**
 * fork.c - aarch64平台fork系统调用实现
 * 
 * 本文件实现了fork系统调用的包装函数。在Linux中，
 * fork实际上是通过clone系统调用实现的，需要传递
 * 正确的标志位来模拟fork的行为。
 */

// 手动定义系统调用号
#define __NR_clone 220  // aarch64平台的clone系统调用号
#define __NR_getpid 172  // aarch64平台的getpid系统调用号

/* clone标志位 */
#define SIGCHLD     17   // 子进程结束时向父进程发送SIGCHLD信号
#define CLONE_CHILD_CLEARTID     0x00200000  // 子进程退出时清除TID
#define CLONE_CHILD_SETTID       0x01000000  // 设置子进程的TID
#define CLONE_PARENT_SETTID      0x00100000  // 设置父进程的TID
#define CLONE_VM                 0x00000100  // 共享内存空间
#define CLONE_FS                 0x00000200  // 共享文件系统信息
#define CLONE_FILES              0x00000400  // 共享文件描述符表
#define CLONE_SIGHAND            0x00000800  // 共享信号处理函数
#define CLONE_THREAD             0x00010000  // 同一线程组

/* fork使用的标志位组合 */
#define FORK_FLAGS (SIGCHLD)

/**
 * 创建一个新的进程
 * 
 * 通过clone系统调用实现fork功能。
 * 
 * clone系统调用参数说明：
 * x0 = flags (标志位)
 * x1 = stack (新进程的栈顶，fork时为0)
 * x2 = parent_tid (父进程的TID指针，fork时为0)
 * x3 = child_tid (子进程的TID指针，fork时为0)
 * x4 = tls (线程本地存储，fork时为0)
 * 
 * @return 在父进程中返回子进程ID，在子进程中返回0，失败返回-1
 */
int fork(void)
{
    register long x8 asm("x8") = __NR_clone;
    register long x0 asm("x0") = FORK_FLAGS;
    register long x1 asm("x1") = 0;  // stack
    register long x2 asm("x2") = 0;  // parent_tid
    register long x3 asm("x3") = 0;  // child_tid
    register long x4 asm("x4") = 0;  // tls
    
    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
        : "memory", "cc"
    );
    
    if (x0 < 0)
    {
        return -1;
    }
    
    return x0;
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