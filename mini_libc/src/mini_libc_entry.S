 .text
 .global _mini_libc_entry
 
 _mini_libc_entry:
 ldr x0, [sp, #0] //获取argc
 add x1, sp, #8 //获取argv
 bl main //跳转到main函数，根据传参规则，会分别从x0、x1获取参数
 _mini_libc_exit:
 mov x8, #93 //sys_exit的软中断号
 mov x0, #0 //参数
 svc #0 //软中断
