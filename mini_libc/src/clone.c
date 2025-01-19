/**
 * @file clone.c
 * @brief clone系统调用的实现
 * 
 * 本文件实现了Linux系统下的clone系统调用封装，用于创建新的进程或线程。
 * 该实现基于aarch64架构，通过汇编代码直接调用系统调用。
 * 
 * 主要功能：
 * - 支持创建新进程/线程
 * - 支持设置新进程/线程的入口函数
 * - 支持设置新进程/线程的栈空间
 * - 支持设置各种clone标志位
 * - 支持传递参数给入口函数
 * 
 * @note 本实现不依赖任何标准库，直接使用系统调用实现功能
 */

#include "mini_lib.h"

#define __NR_clone    220

/**
 * clone系统调用实现
 * 
 * @param fn: 子进程/线程入口函数（可以为NULL）
 * @param stack: 新栈指针
 * @param flags: clone标志位
 * @param arg: 传递给入口函数的参数
 * @param ...: 可变参数（parent_tid, tls, child_tid）
 * @return: 成功时返回子进程/线程ID，失败时返回-1
 */
int clone(int (*fn)(void *), void *stack, int flags, void *arg, ...)
{
    // 处理可变参数
    va_list ap;
    va_start(ap, arg);
    int *parent_tid = va_arg(ap, int*);
    void *tls = va_arg(ap, void*);
    int *child_tid = va_arg(ap, int*);
    va_end(ap);

    register long x8 asm("x8") = __NR_clone;
    register long x0 asm("x0") = flags;
    register long x1 asm("x1") = (long)stack;
    register long x2 asm("x2") = (long)parent_tid;
    register long x3 asm("x3") = (long)tls;
    register long x4 asm("x4") = (long)child_tid;
    register long x5 asm("x5") = (long)fn;
    register long x6 asm("x6") = (long)arg;

    asm volatile(
        "svc #0\n\t"         // 系统调用
        "cmp x0, #0\n\t"     // 检查是否为子进程/线程
        "bne 2f\n\t"         // 如果不是子进程/线程，跳转到2
        "cmp x5, #0\n\t"     // 检查fn是否为NULL
        "beq 1f\n\t"         // 如果是NULL，直接返回
        "mov x0, x6\n\t"     // 设置参数
        "blr x5\n\t"         // 调用入口函数
        "mov x8, #93\n\t"    // __NR_exit
        "svc #0\n\t"         // 退出
        "1:\n\t"             // fn为NULL的情况
        "2:\n\t"             // 父进程继续执行
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6)
        : "memory", "cc"
    );
    
    return x0;
}