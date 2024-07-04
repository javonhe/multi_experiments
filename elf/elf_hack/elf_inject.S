.data

msg:
    .ascii        "I am Inject!\n"

addr:
    .word 0x400540

.text

.globl _my_inject
_my_inject:
    mov     x0, #1      /* fd := STDOUT_FILENO */
    adr     x1, msg    /* buf := msg */
    mov     x2, #13    /* len = 13 */
    mov     w8, #64     /* write is syscall #64 */
    svc     #0          /* invoke syscall */

    adr     x11, addr
    ldr     x30, [x11]
    ret
