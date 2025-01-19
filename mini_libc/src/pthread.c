/**
 * pthread.c - aarch64平台pthread实现
 * 
 * 本文件实现了基本的pthread线程创建功能。
 * 基于Linux的clone系统调用实现，使用适当的标志位
 * 来创建共享地址空间的线程。
 */

#include "mini_lib.h"

/* 系统调用号定义 */
#define __NR_exit     93
#define __NR_futex    98   // futex系统调用号

/* futex操作 */
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

/* pthread创建时使用的标志位组合 */
#define PTHREAD_FLAGS (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | \
                      CLONE_THREAD | CLONE_SYSVSEM | \
                      CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID)

/* 线程栈大小 (64KB) */
#define STACK_SIZE (64 * 1024)  // 64KB

/* 线程启动参数结构 */
struct thread_start_args 
{
    void *(*start_routine)(void*);  // 线程入口函数
    void *arg;                      // 线程参数
    void *stack;                    // 线程栈
    int has_exited;                // 线程是否已退出
    int tid;                      // 线程ID
    void *return_value;           // 添加返回值字段

};

/**
 * futex系统调用包装函数
 */
static int futex(int *uaddr, int futex_op, int val,
                const struct timespec *timeout, int *uaddr2, int val3)
{
    register long x8 asm("x8") = __NR_futex;
    register long x0 asm("x0") = (long)uaddr;
    register long x1 asm("x1") = futex_op;
    register long x2 asm("x2") = val;
    register long x3 asm("x3") = (long)timeout;
    register long x4 asm("x4") = (long)uaddr2;
    register long x5 asm("x5") = val3;
    
    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory", "cc"
    );
    
    return x0;
}

/**
 * 线程启动包装函数
 * 负责调用实际的线程函数并清理资源
 */
static int pthread_start(struct thread_start_args *start_args) 
{
    // 调用实际的线程函数
    start_args->return_value = start_args->start_routine(start_args->arg);
    
    // 设置退出标志
    start_args->has_exited = 1;
    // 唤醒等待的线程
    futex(&start_args->has_exited, FUTEX_WAKE, 1, NULL, NULL, 0);    
}


/**
 * 创建新线程
 * @param thread: 线程标识符
 * @param attr: 线程属性（本实现忽略）
 * @param start_routine: 线程入口函数
 * @param arg: 传递给线程函数的参数
 * @return: 成功返回0，失败返回错误码
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                  void *(*start_routine)(void*), void *arg)
{
    // 分配线程栈
    void *stack = mmap(NULL, STACK_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,  // 添加 MAP_STACK 标志
                      -1, 0);
                      
    if (stack == MAP_FAILED)
    {
        return -1;
    }

    // 分配并初始化启动参数结构
    struct thread_start_args *start_args = malloc(sizeof(struct thread_start_args));
    if (start_args == NULL)
    {
        munmap(stack, STACK_SIZE);
        return -1;
    }

    // 初始化启动参数
    start_args->start_routine = start_routine;
    start_args->arg = arg;
    start_args->stack = stack;
    start_args->has_exited = 0;

    // 栈顶需要16字节对齐
    void *stack_top = (void*)(((unsigned long)stack + STACK_SIZE) & ~15UL);
    
    int ret = clone(
        (int(*)(void*))pthread_start,
        stack_top,
        PTHREAD_FLAGS,
        start_args,
        &start_args->tid,
        NULL,
        &start_args->tid
    );
    
    if (ret < 0)
    {
        printf("clone failed\n");
        free(start_args);
        munmap(stack, STACK_SIZE);
        return -1;
    }
    
    *thread = (pthread_t)start_args;
    return 0;
}

/**
 * 等待线程结束
 * @param thread: 要等待的线程标识符
 * @param retval: 用于存储线程返回值的指针
 * @return: 成功返回0，失败返回错误码
 */
int pthread_join(pthread_t thread, void **retval)
{
    struct thread_start_args *start_args = (struct thread_start_args*)thread;
    
    if (start_args == NULL)
    {
        printf("pthread_join: thread is NULL\n");
        return -1;
    }

    // 等待线程退出
    while (!start_args->has_exited)
    {
        futex(&start_args->has_exited, FUTEX_WAIT, 0, NULL, NULL, 0);
    }

    if (retval != NULL)
    {
        *retval = start_args->return_value;
    }

    // 清理资源
    void *stack = start_args->stack;
    free(start_args);
    munmap(stack, STACK_SIZE);

    return 0;
}
