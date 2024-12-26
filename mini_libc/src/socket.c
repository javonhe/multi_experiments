/**
 * socket.c - aarch64平台socket系统调用实现
 * 
 * 本文件实现了基础的socket系统调用包装函数，用于网络通信。
 * 实现了以下功能：
 * - socket创建
 * - 连接建立
 * - 地址绑定
 * - 监听连接
 * - 接受连接
 * - 数据收发
 * 
 * 所有函数都使用内联汇编直接调用Linux系统调用实现，
 * 不依赖任何外部库。
 */

#include "mini_lib.h"

/* 全局错误码变量 */
static int mini_errno;

/* Linux系统调用号定义 */
#define __NR_socket      198  // 创建socket
#define __NR_connect    203   // 连接到远程地址
#define __NR_bind       200   // 绑定本地地址
#define __NR_listen     201   // 监听连接
#define __NR_accept     202   // 接受连接
#define __NR_sendto     206   // 发送数据
#define __NR_recvfrom   207   // 接收数据
#define __NR_close      57    // 关闭socket

/**
 * 将主机字节序转换为网络字节序(16位)
 * 
 * 网络字节序统一使用大端序（高字节在前），而主机字节序
 * 可能是大端或小端。aarch64平台为小端字节序，因此
 * 需要进行字节顺序的转换。
 * 
 * @param hostshort 主机字节序的16位整数
 * @return 网络字节序的16位整数
 */
unsigned short htons(unsigned short hostshort)
{
    return ((hostshort & 0xFF) << 8) | ((hostshort & 0xFF00) >> 8);
}

/**
 * 创建一个新的socket
 * 
 * 系统调用参数说明：
 * x8 = __NR_socket (系统调用号)
 * x0 = domain (协议族，如AF_INET)
 * x1 = type (socket类型，如SOCK_STREAM)
 * x2 = protocol (协议，通常为0)
 * 
 * @param domain   协议族(如AF_INET)
 * @param type     socket类型(如SOCK_STREAM)
 * @param protocol 协议(通常为0)
 * @return         成功返回socket文件描述符，失败返回-1
 */
int socket(int domain, int type, int protocol)
{
    // register 告诉编译器将变量存储在寄存器中
    // asm("x8") 告诉编译器指定x8为寄存器
    register long x8 asm("x8") = __NR_socket; // 将系统调用号给到变量 x8，并且变量x8存储在寄存器x8中
    register long x0 asm("x0") = domain; // 将domain的值给到变量 x0，并且变量x0存储在寄存器x0中
    register long x1 asm("x1") = type; // 将type的值给到变量 x1，并且变量x1存储在寄存器x1中
    register long x2 asm("x2") = protocol; // 将protocol的值给到变量 x2，并且变量x2存储在寄存器x2中 
    
    asm volatile(
        "svc #0"
        : "+r"(x0)    // 输出：修改x0的值（返回值）
        : "r"(x8), "r"(x0), "r"(x1), "r"(x2)  // 输入：系统调用号和所有参数
        : "memory", "cc"
    );
    
    if (x0 < 0) {
        mini_errno = -x0;
        return -1;
    }
    return x0;
}

/**
 * 连接到远程地址
 * 
 * 用于客户端连接到服务器。系统调用参数：
 * x8 = __NR_connect
 * x0 = sockfd (socket文件描述符)
 * x1 = addr (目标地址结构体指针)
 * x2 = addrlen (地址结构体长度)
 * 
 * @param sockfd   socket文件描述符
 * @param addr     目标地址结构体
 * @param addrlen  地址结构体长度
 * @return         成功返回0，失败返回-1
 */
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    register long x8 asm("x8") = __NR_connect;
    register long x0 asm("x0") = sockfd;
    register long x1 asm("x1") = (long)addr;
    register long x2 asm("x2") = addrlen;
    
    asm volatile(
        "svc #0"
        : "+r"(x0)    // 输出：修改x0的值（返回值）
        : "r"(x8), "r"(x0), "r"(x1), "r"(x2)  // 输入：系统调用号和所有参数
        : "memory", "cc"
    );
    
    if (x0 < 0) {
        mini_errno = -x0;
        return -1;
    }
    return 0;
}

/**
 * 绑定socket到本地地址
 * 
 * 通常用于服务器绑定监听地址。系统调用参数：
 * x8 = __NR_bind
 * x0 = sockfd
 * x1 = addr (本地地址结构体指针)
 * x2 = addrlen
 * 
 * @param sockfd   socket文件描述符
 * @param addr     本地地址结构体
 * @param addrlen  地址结构体长度
 * @return         成功返回0，失败返回-1
 */
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    register long x8 asm("x8") = __NR_bind;
    register long x0 asm("x0") = sockfd;
    register long x1 asm("x1") = (long)addr;
    register long x2 asm("x2") = addrlen;
    
    asm volatile(
        "svc #0"
        : "+r"(x0)    // 输出：修改x0的值（返回值）
        : "r"(x8), "r"(x0), "r"(x1), "r"(x2)  // 输入：系统调用号和所有参数
        : "memory", "cc"
    );
    
    if (x0 < 0) {
        mini_errno = -x0;
        return -1;
    }
    return 0;
}

/**
 * 开始监听连接
 * 
 * 将socket置于监听状态，准备接受连接。系统调用参数：
 * x8 = __NR_listen
 * x0 = sockfd
 * x1 = backlog (等待连接队列的最大长度)
 * 
 * @param sockfd   socket文件描述符
 * @param backlog  待处理连接队列的最大长度
 * @return         成功返回0，失败返回-1
 */
int listen(int sockfd, int backlog)
{
    register long x8 asm("x8") = __NR_listen;
    register long x0 asm("x0") = sockfd;
    register long x1 asm("x1") = backlog;
    
    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x0), "r"(x1)  // 添加x0作为输入
        : "memory", "cc"
    );
    
    if (x0 < 0) {
        mini_errno = -x0;
        return -1;
    }
    return 0;
}

/**
 * 接受一个新的连接
 * 
 * 从监听socket接受一个新的连接。系统调用参数：
 * x8 = __NR_accept
 * x0 = sockfd
 * x1 = addr (用于存储客户端地址)
 * x2 = addrlen (地址长度指针)
 * 
 * @param sockfd   socket文件描述符
 * @param addr     用于存储客户端地址的结构体
 * @param addrlen  地址结构体长度的指针
 * @return         成功返回新的socket文件描述符，失败返回-1
 */
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    register long x8 asm("x8") = __NR_accept;
    register long x0 asm("x0") = sockfd;
    register long x1 asm("x1") = (long)addr;
    register long x2 asm("x2") = (long)addrlen;
    
    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x0), "r"(x1), "r"(x2)  // 添加x0作为输入
        : "memory", "cc"
    );
    
    if (x0 < 0) {
        mini_errno = -x0;
        return -1;
    }
    return x0;
}

/**
 * 发送数据
 * 
 * 通过socket发送数据。系统调用参数：
 * x8 = __NR_send
 * x0 = sockfd
 * x1 = buf (数据缓冲区)
 * x2 = len (数据长度)
 * x3 = flags (发送标志)
 * 
 * @param sockfd   socket文件描述符
 * @param buf      要发送的数据缓冲区
 * @param len      要发送的数据长度
 * @param flags    发送标志
 * @return         成功返回发送的字节数，失败返回-1
 */
ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    register long x8 asm("x8") = __NR_sendto;
    register long x0 asm("x0") = sockfd;
    register long x1 asm("x1") = (long)buf;
    register long x2 asm("x2") = len;
    register long x3 asm("x3") = flags;
    register long x4 asm("x4") = 0;   // src_addr = NULL
    register long x5 asm("x5") = 0;   // addrlen = NULL
    
    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x0), "r"(x1), "r"(x2), "r"(x3)  // 添加x0作为输入
        : "memory", "cc"
    );
    
    if (x0 < 0) {
        mini_errno = -x0;
        return -1;
    }
    return x0;
}

/**
 * 接收数据
 * 
 * 从socket接收数据。系统调用参数：
 * x8 = __NR_recv
 * x0 = sockfd
 * x1 = buf (接收缓冲区)
 * x2 = len (缓冲区长度)
 * x3 = flags (接收标志)
 * 
 * @param sockfd   socket文件描述符
 * @param buf      接收数据的缓冲区
 * @param len      缓冲区长度
 * @param flags    接收标志
 * @return         成功返回接收的字节数，失败返回-1
 */
ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    printf("DEBUG: recv called with sockfd=%d, buf=%lx, len=%lx, flags=%d\n", 
           sockfd, (unsigned long)buf, (unsigned long)len, flags);

    register long x8 asm("x8") = __NR_recvfrom;
    register long x0 asm("x0") = sockfd;
    register long x1 asm("x1") = (long)buf;
    register long x2 asm("x2") = len;
    register long x3 asm("x3") = flags;
    register long x4 asm("x4") = 0;   // src_addr = NULL
    register long x5 asm("x5") = 0;   // addrlen = NULL
    
    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory", "cc"
    );
    
    if (x0 < 0) {
        mini_errno = -x0;
        printf("DEBUG: recv failed with errno=%d\n", mini_errno);
        return -1;
    }
    
    printf("DEBUG: recv succeeded, returned %ld bytes\n", x0);
    return x0;
}
