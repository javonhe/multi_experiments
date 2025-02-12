/**
 * pthread_mutex.c - 互斥锁实现
 */

#include "mini_lib.h"

/* 系统调用号定义 */
#define __NR_futex   98

/* futex操作码 */
#define FUTEX_WAIT   0
#define FUTEX_WAKE   1

/**
 * 原子比较和交换操作
 * 
 * 将ptr指向的值与expected比较,如果相等则将其替换为desired值
 * 整个操作是原子的,不会被中断
 *
 * @param ptr      指向要操作的内存地址的指针
 * @param expected 期望的原值
 * @param desired  要设置的新值
 * @return         1表示交换成功,0表示失败(原值与expected不相等)
 */
static inline int atomic_cas(volatile int *ptr, int expected, int desired)
{
    int tmp;
    int success;

    asm volatile(
        "1: ldxr %w0, [%2]\n"         // 加载当前值到 tmp (%w0)
        "   cmp %w0, %w3\n"           // 比较 tmp 和 expected (%w3)
        "   b.ne 2f\n"                // 不相等则跳转到失败分支
        "   stxr %w1, %w4, [%2]\n"    // 尝试存储 desired (%w4)
        "   cbnz %w1, 1b\n"           // 存储失败则重试
        "   dmb ish\n"                // 内存屏障（release语义）
        "   mov %w1, #1\n"            // 设置 success = 1
        "   b 3f\n"
        "2: mov %w1, #0\n"            // 设置 success = 0
        "3:"
        : "=&r" (tmp), "=&r" (success)  // 输出操作数
        : "r" (ptr),                    // 输入操作数 %2
          "r" (expected),               // 输入操作数 %3
          "r" (desired)                 // 输入操作数 %4
        : "cc", "memory"
    );

    return success;
}

/**
 * 原子存储操作
 * 
 * 使用stlr指令实现原子存储,具有release语义。
 * 确保在此存储之前的所有内存访问都已完成。
 *
 * @param ptr: 要存储的目标内存地址
 * @param val: 要存储的值
 * @note: 使用stlr指令,具有内存屏障效果,保证存储的原子性
 */
static inline void atomic_store(volatile int *ptr, int val)
{
    asm volatile(
        "stlr %w1, [%0]"
        : 
        : "r" (ptr), "r" (val)
        : "memory"
    );
}

/**
 * futex 系统调用包装
 * 
 * 封装Linux的futex系统调用,用于实现用户态和内核态的同步机制。
 * 当用户态的同步操作需要阻塞时,可以通过该系统调用让内核介入。
 * 
 * @param uaddr: 用户空间的同步变量地址。这是一个指向整型的指针,指向需要同步的内存位置。
 *              该变量必须在进程间共享的内存中。
 * 
 * @param futex_op: futex操作类型。主要的操作类型包括:
 *                  - FUTEX_WAIT: 如果*uaddr == val,则让调用进程休眠
 *                  - FUTEX_WAKE: 最多唤醒val个等待在uaddr上的进程
 * 
 * @param val: 操作相关的值,具体含义取决于futex_op:
 *            - 对于FUTEX_WAIT: 用于和*uaddr比较的期望值
 *            - 对于FUTEX_WAKE: 要唤醒的最大进程数
 * 
 * @param timeout: 超时参数,指定等待的最长时间:
 *                - NULL: 表示永久等待
 *                - 非NULL: 指定具体的超时时间结构体
 * 
 * @param uaddr2: 用于某些复杂的futex操作的第二个同步变量地址。
 *               对于基本的WAIT/WAKE操作,该参数通常为NULL。
 * 
 * @param val3: 用于某些复杂futex操作的附加值。
 *             对于基本的WAIT/WAKE操作,该参数通常为0。
 * 
 * @return: 操作结果:
 *         - 0: 操作成功
 *         - >0: 对于FUTEX_WAKE返回唤醒的进程数
 *         - <0: 失败,返回负的错误码
 */
int futex(volatile int *uaddr, int futex_op, int val,
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
    
    return (int)x0;
}

/**
 * 初始化互斥锁
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    mutex->lock = 0;
    mutex->owner = 0;
    return 0;
}


static inline int atomic_load(volatile int *ptr)
{
    int value;
    asm volatile(
        "ldar %w0, [%1]"
        : "=r"(value)
        : "r"(ptr)
        : "memory"
    );
    return value;
}

/**
 * 加锁操作
 * 
 * 对互斥锁进行加锁。如果锁已被其他线程持有,则当前线程将被阻塞,直到能获取到锁。
 * 采用自旋+休眠的混合策略,先尝试自旋一定次数,如果仍无法获取锁则通过futex系统调用休眠。
 * 
 * @param mutex: 要加锁的互斥锁指针
 * @return: 成功返回0,失败返回错误码
 *         - EINVAL: 参数无效
 *         - EDEADLK: 检测到死锁(重入)
 * 
 * @note: 同一个线程不能对同一个互斥锁重复加锁,否则会返回错误
 */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    int tid = gettid();
    int spin_count = 100;  // 自旋计数

    // 检查参数
    if (!mutex)
    {
        return -1;
    }

    // 检查重入
    if (mutex->owner == tid)
    {
        return -1;
    }

    while (1)
    {
        if (atomic_cas(&mutex->lock, 0, 1))
        {
            mutex->owner = tid;
            return 0;
        }

        // 自旋等待
        if (spin_count-- > 0)
        {
            __asm__ volatile("yield");  // 使用 yield 指令让出 CPU
            continue;
        }

        // 应使用原子读取
        int ret = futex(&mutex->lock, FUTEX_WAIT, 1, NULL, NULL, 0);
        if (ret < 0)
        {
            // 通过系统调用返回值判断错误类型
            int err = -ret;
            if (err == EAGAIN)  // 需要确认系统调用返回码
            {
                spin_count = 100;
                continue;
            }
            if (err == EINTR)
            {
                continue;
            }
            return -MINI_EINVAL;
        }
        spin_count = 100;  // 重置自旋计数
    }
}

/**
 * 解锁操作
 * @param mutex: 要解锁的互斥锁
 * @return: 成功返回0，失败返回错误码
 * @note: 只有锁的持有者(owner)才能解锁，否则返回错误
 * @note: 解锁后会唤醒一个等待该锁的线程
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    int tid = gettid();

    // 参数检查
    if (!mutex)
    {
        return -1;
    }

    // 所有权检查
    if (mutex->owner != tid)
    {
        return -1;  // 非持有者解锁错误
    }

    // 先清除 owner，再释放锁
    mutex->owner = 0;
    atomic_store(&mutex->lock, 0);

    // 唤醒等待者
    int ret = futex(&mutex->lock, FUTEX_WAKE, 1, NULL, NULL, 0);
    if (ret == -1)
    {
        // futex wake 失败，但锁已经释放，继续返回成功
        return 0;
    }

    return 0;
}