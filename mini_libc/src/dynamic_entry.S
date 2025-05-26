.global _dynamic_entry
.type _dynamic_entry, @function

_dynamic_entry:
    # 保存链接寄存器
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    
    # 调用动态库中的初始化函数
    bl _mini_libc_entry
    
    # 恢复链接寄存器并返回
    ldp x29, x30, [sp], #16
    ret
    
.size _dynamic_entry, .-_dynamic_entry