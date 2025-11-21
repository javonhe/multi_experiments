#include "mini_lib.h"

// 全局变量：保存环境变量指针
static char **g_environ = NULL;

// 辅助向量 (auxv) 类型定义
typedef struct {
    uint64_t tag;
    uint64_t value;
} AuxvEntry;

// 辅助向量标签定义 (AT_*)
#define AT_NULL      0    // 结束标记
#define AT_IGNORE    1    // 忽略
#define AT_EXECFD    2    // 可执行文件描述符
#define AT_PHDR      3    // 程序头表地址
#define AT_PHENT     4    // 程序头表项大小
#define AT_PHNUM     5    // 程序头表项数量
#define AT_PAGESZ    6    // 页大小
#define AT_BASE      7    // 动态链接器基址
#define AT_FLAGS     8    // 标志
#define AT_ENTRY     9    // 可执行文件入口点
#define AT_NOTELF    10   // 不是 ELF 文件
#define AT_UID       11   // 用户 ID
#define AT_EUID      12   // 有效用户 ID
#define AT_GID       13   // 组 ID
#define AT_EGID      14   // 有效组 ID
#define AT_PLATFORM  15   // 平台字符串
#define AT_HWCAP     16   // 硬件能力
#define AT_CLKTCK    17   // 时钟频率
#define AT_SECURE     23   // 安全模式
#define AT_RANDOM    25   // 随机数种子
#define AT_EXECFN    31   // 可执行文件路径

// 全局变量：保存重要的 auxv 值
static uint64_t g_auxv_entry = 0;      // AT_ENTRY: 可执行文件入口点
static uint64_t g_auxv_phdr = 0;       // AT_PHDR: 程序头表地址
static uint64_t g_auxv_phent = 0;      // AT_PHENT: 程序头表项大小
static uint64_t g_auxv_phnum = 0;      // AT_PHNUM: 程序头表项数量
static uint64_t g_auxv_pagesz = 0;     // AT_PAGESZ: 页大小
static uint64_t g_auxv_base = 0;       // AT_BASE: 动态链接器基址
static const char *g_auxv_platform = NULL;  // AT_PLATFORM: 平台字符串
static const char *g_auxv_execfn = NULL;     // AT_EXECFN: 可执行文件路径


/**
 * 简单的字符串长度函数（不使用 PLT）
 */
static size_t strlen_simple(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
    {
        len++;
    }
    return len;
}

/**
 * 将整数转换为十进制字符串（不使用 PLT）
 */
static void itoa_simple(int64_t value, char *buf, int base)
{
    char *p = buf;
    int negative = 0;
    
    if (value < 0 && base == 10)
    {
        negative = 1;
        value = -value;
    }
    
    if (value == 0)
    {
        *p++ = '0';
        *p = '\0';
        return;
    }
    
    // 从低位到高位转换
    char temp[32];
    int i = 0;
    while (value > 0)
    {
        int digit = value % base;
        temp[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    }
    
    if (negative)
    {
        *p++ = '-';
    }
    
    // 反转字符串
    while (i > 0)
    {
        *p++ = temp[--i];
    }
    *p = '\0';
}

/**
 * 将无符号整数转换为十六进制字符串（不使用 PLT）
 */
static void utoa_hex_simple(uint64_t value, char *buf)
{
    char *p = buf;
    
    if (value == 0)
    {
        *p++ = '0';
        *p = '\0';
        return;
    }
    
    // 从低位到高位转换
    char temp[32];
    int i = 0;
    while (value > 0)
    {
        int digit = value % 16;
        temp[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= 16;
    }
    
    // 反转字符串
    while (i > 0)
    {
        *p++ = temp[--i];
    }
    *p = '\0';
}

/**
 * 简单的 write 系统调用（不使用 PLT）
 * AArch64: __NR_write = 64 (0x40)
 */
static int write_simple(int fd, const void *buf, size_t count)
{
    long result;
    asm volatile(
        "mov x8, #0x40\n"           // __NR_write = 64
        "mov x0, %1\n"              // fd
        "mov x1, %2\n"              // buf
        "mov x2, %3\n"              // count
        "svc #0\n"                  // 系统调用
        "mov %0, x0\n"              // 返回值
        : "=r"(result)
        : "r"(fd), "r"(buf), "r"(count)
        : "x0", "x1", "x2", "x8"
    );
    return (int)result;
}


/**
 * 简单的 printf 实现（不使用 PLT/GOT）
 * 支持格式：%s, %d, %x, %p, %c, %%
 * 
 * 注意：这是一个简化实现，使用内联汇编获取可变参数
 */
static int printf_simple(const char *format, ...)
{
    char buffer[1024];
    char num_buf[32];
    int buf_pos = 0;
    
    // 使用内联汇编获取可变参数
    // 在 AArch64 上，可变参数通过寄存器 x1-x7 和栈传递
    // 我们使用 __builtin_va_list 来获取参数
    __builtin_va_list args;
    __builtin_va_start(args, format);
    
    const char *p = format;
    
    while (*p != '\0' && buf_pos < sizeof(buffer) - 1)
    {
        if (*p == '%')
        {
            p++;
            switch (*p)
            {
                case 's':  // 字符串
                {
                    const char *str = __builtin_va_arg(args, const char *);
                    if (str)
                    {
                        size_t len = strlen_simple(str);
                        if (buf_pos + len < sizeof(buffer) - 1)
                        {
                            for (size_t i = 0; i < len; i++)
                            {
                                buffer[buf_pos++] = str[i];
                            }
                        }
                    }
                    break;
                }
                case 'd':  // 十进制整数
                {
                    int val = __builtin_va_arg(args, int);
                    itoa_simple(val, num_buf, 10);
                    size_t len = strlen_simple(num_buf);
                    if (buf_pos + len < sizeof(buffer) - 1)
                    {
                        for (size_t i = 0; i < len; i++)
                        {
                            buffer[buf_pos++] = num_buf[i];
                        }
                    }
                    break;
                }
                case 'x':  // 十六进制整数（小写）
                {
                    unsigned int val = __builtin_va_arg(args, unsigned int);
                    utoa_hex_simple(val, num_buf);
                    size_t len = strlen_simple(num_buf);
                    if (buf_pos + len < sizeof(buffer) - 1)
                    {
                        for (size_t i = 0; i < len; i++)
                        {
                            buffer[buf_pos++] = num_buf[i];
                        }
                    }
                    break;
                }
                case 'p':  // 指针（作为十六进制处理）
                {
                    void *val = __builtin_va_arg(args, void *);
                    buffer[buf_pos++] = '0';
                    buffer[buf_pos++] = 'x';
                    utoa_hex_simple((uint64_t)val, num_buf);
                    size_t len = strlen_simple(num_buf);
                    if (buf_pos + len < sizeof(buffer) - 1)
                    {
                        for (size_t i = 0; i < len; i++)
                        {
                            buffer[buf_pos++] = num_buf[i];
                        }
                    }
                    break;
                }
                case 'c':  // 字符
                {
                    char c = (char)__builtin_va_arg(args, int);  // char 提升为 int
                    if (buf_pos < sizeof(buffer) - 1)
                    {
                        buffer[buf_pos++] = c;
                    }
                    break;
                }
                case '%':  // 百分号
                {
                    if (buf_pos < sizeof(buffer) - 1)
                    {
                        buffer[buf_pos++] = '%';
                    }
                    break;
                }
                case '\n':  // 换行符（方便使用）
                {
                    if (buf_pos < sizeof(buffer) - 1)
                    {
                        buffer[buf_pos++] = '\n';
                    }
                    break;
                }
                default:
                    // 未知格式，原样输出
                    if (buf_pos < sizeof(buffer) - 1)
                    {
                        buffer[buf_pos++] = '%';
                        if (*p != '\0')
                        {
                            buffer[buf_pos++] = *p;
                        }
                    }
                    break;
            }
            if (*p != '\0')
            {
                p++;
            }
        }
        else
        {
            buffer[buf_pos++] = *p++;
        }
    }
    
    buffer[buf_pos] = '\0';
    
    __builtin_va_end(args);
    
    // 写入 stdout (fd = 1)
    if (buf_pos > 0)
    {
        return write_simple(1, buffer, buf_pos);
    }
    
    return 0;
}

/**
 * 简单的字符串比较函数（带长度限制，不使用 PLT）
 */
static int strncmp_simple(const char *s1, const char *s2, size_t n)
{
    while (n-- && *s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    
    if (n == (size_t)-1)
    {
        return 0;
    }
    
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/**
 * 获取环境变量值（不使用 PLT）
 * 
 * @param name: 环境变量名
 * @return: 环境变量值，不存在返回NULL
 */
static char* get_env(const char *name)
{
    if (!g_environ || !name) 
    {
        return NULL;
    }
    
    size_t name_len = strlen_simple(name);
    
    // 遍历环境变量数组
    for (char **env = g_environ; *env != NULL; env++) 
    {
        // 检查变量名是否匹配（比较前 name_len 个字符）
        if (strncmp_simple(*env, name, name_len) == 0 && (*env)[name_len] == '=') 
        {
            // 返回 '=' 后面的值
            return (*env) + name_len + 1;
        }
    }
    
    return NULL;
}

/**
 * 打印所有环境变量（用于调试）
 */
static void print_all_env(void)
{
    if (!g_environ) 
    {
        return;
    }
    
    printf_simple("=== Environment Variables ===\n");
    for (char **env = g_environ; *env != NULL; env++) 
    {
        printf_simple("%s\n", *env);
    }
    printf_simple("============================\n");
}

/**
 * 获取辅助向量值
 * 
 * @param auxv: 辅助向量数组
 * @param tag: 要查找的标签
 * @return: 对应的值，不存在返回0
 */
static uint64_t get_auxv(uint64_t *auxv, uint64_t tag)
{
    if (!auxv) 
    {
        return 0;
    }
    
    // 遍历辅助向量数组
    for (uint64_t *p = auxv; p[0] != AT_NULL; p += 2) 
    {
        if (p[0] == tag) 
        {
            return p[1];
        }
    }
    
    return 0;
}

/**
 * 解析辅助向量 (auxv)
 * 
 * @param auxv: 辅助向量数组指针
 */
static void parse_auxv(uint64_t *auxv)
{
    if (!auxv) 
    {
        printf_simple("auxv is NULL\n");
        return;
    }
    
    printf_simple("=== Parsing Auxiliary Vector ===\n");
    
    // 解析并保存重要的 auxv 值
    g_auxv_entry = get_auxv(auxv, AT_ENTRY);
    g_auxv_phdr = get_auxv(auxv, AT_PHDR);
    g_auxv_phent = get_auxv(auxv, AT_PHENT);
    g_auxv_phnum = get_auxv(auxv, AT_PHNUM);
    g_auxv_pagesz = get_auxv(auxv, AT_PAGESZ);
    g_auxv_base = get_auxv(auxv, AT_BASE);
    
    uint64_t platform_ptr = get_auxv(auxv, AT_PLATFORM);
    if (platform_ptr) 
    {
        g_auxv_platform = (const char *)platform_ptr;
    }
    
    uint64_t execfn_ptr = get_auxv(auxv, AT_EXECFN);
    if (execfn_ptr) 
    {
        g_auxv_execfn = (const char *)execfn_ptr;
    }
    
    // 使用 printf_simple 打印所有重要的 auxv 值
    if (g_auxv_entry) 
    {
        printf_simple("AT_ENTRY: 0x%x\n", (unsigned int)g_auxv_entry);
    }
    if (g_auxv_phdr) 
    {
        printf_simple("AT_PHDR: 0x%x\n", (unsigned int)g_auxv_phdr);
    }
    if (g_auxv_phent) 
    {
        printf_simple("AT_PHENT: %d\n", (int)g_auxv_phent);
    }
    if (g_auxv_phnum) 
    {
        printf_simple("AT_PHNUM: %d\n", (int)g_auxv_phnum);
    }
    if (g_auxv_pagesz) 
    {
        printf_simple("AT_PAGESZ: %d\n", (int)g_auxv_pagesz);
    }
    if (g_auxv_base) 
    {
        printf_simple("AT_BASE: 0x%x\n", (unsigned int)g_auxv_base);
    }
    if (g_auxv_platform) 
    {
        printf_simple("AT_PLATFORM: %s\n", g_auxv_platform);
    }
    if (g_auxv_execfn) 
    {
        printf_simple("AT_EXECFN: %s\n", g_auxv_execfn);
    }
    
    printf_simple("==============================\n");
}

/**
 * 从栈指针解析参数
 * 栈布局：
 * [sp]: argc (8字节)
 * [sp+8]: argv[0]
 * [sp+16]: argv[1]
 * ...
 * [sp+8*argc]: NULL (argv 结束)
 * [sp+8*(argc+1)]: envp[0]
 * ...
 * [sp+8*(argc+1+envp_count)]: NULL (envp 结束)
 * [sp+8*(argc+2+envp_count)]: auxv[0].tag
 * [sp+8*(argc+2+envp_count)+8]: auxv[0].value
 * ...
 * auxv 以 AT_NULL (tag=0, value=0) 结束
 */
static void parse_stack(void *stack_ptr, int *argc, char ***argv, char ***envp, uint64_t **auxv)
{
    uint64_t *sp = (uint64_t *)stack_ptr;
    
    // 读取 argc
    *argc = (int)sp[0];
    
    // 计算 argv 地址（跳过 argc）
    *argv = (char **)(sp + 1);
    
    // 计算 envp 地址（跳过 argv 数组，argc+1 个元素包括 NULL）
    *envp = (char **)(sp + 1 + (*argc) + 1);
    
    // 找到 envp 的结束位置（NULL）
    char **env = *envp;
    while (*env != NULL)
    {
        env++;
    }
    
    // auxv 在 envp 结束后的下一个位置
    *auxv = (uint64_t *)(env + 1);
}

__attribute__((visibility("hidden")))
int main(void *stack_pointer)
{
    int argc;
    char **argv;
    char **envp;
    uint64_t *auxv;
    
    // 从栈指针解析所有参数
    parse_stack(stack_pointer, &argc, &argv, &envp, &auxv);
    
    // 保存环境变量指针
    g_environ = envp;
    
    // 使用 printf_simple 进行初始化阶段的打印
    printf_simple("=== Dynamic Linker Starting ===\n");
    printf_simple("argc: %d\n", argc);
    
    // 解析辅助向量 (auxv)
    parse_auxv(auxv);
    
    // 解析环境变量
    printf_simple("\nParsing environment variables...\n");
    print_all_env();
    
    // 解析重要的环境变量
    char *ld_library_path = get_env("LD_LIBRARY_PATH");
    if (ld_library_path) 
    {
        printf_simple("LD_LIBRARY_PATH=%s\n", ld_library_path);
    } else 
    {
        printf_simple("LD_LIBRARY_PATH not set\n");
    }
    
    char *ld_preload = get_env("LD_PRELOAD");
    if (ld_preload) 
    {
        printf_simple("LD_PRELOAD=%s\n", ld_preload);
    }
    
    char *path = get_env("PATH");
    if (path) 
    {
        printf_simple("PATH=%s\n", path);
    }
    
    printf_simple("\nDynamic Linker initialization complete.\n");
    
    // TODO: 解析参数
    // TODO: 加载动态库
    // TODO: 初始化动态库
    // TODO: 执行动态库（使用 g_auxv_entry）
    
    return 0;
}