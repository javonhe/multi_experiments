.data

/* Data segment: define our message string and calculate its length. */
msg:
    .ascii        "Hello, ARM64!\n"

.text

/* Our application's entry point. */
.globl _my_inject
_my_inject:
    /* syscall write(int fd, const void *buf, size_t count) */
    mov     x0, #1      /* fd := STDOUT_FILENO */
    adr     x1, msg    /* buf := msg */
    mov     x2, #14    /* count := len */
    mov     w8, #64     /* write is syscall #64 */
    svc     #0          /* invoke syscall */

    /* syscall exit(int status) */
    mov     x0, #0      /* status := 0 */
    mov     w8, #93     /* exit is syscall #1 */
    svc     #0          /* invoke syscall */