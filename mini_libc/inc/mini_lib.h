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

#define va_list __builtin_va_list
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

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

#endif