#include "mini_lib.h"

static void *__mini_brk = NULL;

static inline void *__brk(void *end_data)
{
    void *result;
    asm volatile(
        "mov x8, #214\n"                // syscall number for brk
        "mov x0, %1\n"                  // end_data
        "svc #0\n"                      // Call the syscall
        "mov %0, x0\n"                  // Store the result in 'result'
        : "=r"(result)                  // 输出操作数列表
        : "r"(end_data)                 // 输入操作数列表,对应占位符顺序
        : "x0", "x8"                    // 告诉编译器在这段汇编代码中会被修改的寄存器, 顺序可以改变
    );

    return result;
}


int brk(void  *end_data)
{
  __mini_brk = __brk(end_data);
  if (__mini_brk < end_data)
  {
    return -1;
  }

  return 0;
}


void *sbrk(long increment)
{
    // 初始化 __mini_brk
    if (__mini_brk == NULL)
    {
        __mini_brk = __brk(NULL);
    }

    if (increment == 0)
    {
        return __mini_brk;
    }

    // 处理溢出情况
    unsigned long old_brk = (unsigned long)__mini_brk;
    if ((increment > 0 && (unsigned long)(increment) > (UINT_MAX - old_brk)) ||
        (increment < 0 && (unsigned long)(-increment) > old_brk))
    {
        return NULL;
    }

    void *desired_brk = (void *)(old_brk + increment);
    __mini_brk = __brk(desired_brk);
    if (__mini_brk < desired_brk)
    {
        return NULL;
    }

    return (void *)(old_brk);
}
