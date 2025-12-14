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
static uint64_t g_auxv_phdr = 0;       // AT_PHDR: 可执行文件的程序头表在内存中的虚拟地址（真实地址，可直接使用）
static uint64_t g_auxv_phent = 0;      // AT_PHENT: 程序头表项大小
static uint64_t g_auxv_phnum = 0;      // AT_PHNUM: 程序头表项数量
static uint64_t g_auxv_pagesz = 0;     // AT_PAGESZ: 页大小
static uint64_t g_auxv_base = 0;       // AT_BASE: 动态链接器基址
static const char *g_auxv_platform = NULL;  // AT_PLATFORM: 平台字符串
static const char *g_auxv_execfn = NULL;     // AT_EXECFN: 可执行文件路径

// ELF 文件结构定义
#define EI_NIDENT 16
#define PT_LOAD 1
#define PT_DYNAMIC 2

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

// 动态段条目
typedef struct {
    int64_t d_tag;
    uint64_t d_val;
} Elf64_Dyn;

// 动态段标签 (DT_*)
#define DT_NULL     0
#define DT_NEEDED   1
#define DT_PLTRELSZ 2
#define DT_PLTGOT   3
#define DT_HASH     4
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_STRSZ    10
#define DT_SYMENT   11
#define DT_INIT     12
#define DT_FINI     13
#define DT_SONAME   14
#define DT_RPATH    15
#define DT_SYMBOLIC 16
#define DT_REL      17
#define DT_RELSZ    18
#define DT_RELENT   19
#define DT_PLTREL   20
#define DT_DEBUG    21
#define DT_TEXTREL  22
#define DT_JMPREL   23
#define DT_BIND_NOW 24
#define DT_INIT_ARRAY    25
#define DT_FINI_ARRAY    26
#define DT_INIT_ARRAYSZ  27
#define DT_FINI_ARRAYSZ  28
#define DT_RUNPATH       29
#define DT_FLAGS         30
#define DT_PREINIT_ARRAY 32
#define DT_PREINIT_ARRAYSZ 33

// 重定位条目 (AArch64)
typedef struct {
    uint64_t r_offset;
    uint32_t r_info;
    int64_t  r_addend;  // 使用int64_t代替int32_t
} Elf64_Rela;

// AArch64使用32位r_info，高24位是符号索引，低8位是类型
#define ELF64_R_SYM(i)    (((uint64_t)(i)) >> 8)
#define ELF64_R_TYPE(i)   ((i) & 0xff)
#define ELF64_R_INFO(s,t) (((s) << 8) + ((t) & 0xff))

// AArch64 重定位类型
#define R_AARCH64_NONE     0
#define R_AARCH64_ABS64    257
#define R_AARCH64_GLOB_DAT 1025
#define R_AARCH64_JUMP_SLOT 1026
#define R_AARCH64_RELATIVE 1027

// 符号表条目
typedef struct {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

#define ELF64_ST_BIND(i)  ((i) >> 4)
#define ELF64_ST_TYPE(i)  ((i) & 0xf)
#define ELF64_ST_INFO(b,t) (((b) << 4) + ((t) & 0xf))

#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

// 自举状态标志
static int g_bootstrap_done = 0;


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
    g_auxv_entry = get_auxv(auxv, AT_ENTRY); // 可执行文件入口点
    g_auxv_phdr = get_auxv(auxv, AT_PHDR); // 程序头表地址
    g_auxv_phent = get_auxv(auxv, AT_PHENT); // 程序头表项大小
    g_auxv_phnum = get_auxv(auxv, AT_PHNUM); // 程序头表项数量
    g_auxv_pagesz = get_auxv(auxv, AT_PAGESZ); // 页大小
    g_auxv_base = get_auxv(auxv, AT_BASE); // 动态链接器基址
    
    uint64_t platform_ptr = get_auxv(auxv, AT_PLATFORM); // 平台字符串指针 
    if (platform_ptr) 
    {
        g_auxv_platform = (const char *)platform_ptr;
    }
    
    uint64_t execfn_ptr = get_auxv(auxv, AT_EXECFN); // 可执行文件路径字符串指针
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

/**
 * 简单的内存复制函数（不使用 PLT）
 */
static void *memcpy_simple(void *dest, const void *src, size_t n)
{
    char *d = (char *)dest;
    const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

/**
 * 简单的内存设置函数（不使用 PLT）
 */
static void *memset_simple(void *s, int c, size_t n)
{
    char *p = (char *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (char)c;
    }
    return s;
}

/**
 * 从动态段中获取指定标签的值
 */
static uint64_t get_dynamic_tag(Elf64_Dyn *dynamic, int64_t tag)
{
    if (!dynamic) {
        return 0;
    }
    
    // 安全检查：确保地址在合理范围内
    if ((uintptr_t)dynamic < g_auxv_base || 
        (uintptr_t)dynamic > g_auxv_base + 0x200000) {
        printf_simple("Warning: dynamic pointer out of range: 0x%x\n", 
                     (unsigned int)(uintptr_t)dynamic);
        return 0;
    }
    
    // 限制搜索范围，避免无限循环
    for (int i = 0; i < 1000; i++) {
        // 检查当前条目地址是否有效
        Elf64_Dyn *d = &dynamic[i];
        if ((uintptr_t)d < g_auxv_base || 
            (uintptr_t)d > g_auxv_base + 0x200000) {
            break;
        }
        
        if (d->d_tag == DT_NULL) {
            break;
        }
        
        if (d->d_tag == tag) {
            return d->d_val;
        }
    }
    
    return 0;
}

/**
 * 解析符号名称
 */
static const char *get_symbol_name(uint64_t strtab, uint32_t st_name)
{
    if (!strtab || !st_name) {
        return NULL;
    }
    return (const char *)(strtab + st_name);
}

/**
 * 查找符号
 */
static Elf64_Sym *find_symbol(const char *name, Elf64_Sym *symtab, 
                               const char *strtab, uint32_t symnum)
{
    if (!name || !symtab || !strtab) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < symnum; i++) {
        const char *symname = get_symbol_name((uint64_t)strtab, symtab[i].st_name);
        if (symname && strncmp_simple(symname, name, 256) == 0) {
            return &symtab[i];
        }
    }
    
    return NULL;
}

/**
 * 通过LOAD段映射关系计算符号的实际内存地址
 * 
 * @param sym_entry 符号表条目
 * @param base_addr 加载基址
 * @param phdr 程序头表
 * @param phnum 程序头表项数量
 * @return 符号的实际内存地址，如果找不到返回0
 */
static uint64_t calculate_symbol_address(Elf64_Sym *sym_entry, uint64_t base_addr,
                                         Elf64_Phdr *phdr, uint16_t phnum)
{
    if (!sym_entry || !phdr) {
        return 0;
    }
    
    // 方法1：通过包含符号的LOAD段计算（最准确）
    uint64_t sym_addr = 0;
    for (uint16_t i = 0; i < phnum; i++) {
        Elf64_Phdr *load = &phdr[i];
        if (load->p_type == PT_LOAD) {
            // 检查符号的虚拟地址是否在这个LOAD段中
            if (sym_entry->st_value >= load->p_vaddr && 
                sym_entry->st_value < load->p_vaddr + load->p_memsz) {
                // 使用该LOAD段的映射关系
                uint64_t offset_in_segment = sym_entry->st_value - load->p_vaddr;
                uint64_t file_offset = load->p_offset + offset_in_segment;
                sym_addr = base_addr + file_offset;
                break;
            }
        }
    }
    
    return sym_addr;
}

/**
 * 通过LOAD段映射关系计算虚拟地址对应的实际内存地址
 * 
 * @param vaddr 虚拟地址
 * @param base_addr 加载基址
 * @param phdr 程序头表
 * @param phnum 程序头表项数量
 * @return 实际内存地址，如果找不到返回0
 */
static uint64_t calculate_address_from_vaddr(uint64_t vaddr, uint64_t base_addr,
                                             Elf64_Phdr *phdr, uint16_t phnum)
{
    if (!phdr) {
        return 0;
    }
    
    // 查找包含该虚拟地址的LOAD段
    printf_simple("calculate_address_from_vaddr: searching for vaddr=0x%x in %d LOAD segments\n",
                 (unsigned int)vaddr, phnum);
    for (uint16_t i = 0; i < phnum; i++) {
        Elf64_Phdr *load = &phdr[i];
        if (load->p_type == PT_LOAD) {
            uint64_t load_end = load->p_vaddr + load->p_memsz;
            printf_simple("  LOAD[%d]: vaddr=0x%x-0x%x, offset=0x%x, flags=0x%x\n",
                         i, (unsigned int)load->p_vaddr, (unsigned int)load_end,
                         (unsigned int)load->p_offset, (unsigned int)load->p_flags);
            // 检查虚拟地址是否在这个LOAD段中
            if (vaddr >= load->p_vaddr && vaddr < load_end) {
                // 通用地址计算：适用于自举和其他ELF文件
                // base_addr 的含义：
                //   - 自举：base_addr 是文件偏移0的地址（整个文件被连续加载）
                //   - 其他ELF文件：base_addr 是第一个LOAD段的加载地址
                // 
                // 对于两种情况，计算公式都是：
                //   实际地址 = base_addr + (vaddr - first_load_vaddr)
                // 
                // 但是，我们需要找到第一个LOAD段来确定 first_load_vaddr
                // 如果当前LOAD段就是第一个LOAD段，则：
                //   实际地址 = base_addr + (vaddr - load->p_vaddr)
                // 
                // 为了通用性，我们使用LOAD段的映射关系：
                //   实际地址 = base_addr + (vaddr - first_load_vaddr)
                // 
                // 首先找到第一个LOAD段
                uint64_t first_load_vaddr = 0;
                for (uint16_t j = 0; j < phnum; j++) {
                    if (phdr[j].p_type == PT_LOAD) {
                        first_load_vaddr = phdr[j].p_vaddr;
                        break;
                    }
                }
                
                // 计算实际地址
                uint64_t result = base_addr + (vaddr - first_load_vaddr);
                printf_simple("Found LOAD segment: vaddr=0x%x, p_vaddr=0x%x, first_load_vaddr=0x%x, result=0x%x\n",
                             (unsigned int)vaddr, (unsigned int)load->p_vaddr, 
                             (unsigned int)first_load_vaddr, (unsigned int)result);
                
                // 注意：对于重定位目标地址，即使段不可写，我们也需要计算地址
                // （虽然通常重定位目标应该在可写段中，但这里我们只计算地址，不检查权限）
                return result;
            }
        }
    }
    
    return 0;
}

/**
 * 处理重定位条目
 * 使用LOAD段映射关系进行地址转换，以兼容各种情况
 */
static void process_relocation(Elf64_Rela *rela, uint64_t base_addr,
                                Elf64_Sym *symtab, const char *strtab,
                                uint32_t symnum, uint64_t *got,
                                Elf64_Ehdr *ehdr, Elf64_Phdr *phdr)
{
    if (!rela) {
        return;
    }
    
    // 安全检查：确保rela指针有效
    if ((uintptr_t)rela < base_addr || (uintptr_t)rela > base_addr + 0x200000) {
        printf_simple("Error: rela pointer out of range: 0x%x\n", (unsigned int)(uintptr_t)rela);
        return;
    }
    
    uint32_t type = ELF64_R_TYPE(rela->r_info);
    uint64_t sym = ELF64_R_SYM(rela->r_info);
    uint64_t offset = rela->r_offset;  // r_offset是虚拟地址（vaddr）
    
    // 计算目标地址：使用通用的LOAD段映射关系
    // 适用于自举和其他ELF文件
    // 
    // base_addr 的含义：
    //   - 自举：base_addr 是文件偏移0的地址（整个文件被连续加载）
    //   - 其他ELF文件：base_addr 是第一个LOAD段的加载地址
    // 
    // 通用计算公式：实际地址 = base_addr + (vaddr - first_load_vaddr)
    uint64_t *addr = NULL;
    
    if (!ehdr || !phdr) {
        // 回退：直接使用 base_addr + offset（假设first_load_vaddr=0）
        addr = (uint64_t *)(base_addr + offset);
    } else {
        // 首先尝试使用通用的 calculate_address_from_vaddr
        // 这适用于所有ELF文件（自举和其他共享库）
        uint64_t target_addr = calculate_address_from_vaddr(offset, base_addr, phdr, ehdr->e_phnum);
        
        if (target_addr != 0) {
            // 成功找到LOAD段，使用计算结果
            addr = (uint64_t *)target_addr;
        } else {
            // 回退：如果 calculate_address_from_vaddr 失败，使用简化方法
            // 找到第一个LOAD段，计算文件偏移
            uint64_t first_load_vaddr = 0;
            for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
                Elf64_Phdr *load = &phdr[i];
                if (load->p_type == PT_LOAD) {
                    first_load_vaddr = load->p_vaddr;
                    break;
                }
            }
            // 通用公式：实际地址 = base_addr + (vaddr - first_load_vaddr)
            addr = (uint64_t *)(base_addr + offset - first_load_vaddr);
        }
    }
    
    // 安全检查：确保地址在合理范围内
    if (!addr) {
        printf_simple("Error: addr is NULL\n");
        return;
    }
    if ((uintptr_t)addr < base_addr || (uintptr_t)addr > base_addr + 0x200000) {
        printf_simple("Warning: relocation target address out of range: 0x%x\n",
                     (unsigned int)(uintptr_t)addr);
        return;
    }
    
    switch (type) {
        case 3:  // R_AARCH64_RELATIVE (1027)的低8位是3
            // 相对重定位：addr = base_addr + addend
            // addend 是文件偏移，需要转换为实际地址
            // 注意：R_AARCH64_RELATIVE = 1027 (0x403)，ELF64_R_TYPE(0x403) = 3
            *addr = base_addr + rela->r_addend;
            break;
            
        case 1:  // R_AARCH64_ABS64 (257) 和 R_AARCH64_GLOB_DAT (1025) 的低8位都是1
            // 需要根据上下文区分：
            // - R_AARCH64_ABS64: 用于直接重定位（非GOT/PLT）
            // - R_AARCH64_GLOB_DAT: 用于GOT条目
            // 这里统一处理为需要查找符号的情况
            // 计算公式：S + A，其中 S = 符号地址，A = r_addend
            if (sym < symnum && symtab) {
                Elf64_Sym *sym_entry = &symtab[sym];
                // 通过LOAD段映射关系计算符号地址
                uint64_t sym_addr = 0;
                if (ehdr && phdr) {
                    sym_addr = calculate_symbol_address(sym_entry, base_addr, phdr, ehdr->e_phnum);
                }
                
                // 如果通过LOAD段找不到，使用简化方法（回退）
                if (sym_addr == 0) {
                    sym_addr = base_addr + sym_entry->st_value;
                }
                
                *addr = sym_addr + rela->r_addend;
            } else {
                printf_simple("Warning: invalid symbol index %d for relocation\n", (unsigned int)sym);
            }
            break;
            
        case 2:  // R_AARCH64_JUMP_SLOT (1026)的低8位是2
            // 跳转槽（PLT条目）：需要查找符号
            // 计算公式：S + A，其中 S = 符号地址，A = r_addend
            if (sym < symnum && symtab) {
                Elf64_Sym *sym_entry = &symtab[sym];
                // 通过LOAD段映射关系计算符号地址
                uint64_t sym_addr = 0;
                if (ehdr && phdr) {
                    sym_addr = calculate_symbol_address(sym_entry, base_addr, phdr, ehdr->e_phnum);
                }
                
                // 如果通过LOAD段找不到，使用简化方法（回退）
                if (sym_addr == 0) {
                    sym_addr = base_addr + sym_entry->st_value;
                }
                
                *addr = sym_addr + rela->r_addend;
            } else {
                // 如果没有符号，使用 base_addr + addend
                *addr = base_addr + rela->r_addend;
            }
            break;
            
        case 0:  // R_AARCH64_NONE (0)
            // 无操作重定位，忽略
            break;
            
        default:
            // 未知重定位类型，尝试作为相对重定位处理
            printf_simple("Warning: unknown relocation type %d, treating as relative\n", type);
            *addr = base_addr + rela->r_addend;
            break;
    }
}

/**
 * 自举动态链接器自身
 * 返回0表示成功，非0表示失败
 */
static int bootstrap_self(void)
{
    if (g_bootstrap_done)
    {
        return 0;  // 已经完成自举
    }
    
    if (!g_auxv_base)
    {
        printf_simple("Error: AT_BASE not found in auxv\n");
        return 1;
    }
    
    printf_simple("Dynamic linker base: 0x%x\n", (unsigned int)g_auxv_base);
    
    // 获取ELF头
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)g_auxv_base;
    
    // 验证ELF魔数
    if (ehdr->e_ident[0] != 0x7f || 
        ehdr->e_ident[1] != 'E' || 
        ehdr->e_ident[2] != 'L' || 
        ehdr->e_ident[3] != 'F')
    {
        printf_simple("Error: Invalid ELF magic\n");
        return 1;
    }
    
    printf_simple("ELF header validated\n");
    
    // 计算程序头表地址
    Elf64_Phdr *phdr = (Elf64_Phdr *)(g_auxv_base + ehdr->e_phoff);
    printf_simple("Program header table at: 0x%x (g_auxv_base=0x%x, e_phoff=0x%x)\n",
                 (unsigned int)(uintptr_t)phdr, (unsigned int)g_auxv_base, (unsigned int)ehdr->e_phoff);
    
    // 查找动态段
    // 程序头表地址计算：
    // 
    // 在动态链接器自举时，内核将整个ELF文件连续加载到内存中。
    // g_auxv_base是ELF头在内存中的位置，对应文件偏移0。
    // 由于整个文件被连续加载，文件偏移直接对应内存偏移。
    // 
    // 因此，程序头表的地址可以直接计算为：
    //   phdr = g_auxv_base + e_phoff
    // 
    // 这个简单方法总是正确的，因为：
    // 1. 整个文件被连续加载，文件布局 = 内存布局
    // 2. g_auxv_base对应文件偏移0（ELF头位置）
    // 3. e_phoff是程序头表在文件中的偏移
    // 4. 因此 g_auxv_base + e_phoff 直接给出程序头表在内存中的位置
    // 
    // 注意：LOAD段方法（通过p_vaddr和p_offset计算）主要用于加载其他共享库时，
    // 因为那些库可能只加载LOAD段，而不是整个文件。但在自举时，整个文件都在内存中。
    
    Elf64_Dyn *dynamic = NULL;
    
    // 查找第一个LOAD段，用于后续的地址转换（动态段、符号表等）
    uint64_t first_load_vaddr = 0;
    uint64_t first_load_offset = 0;
    for (uint16_t i = 0; i < ehdr->e_phnum; i++)
    {
        Elf64_Phdr *p = &phdr[i];
        if (p->p_type == PT_LOAD) {
            first_load_vaddr = p->p_vaddr;
            first_load_offset = p->p_offset;
            printf_simple("First LOAD segment: vaddr=0x%x, offset=0x%x\n", 
                         (unsigned int)first_load_vaddr, (unsigned int)first_load_offset);
            break;
        }
    }
    
    // 遍历程序头表查找动态段
    for (uint16_t i = 0; i < ehdr->e_phnum; i++)
    {
        Elf64_Phdr *p = &phdr[i];
        if (p->p_type == PT_DYNAMIC) {
            // 动态段的地址计算：
            // 共享库按照LOAD段的虚拟地址加载，g_auxv_base是第一个LOAD段的加载地址。
            // 如果第一个LOAD段的虚拟地址是0，则：
            //   memaddr = g_auxv_base + p_vaddr
            // 
            // 如果第一个LOAD段的虚拟地址不是0，需要找到包含动态段的LOAD段：
            //   找到包含p_vaddr的LOAD段，使用该段的映射关系计算
            
            // 计算动态段的内存地址
            // 对于共享库，g_auxv_base是第一个LOAD段的加载地址
            // 如果第一个LOAD段的虚拟地址是0，则所有虚拟地址都可以直接加上g_auxv_base
            uint64_t dynamic_memaddr = 0;
            
            if (first_load_vaddr == 0) {
                // 最简单的情况：第一个LOAD段的虚拟地址是0
                // 直接使用：memaddr = g_auxv_base + p_vaddr
                dynamic_memaddr = g_auxv_base + p->p_vaddr;
                printf_simple("Using simple calculation: g_auxv_base + p_vaddr = 0x%x + 0x%x = 0x%x\n",
                             (unsigned int)g_auxv_base, (unsigned int)p->p_vaddr, (unsigned int)dynamic_memaddr);
            } else {
                // 第一个LOAD段的虚拟地址不是0的情况
                // 需要找到包含动态段的LOAD段，使用该段的映射关系
                Elf64_Phdr *containing_load = NULL;
                for (uint16_t j = 0; j < ehdr->e_phnum; j++) {
                    Elf64_Phdr *load = &phdr[j];
                    if (load->p_type == PT_LOAD) {
                        if (p->p_vaddr >= load->p_vaddr && 
                            p->p_vaddr < load->p_vaddr + load->p_memsz) {
                            containing_load = load;
                            break;
                        }
                    }
                }
                
                if (containing_load) {
                    // 计算动态段在包含它的LOAD段中的偏移
                    uint64_t offset_in_segment = p->p_vaddr - containing_load->p_vaddr;
                    
                    // 计算包含动态段的LOAD段的加载地址
                    // 该LOAD段的加载地址 = g_auxv_base + (load->p_vaddr - first_load_vaddr)
                    uint64_t load_base_addr = g_auxv_base + (containing_load->p_vaddr - first_load_vaddr);
                    
                    // 动态段的内存地址 = LOAD段加载地址 + 段内偏移
                    dynamic_memaddr = load_base_addr + offset_in_segment;
                    
                    printf_simple("Using LOAD segment mapping: load_base=0x%x, offset_in_segment=0x%x, memaddr=0x%x\n",
                                 (unsigned int)load_base_addr, 
                                 (unsigned int)offset_in_segment,
                                 (unsigned int)dynamic_memaddr);
                } else {
                    // 如果没找到包含动态段的LOAD段，使用简化方法
                    printf_simple("Warning: Cannot find LOAD segment containing dynamic segment, using fallback\n");
                    // 简化计算：假设虚拟地址空间连续
                    dynamic_memaddr = g_auxv_base + (p->p_vaddr - first_load_vaddr);
                }
            }
            
            dynamic = (Elf64_Dyn *)dynamic_memaddr;
            printf_simple("Found dynamic segment: vaddr=0x%x, offset=0x%x, memaddr=0x%x\n", 
                         (unsigned int)p->p_vaddr, 
                         (unsigned int)p->p_offset,
                         (unsigned int)(uintptr_t)dynamic);
            break;
        }
    }
    
    if (!dynamic) {
        printf_simple("Error: Dynamic segment not found\n");
        return 1;
    }
    
    // 验证动态段地址是否有效（简单检查：在合理范围内）
    if ((uintptr_t)dynamic < g_auxv_base || 
        (uintptr_t)dynamic > g_auxv_base + 0x1000000) {
        printf_simple("Error: Dynamic segment address out of range: 0x%x\n", 
                     (unsigned int)(uintptr_t)dynamic);
        return 1;
    }
    
    printf_simple("Accessing dynamic segment at 0x%x...\n", (unsigned int)(uintptr_t)dynamic);
    
    // 尝试读取第一个动态条目来验证地址是否有效
    // 使用volatile避免编译器优化
    volatile Elf64_Dyn *test_dyn = dynamic;
    volatile int64_t test_tag = test_dyn->d_tag;
    printf_simple("First dynamic tag: 0x%x\n", (unsigned int)test_tag);
    
    // 获取动态段中的关键信息
    uint64_t strtab_addr = get_dynamic_tag((Elf64_Dyn *)dynamic, DT_STRTAB);
    uint64_t symtab_addr = get_dynamic_tag((Elf64_Dyn *)dynamic, DT_SYMTAB);
    uint64_t rela_addr = get_dynamic_tag(dynamic, DT_RELA);
    uint64_t rela_size = get_dynamic_tag(dynamic, DT_RELASZ);
    uint64_t rela_ent = get_dynamic_tag(dynamic, DT_RELAENT);
    uint64_t pltrel_addr = get_dynamic_tag(dynamic, DT_JMPREL);
    uint64_t pltrel_size = get_dynamic_tag(dynamic, DT_PLTRELSZ);
    uint64_t got_addr = get_dynamic_tag(dynamic, DT_PLTGOT);
    
    if (!strtab_addr || !symtab_addr) {
        printf_simple("Error: Missing required dynamic tags (strtab=0x%x, symtab=0x%x)\n",
                     (unsigned int)strtab_addr, (unsigned int)symtab_addr);
        return 1;
    }
    
    // 计算strtab和symtab的实际内存地址
    // 在动态链接器自举时，整个文件被连续加载，文件偏移直接对应内存偏移。
    // strtab_addr和symtab_addr是虚拟地址，需要转换为文件偏移，然后使用文件偏移。
    // 
    // 转换方法：
    // 1. 找到包含strtab/symtab的LOAD段
    // 2. 计算在LOAD段中的相对偏移：offset_in_segment = vaddr - load->p_vaddr
    // 3. 计算文件偏移：file_offset = load->p_offset + offset_in_segment
    // 4. 计算实际地址：memaddr = g_auxv_base + file_offset
    
    uint64_t strtab_memaddr = 0;
    uint64_t symtab_memaddr = 0;
    
    // 查找包含这些地址的LOAD段
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *load = &phdr[i];
        if (load->p_type == PT_LOAD) {
            // 计算strtab地址
            if (strtab_memaddr == 0 && strtab_addr >= load->p_vaddr && 
                strtab_addr < load->p_vaddr + load->p_memsz) {
                // 使用该LOAD段的映射关系
                uint64_t offset_in_segment = strtab_addr - load->p_vaddr;
                uint64_t file_offset = load->p_offset + offset_in_segment;
                strtab_memaddr = g_auxv_base + file_offset;
                //printf_simple("1111strtab: vaddr=0x%x, memaddr=0x%x\n", 
                             //(unsigned int)strtab_addr, (unsigned int)strtab_memaddr);
                //printf_simple("11111p_vaddr: 0x%x, p_offset: 0x%x\n", (unsigned int)load->p_vaddr, (unsigned int)load->p_offset);
            }
            // 计算symtab地址
            if (symtab_memaddr == 0 && symtab_addr >= load->p_vaddr && 
                symtab_addr < load->p_vaddr + load->p_memsz) {
                // 使用该LOAD段的映射关系
                uint64_t offset_in_segment = symtab_addr - load->p_vaddr;
                uint64_t file_offset = load->p_offset + offset_in_segment;
                symtab_memaddr = g_auxv_base + file_offset;
                //printf_simple("symtab: vaddr=0x%x, memaddr=0x%x, p_vaddr=0x%x, p_offset=0x%x\n", 
                             //(unsigned int)symtab_addr, (unsigned int)symtab_memaddr, 
                             //(unsigned int)load->p_vaddr, (unsigned int)load->p_offset);
                //printf_simple("22222symtab: vaddr=0x%x, memaddr=0x%x\n", 
                             //(unsigned int)symtab_addr, (unsigned int)symtab_memaddr);
                //printf_simple("22222p_vaddr: 0x%x, p_offset: 0x%x\n", (unsigned int)load->p_vaddr, (unsigned int)load->p_offset);
            }
            if (strtab_memaddr != 0 && symtab_memaddr != 0) {
                break;
            }
        }
    }
    
    // 如果没找到，使用简化方法（假设第一个LOAD段的p_offset=0，且虚拟地址空间连续）
    // 这适用于大多数情况（PIE/共享库）
    if (strtab_memaddr == 0) {
        if (first_load_offset == 0) {
            // 如果第一个LOAD段的p_offset=0，可以使用简化公式
            strtab_memaddr = g_auxv_base + strtab_addr - first_load_vaddr;
        } else {
            printf_simple("Warning: Cannot determine strtab address (first_load_offset != 0)\n");
        }
    }
    if (symtab_memaddr == 0) {
        if (first_load_offset == 0) {
            // 如果第一个LOAD段的p_offset=0，可以使用简化公式
            symtab_memaddr = g_auxv_base + symtab_addr - first_load_vaddr;
        } else {
            printf_simple("Warning: Cannot determine symtab address (first_load_offset != 0)\n");
        }
    }
    
    // 最后的回退（不应该到达这里）
    if (strtab_memaddr == 0) {
        printf_simple("Error: Cannot determine strtab address\n");
        return 1;
    }
    if (symtab_memaddr == 0) {
        printf_simple("Error: Cannot determine symtab address\n");
        return 1;
    }
    
    printf_simple("strtab: vaddr=0x%x, memaddr=0x%x\n", 
                  (unsigned int)strtab_addr, (unsigned int)strtab_memaddr);
    printf_simple("symtab: vaddr=0x%x, memaddr=0x%x\n", 
                  (unsigned int)symtab_addr, (unsigned int)symtab_memaddr);
    
    // 计算符号表大小
    // 简化方法：从hash表获取，或者通过strtab大小估算
    // 这里我们使用一个较大的值，实际应该从hash表获取
    uint32_t symnum = 2000;  // 临时值，实际应该从hash表获取
    
    const char *strtab = (const char *)strtab_memaddr;
    Elf64_Sym *symtab = (Elf64_Sym *)symtab_memaddr;
    
    // 验证地址有效性（简单检查）
    if ((uintptr_t)strtab < g_auxv_base || (uintptr_t)symtab < g_auxv_base) {
        printf_simple("Warning: strtab or symtab address seems invalid\n");
    }
    
    // 处理重定位表
    if (rela_addr && rela_size && rela_ent) {
        uint32_t rela_count = rela_size / rela_ent;
        
        // 计算rela的实际内存地址：使用LOAD段映射关系
        uint64_t rela_memaddr = calculate_address_from_vaddr(rela_addr, g_auxv_base, phdr, ehdr->e_phnum);
        if (rela_memaddr == 0) {
            // 回退：使用简化方法
            if (first_load_offset == 0) {
                rela_memaddr = g_auxv_base + rela_addr - first_load_vaddr;
            } else {
                rela_memaddr = g_auxv_base + rela_addr;
            }
        }
        
        Elf64_Rela *rela = (Elf64_Rela *)rela_memaddr;
        
        printf_simple("Processing %d relocations at 0x%x...\n", 
                     rela_count, (unsigned int)rela_memaddr);
        
        for (uint32_t i = 0; i < rela_count; i++) {
            // 安全检查：确保rela地址有效
            if ((uintptr_t)&rela[i] < g_auxv_base || 
                (uintptr_t)&rela[i] > g_auxv_base + 0x200000) {
                printf_simple("Warning: rela[%d] address out of range: 0x%x\n", 
                             i, (unsigned int)(uintptr_t)&rela[i]);
                break;
            }
            // 先读取rela[i]的字段，避免在printf中重复访问
            uint32_t rel_type = ELF64_R_TYPE(rela[i].r_info);
            uint64_t rel_offset = rela[i].r_offset;
            // printf_simple("Processing relocation[%d]: type=%d, offset=0x%x\n",
            //              i, rel_type, (unsigned int)rel_offset);
            
            // 处理重定位
            process_relocation(&rela[i], g_auxv_base, symtab, strtab, symnum, (uint64_t *)got_addr, ehdr, phdr);
        }
        
        printf_simple("Relocations processed\n");
    }
    
    // 处理PLT重定位表
    if (pltrel_addr && pltrel_size && rela_ent) {
        uint32_t pltrel_count = pltrel_size / rela_ent;
        
        // 计算pltrel的实际内存地址：使用LOAD段映射关系
        uint64_t pltrel_memaddr = calculate_address_from_vaddr(pltrel_addr, g_auxv_base, phdr, ehdr->e_phnum);
        if (pltrel_memaddr == 0) {
            // 回退：使用简化方法
            if (first_load_offset == 0) {
                pltrel_memaddr = g_auxv_base + pltrel_addr - first_load_vaddr;
            } else {
                pltrel_memaddr = g_auxv_base + pltrel_addr;
            }
        }
        
        Elf64_Rela *pltrel = (Elf64_Rela *)pltrel_memaddr;
        
        printf_simple("Processing %d PLT relocations at 0x%x...\n", 
                     pltrel_count, (unsigned int)pltrel_memaddr);
        
        for (uint32_t i = 0; i < pltrel_count; i++) {
            process_relocation(&pltrel[i], g_auxv_base, symtab, strtab, symnum, (uint64_t *)got_addr, ehdr, phdr);
        }
        
        printf_simple("PLT relocations processed\n");
    }
    
    g_bootstrap_done = 1;
    return 0;
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
    
    // 自举动态链接器
    printf_simple("\n=== Bootstrap Dynamic Linker ===\n");
    if (bootstrap_self())
    {
        printf_simple("Bootstrap failed!\n");
        return 1;
    }
    printf_simple("Bootstrap completed successfully!\n");
    
    // 自举完成后，可以使用mini_lib的printf函数
    printf("\n=== Testing mini_lib printf ===\n");
    printf("This message is printed using mini_lib's printf function!\n");
    printf("Bootstrap verification: SUCCESS\n");
    printf("Dynamic linker base: 0x%x\n", (unsigned int)g_auxv_base);
    
    // TODO: 加载动态库
    // TODO: 初始化动态库
    // TODO: 执行动态库（使用 g_auxv_entry）
    
    return 0;
}