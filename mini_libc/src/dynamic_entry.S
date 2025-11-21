.text
.global _start
.type _start, @function

_start:
    # 内核调用解释器时，参数通过栈传递
    # 直接将栈指针传递给 main 函数，让 C 代码解析
    # main(void *stack_pointer)
    mov x0, sp
    bl main
    
    # main 函数返回后，使用返回值作为退出码, 返回值在x0中
    # __NR_exit 系统调用号为 93 (AArch64)
    # 注意：x0 已经包含 main 的返回值
    mov x8, #93
    svc #0
