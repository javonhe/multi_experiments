#ifndef _MINI_LIB_H_
#define _MINI_LIB_H_

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

// 基本类型定义
typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned long uintptr_t;
typedef int pid_t;
typedef unsigned int socklen_t;

// 变长参数相关定义
#define va_list __builtin_va_list
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

// mmap相关常量定义
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define MAP_SHARED  0x01                /* Share changes.  */
#define MAP_PRIVATE 0x02                /* Changes are private.  */
#define MAP_ANONYMOUS 0x20              /* Don't use a file.  */
#define MAP_STACK    0x20000            /* Stack mapping.  */
#define MAP_GROWSDOWN 0x0100            /* Stack grows down.  */
#define MAP_HUGETLB   0x40000          /* Create huge page mapping.  */
#define MAP_FIXED     0x10              /* Interpret addr exactly.  */
#define MAP_FILE     0
#define MAP_ANON     MAP_ANONYMOUS
#define MAP_FAILED ((void *)-1)
#define NULL ((void*)0)

/* clone标志位 */
#define CLONE_CHILD_CLEARTID     0x00200000  // 子进程退出时清除TID
#define CLONE_CHILD_SETTID       0x01000000  // 设置子进程的TID
#define CLONE_PARENT_SETTID      0x00100000  // 设置父进程的TID
#define CLONE_VM                 0x00000100  // 共享内存空间
#define CLONE_FS                 0x00000200  // 共享文件系统信息
#define CLONE_FILES              0x00000400  // 共享文件描述符表
#define CLONE_SIGHAND            0x00000800  // 共享信号处理函数
#define CLONE_THREAD             0x00010000  // 同一线程组
#define CLONE_SYSVSEM            0x00040000   // 共享System V SEM_UNDO语义
#define CLONE_SETTLS             0x00080000   // 创建新的TLS
#define CLONE_PARENT_SETTID      0x00100000  // 将TID写入父进程
#define CLONE_CHILD_CLEARTID     0x00200000  // 清除子进程的TID
#define CLONE_CHILD_SETTID       0x01000000
#define CLONE_DETACHED           0x00400000  // 分离状态

/* Socket相关类型和常量 */
typedef unsigned int socklen_t;

/* 协议族 */
#define AF_INET     2

/* Socket类型 */
#define SOCK_STREAM 1
#define SOCK_DGRAM  2

/* 特殊IP地址 */
#define INADDR_ANY ((unsigned long)0x00000000)

// 网络相关结构体定义
struct sockaddr
{
    unsigned short sa_family;
    char sa_data[14];
};

struct in_addr
{
    unsigned long s_addr;
};

struct sockaddr_in
{
    short          sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char           sin_zero[8];
};

struct timespec 
{
    long tv_sec;    /* 秒 */
    long tv_nsec;   /* 纳秒 */
};

// 字符串操作函数声明
int strlen(const char *s);
char *itoa(long num, char *str, int radix, unsigned char sign_flag);
void *memcpy(void *dest, const void *src, size_t n);

// 文件操作函数声明
int write(int fd, const void *buf, int count);
int open(const char *pathname, int flags, int mode);
int close(int fd);
int lseek(int fd, int offset, int whence);
// 打印函数声明
int printf(const char *format, ...);

// 内存管理函数声明
int brk(void *end_data);
void *sbrk(long increment);
void *mmap(void *addr, long size, int prot, int flags, int fd, long offset);
int munmap(void *addr, long size);
void* malloc(size_t size);
void free(void* ptr);
void* memset(void* s, int c, size_t n);

// 进程操作函数声明
int fork(void);
pid_t getpid(void);
int clone(int (*fn)(void *), void *stack, int flags, void *arg, ...);

// Socket操作函数声明
int socket(int domain, int type, int protocol);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
unsigned short htons(unsigned short hostshort);

/* pthread相关定义 */
typedef unsigned long pthread_t;
typedef struct pthread_attr_t {
    int dummy;  // 暂时不支持线程属性
} pthread_attr_t;

// pthread函数声明
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                  void *(*start_routine)(void*), void *arg);
int pthread_join(pthread_t thread, void **retval);

/* pthread错误码定义 */
#define EINVAL      22      /* Invalid argument */
#define EAGAIN      11      /* Try again */
#define ENOMEM      12      /* Out of memory */
#define ESRCH       3       /* No such process */
#define EDEADLK     35      /* Resource deadlock would occur */

#endif