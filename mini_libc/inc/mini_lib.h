#ifndef _MINI_LIB_H_
#define _MINI_LIB_H_

#define NULL 0

#define AT_FDCWD		-100
#define O_RDONLY 00000000
#define O_WRONLY 00000001
#define O_RDWR 00000002
#define O_CREAT 00000100
#define O_TRUNC 00001000
#define O_APPEND 00002000

#define UINT_MAX 0xffffffffffffffffUL

#define PROT_READ        0x1                /* Page can be read.  */
#define PROT_WRITE        0x2               /* Page can be written.  */
#define PROT_EXEC        0x4                /* Page can be executed.  */
#define PROT_NONE        0x0                /* Page can not be accessed.  */
#define PROT_GROWSDOWN        0x01000000    /* Extend change to start of
                                               growsdown vma (mprotect only).  */
#define PROT_GROWSUP        0x02000000      /* Extend change to start of
                                               growsup vma (mprotect only).  */

/* Sharing types (must choose one and only one of these).  */
#define MAP_SHARED        0x01                /* Share changes.  */
#define MAP_PRIVATE        0x02                /* Changes are private.  */

#define va_list __builtin_va_list
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

#define __MINI_ALIGN(__value, __alignment) (((__value) + (__alignment)-1) & ~((__alignment)-1))

enum
{
    SEEK_SET = 0,
    SEEK_CUR,
    SEEK_END
};

int strlen(const char *s);
char *itoa(int num, char *str, int radix);
int write(int fd, const void *buf, int count);
int open(const char *pathname, int flags, int mode);
int close(int fd);
int printf(const char *format, ...);
int lseek(int fd, int offset, int whence);
int brk(void *end_data);
void *sbrk(long increment);
void *mmap(void *addr, long size, int prot, int flags, int fd, long offset);
int munmap(void *addr, long size);
void *malloc(int size);

#endif