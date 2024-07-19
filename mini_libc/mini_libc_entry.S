 .text
 .global _mini_libc_entry
 _mini_libc_entry:
 ldr x0, [sp, #0]
 add x1, sp, #8
 bl main
