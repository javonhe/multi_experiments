/**
 * test_mini_lib.c - Mini C库测试程序
 * 
 * 支持通过命令行参数选择测试功能：
 * -f: 文件操作测试
 * -m: 内存操作测试
 * -p: 进程(fork)测试
 * -s: socket服务器测试
 * -c: socket客户端测试
 */

#include "mini_lib.h"

/**
 * 打印使用说明
 */
static void print_usage(const char* program) 
{
    printf("Usage: %s <test_mode> [args]\n", program);
    printf("Test modes:\n");
    printf("  -f <filename>: File operations test\n");
    printf("  -m: Memory operations test\n");
    printf("  -p: Process (fork) test\n");
    printf("  -s: Socket server test\n");
    printf("  -c: Socket client test\n");
}

/**
 * 文件操作测试
 */
static void test_file(const char* filename)
{
    printf("\n=== 开始文件操作测试 ===\n");
    
    int fd = open(filename, O_CREAT | O_APPEND | O_RDWR, 0644);
    printf("open file %s, fd = %d\n", filename, fd);

    if (fd > 0)
    {
        write(fd, "hello world", 12);

        // 测试lseek，打印文件长度
        int len = lseek(fd, 0, SEEK_END);
        printf("file length: %d\n", len);

        close(fd);
        printf("file closed\n");
    }
    
    printf("=== 文件操作测试完成 ===\n\n");
}

/**
 * 内存操作测试
 */
static void test_memory(void)
{
    printf("\n=== 开始内存操作测试 ===\n");
    
    // 测试sbrk
    void *p = sbrk(1024);
    printf("sbrk p: 0x%lx\n", (long)p);

    // 测试malloc和free
    char *str = (char *)malloc(256);
    if (str != NULL) 
    {
        printf("malloc success, addr: 0x%lx\n", (long)str);
        
        memset(str, 'A', 255);
        str[255] = '\0';
        printf("after memset, str: %s\n", str);
        
        free(str);
        printf("free success\n");
    } 
    else 
    {
        printf("malloc failed\n");
    }

    // 测试mmap
    void *mmap_p = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mmap_p == MAP_FAILED || mmap_p == NULL)
    {
        printf("mmap failed\n");
    }
    else
    {
        printf("mmap p: 0x%lx\n", (long)mmap_p);
        munmap(mmap_p, 1024);
        printf("munmap success\n");
    }
    
    printf("=== 内存操作测试完成 ===\n\n");
}

/**
 * Fork功能测试
 */
static void test_fork(void)
{
    printf("\n=== 开始Fork功能测试 ===\n");
    
    pid_t pid = fork();
    if (pid < 0) 
    {
        printf("fork failed\n");
        return;
    }
    
    if (pid == 0) 
    {
        // 子进程
        for (int i = 0; i < 100000000; i++); // 子进程忙等待，模拟延时
        printf("fork child, PID: %d\n", getpid());
    } 
    else 
    {
        // 父进程
        printf("fork parent, PID: %d, child PID: %d\n", getpid(), pid);
    }
    
    printf("=== Fork功能测试完成 ===\n\n");
}

/**
 * socket测试函数 - 服务器端
 */
static void test_socket_server(void)
{
    printf("\n=== 开始Socket服务器测试 ===\n");
    
    int server_fd;
    struct sockaddr_in server_addr;
    
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) 
    {
        printf("Server: socket create failed\n");
        return;
    }
    printf("Server: socket created, fd=%d\n", server_fd);
    
    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
    server_addr.sin_port = htons(8080);        // 监听8080端口
    
    // 绑定地址
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        printf("Server: bind failed\n");
        return;
    }
    printf("Server: bind success\n");
    
    // 开始监听
    if (listen(server_fd, 1) < 0) 
    {
        printf("Server: listen failed\n");
        return;
    }
    printf("Server: listening on port 8080\n");
    
    // 接受连接
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) 
    {
        printf("Server: accept failed\n");
        return;
    }
    printf("Server: client connected\n");
    
    // 接收数据
    char buf[128];
    memset(buf, 0, sizeof(buf));
    int recv_len = recv(client_fd, buf, sizeof(buf)-1, 0);
    if (recv_len > 0) 
    {
        printf("Server received: %s\n", buf);
        
        // 发送响应
        const char *response = "Hello from server!";
        send(client_fd, response, strlen(response), 0);
    }
    
    // 关闭连接
    close(client_fd);
    close(server_fd);
    printf("Server: connection closed\n");
    
    printf("=== Socket服务器测试完成 ===\n\n");
}

/**
 * socket测试函数 - 客户端
 */
static void test_socket_client(void)
{
    printf("\n=== 开始Socket客户端测试 ===\n");
    
    int sock_fd;
    struct sockaddr_in server_addr;
    
    // 创建socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) 
    {
        printf("Client: socket create failed\n");
        return;
    }
    printf("Client: socket created, fd=%d\n", sock_fd);
    
    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 连接本地服务器
    
    // 连接服务器
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        printf("Client: connect failed\n");
        return;
    }
    printf("Client: connected to server\n");
    
    // 发送数据
    const char *message = "Hello from client!";
    send(sock_fd, message, strlen(message), 0);
    printf("Client: message sent\n");
    
    // 接收响应
    char buf[128];
    memset(buf, 0, sizeof(buf));
    int recv_len = recv(sock_fd, buf, sizeof(buf)-1, 0);
    if (recv_len > 0) 
    {
        printf("Client received: %s\n", buf);
    }
    
    // 关闭连接
    close(sock_fd);
    printf("Client: connection closed\n");
    
    printf("=== Socket客户端测试完成 ===\n\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2) 
    {
        print_usage(argv[0]);
        return -1;
    }

    // 根据第一个参数选择测试模式
    switch (argv[1][1])  // 跳过'-'
    {
        case 'f':  // 文件操作测试
            if (argc < 3) 
            {
                printf("Error: Missing filename for file test\n");
                print_usage(argv[0]);
                return -1;
            }
            test_file(argv[2]);
            break;
            
        case 'm':  // 内存操作测试
            test_memory();
            break;
            
        case 'p':  // 进程(fork)测试
            test_fork();
            break;
            
        case 's':  // socket服务器测试
            test_socket_server();
            break;
            
        case 'c':  // socket客户端测试
            test_socket_client();
            break;
            
        default:
            printf("Error: Unknown test mode '%s'\n", argv[1]);
            print_usage(argv[0]);
            return -1;
    }

    return 0;
}