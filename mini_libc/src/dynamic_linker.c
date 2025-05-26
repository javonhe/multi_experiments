/**
 * dynamic_linker.c - aarch64平台动态链接器实现
 * 
 * 本文件实现了一个简单的动态链接器，用于加载和链接ELF格式的可执行文件和共享库。
 * 主要功能包括：
 * 1. 解析ELF文件头和程序头
 * 2. 加载可执行文件和共享库到内存
 * 3. 处理重定位表
 * 4. 解析和链接符号
 * 
 * 实现原理：
 * 1. 文件加载：
 *    - 使用mmap将ELF文件映射到内存
 *    - 根据程序头表(Program Headers)设置内存权限
 *    - 处理LOAD、DYNAMIC等段
 * 
 * 2. 符号解析：
 *    - 维护全局符号表
 *    - 按照依赖顺序解析符号
 *    - 处理未定义符号和弱符号
 * 
 * 3. 重定位处理：
 *    - 支持R_AARCH64_GLOB_DAT、R_AARCH64_JUMP_SLOT等重定位类型
 *    - 计算符号的实际地址
 *    - 修正GOT/PLT表项
 */

#include "mini_lib.h"

// ELF文件类型
#define ET_NONE     0       // 未知类型
#define ET_REL      1       // 可重定位文件
#define ET_EXEC     2       // 可执行文件
#define ET_DYN      3       // 共享目标文件
#define ET_CORE     4       // Core文件

// ELF机器类型
#define EM_AARCH64  183     // ARM 64位架构

// 程序头类型
#define PT_NULL     0       // 未使用
#define PT_LOAD     1       // 可加载段
#define PT_DYNAMIC  2       // 动态链接信息
#define PT_INTERP   3       // 程序解释器
#define PT_NOTE     4       // 辅助信息
#define PT_PHDR     6       // 程序头表

// 动态表项类型
#define DT_NULL     0       // 标记动态段结束
#define DT_NEEDED   1       // 需要的共享库
#define DT_PLTRELSZ 2       // PLT重定位项大小
#define DT_PLTGOT   3       // PLT/GOT地址
#define DT_HASH     4       // 符号哈希表
#define DT_STRTAB   5       // 字符串表
#define DT_SYMTAB   6       // 符号表
#define DT_RELA     7       // 重定位表
#define DT_RELASZ   8       // 重定位表大小
#define DT_RELAENT  9       // 重定位项大小
#define DT_STRSZ    10      // 字符串表大小
#define DT_SYMENT   11      // 符号表项大小
#define DT_INIT     12      // 初始化函数
#define DT_FINI     13      // 终止函数
#define DT_SONAME   14      // 共享库名称
#define DT_RPATH    15      // 运行时库搜索路径
#define DT_SYMBOLIC 16      // 改变符号解析顺序
#define DT_REL      17      // 重定位表
#define DT_RELSZ    18      // 重定位表大小
#define DT_RELENT   19      // 重定位项大小

// ELF符号绑定类型
#define STB_LOCAL   0       // 局部符号
#define STB_GLOBAL  1       // 全局符号
#define STB_WEAK    2       // 弱符号
#define STB_NUM     3       // 符号绑定数量

// ELF符号类型
#define STT_NOTYPE  0       // 未指定类型
#define STT_OBJECT  1       // 数据对象
#define STT_FUNC    2       // 函数入口点
#define STT_SECTION 3       // 节
#define STT_FILE    4       // 源文件名
#define STT_COMMON  5       // 未初始化的公共块
#define STT_TLS     6       // 线程局部存储

// 特殊节索引
#define SHN_UNDEF   0       // 未定义节
#define SHN_ABS     0xfff1  // 绝对值
#define SHN_COMMON  0xfff2  // 公共块

// AArch64重定位类型
#define R_AARCH64_NONE          0
#define R_AARCH64_GLOB_DAT      1025
#define R_AARCH64_JUMP_SLOT     1026
#define R_AARCH64_RELATIVE      1027

// ELF段权限标志
#define PF_X        0x1     // 可执行
#define PF_W        0x2     // 可写
#define PF_R        0x4     // 可读
#define PF_MASKOS   0x0ff00000  // OS特定
#define PF_MASKPROC 0xf0000000  // 处理器特定

// ELF头结构体定义
typedef struct 
{
    unsigned char e_ident[16];    // ELF标识
    uint16_t    e_type;          // 文件类型
    uint16_t    e_machine;       // 机器类型
    uint32_t    e_version;       // 文件版本
    uint64_t    e_entry;         // 入口点地址
    uint64_t    e_phoff;         // 程序头表偏移
    uint64_t    e_shoff;         // 节头表偏移
    uint32_t    e_flags;         // 处理器特定标志
    uint16_t    e_ehsize;        // ELF头大小
    uint16_t    e_phentsize;     // 程序头表项大小
    uint16_t    e_phnum;         // 程序头表项数量
    uint16_t    e_shentsize;     // 节头表项大小
    uint16_t    e_shnum;         // 节头表项数量
    uint16_t    e_shstrndx;      // 节名字符串表索引
} Elf64_Ehdr;

// 程序头结构体
typedef struct 
{
    uint32_t    p_type;          // 段类型
    uint32_t    p_flags;         // 段标志
    uint64_t    p_offset;        // 段在文件中的偏移
    uint64_t    p_vaddr;         // 段的虚拟地址
    uint64_t    p_paddr;         // 段的物理地址
    uint64_t    p_filesz;        // 段在文件中的大小
    uint64_t    p_memsz;         // 段在内存中的大小
    uint64_t    p_align;         // 段对齐
} Elf64_Phdr;

// 动态表项结构体
typedef struct 
{
    uint64_t    d_tag;           // 动态项类型
    union 
    {
        uint64_t d_val;          // 整数值
        uint64_t d_ptr;          // 地址值
    } d_un;
} Elf64_Dyn;

// 重定位项结构体
typedef struct 
{
    uint64_t    r_offset;        // 重定位位置
    uint64_t    r_info;          // 重定位类型和符号
    int64_t     r_addend;        // 常量加数
} Elf64_Rela;

// 符号表项结构体
typedef struct 
{
    uint32_t    st_name;         // 符号名称
    unsigned char st_info;        // 符号类型和绑定
    unsigned char st_other;       // 符号可见性
    uint16_t    st_shndx;        // 符号所在节
    uint64_t    st_value;        // 符号值
    uint64_t    st_size;         // 符号大小
} Elf64_Sym;

// 加载器上下文相关结构定义
typedef struct LoadedObject 
{
    char *path;              // 文件路径
    void *base;              // 加载基址
    uint64_t base_offset;    // 基址偏移量(实际基址 - 预期基址)
    Elf64_Dyn *dynamic;      // 动态段
    size_t size;             // 映射大小
    void *entry;             // 入口点
    struct LoadedObject *next;
} LoadedObject;

typedef struct SymbolEntry 
{
    const char *name;        // 符号名
    uint64_t value;          // 符号值
    struct LoadedObject *obj; // 所属对象
    struct SymbolEntry *next;
} SymbolEntry;

typedef struct 
{
    struct LoadedObject *loaded_objects;
    struct SymbolEntry *symbols;
    char *error;                 // 错误信息
    char *interp;               // 解释器路径
} LoaderContext;

static LoaderContext g_ctx = {0};

#define MAX_SEARCH_PATHS 32
#define MAX_PATH_LEN 256

// 动态库搜索路径结构
typedef struct 
{
    char *paths[MAX_SEARCH_PATHS];  // 搜索路径数组
    int count;                      // 当前路径数量
} SearchPaths;

static SearchPaths g_search_paths = {0};

// 文件访问权限检查
#define F_OK 0

// 全局变量存储环境变量
static char **g_environ = NULL;

/**
 * 检查文件是否存在
 * 
 * @param path: 文件路径
 * @return: 文件存在返回0，不存在返回-1
 */
static int file_exists(const char *path)
{
    int fd = open(path, O_RDONLY, 0);
    if (fd >= 0) 
    {
        close(fd);
        return 0;
    }
    return -1;
}

/**
 * 字符串比较函数(带长度限制)
 * 
 * @param s1: 第一个字符串
 * @param s2: 第二个字符串
 * @param n: 最大比较长度
 * @return: 
 *   - 如果s1 < s2，返回负值
 *   - 如果s1 = s2，返回0
 *   - 如果s1 > s2，返回正值
 */
static int strncmp(const char *s1, const char *s2, size_t n)
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
 * 查找字符在字符串中的位置
 * 
 * @param s: 要搜索的字符串
 * @param c: 要查找的字符
 * @return: 找到返回字符在字符串中的位置，否则返回NULL
 */
static char* strchr(const char *s, int c)
{
    while (*s && *s != (char)c)
    {
        s++;
    }
    
    return *s == (char)c ? (char*)s : NULL;
}

/**
 * 获取环境变量值
 * 
 * @param name: 环境变量名
 * @return: 环境变量值，不存在返回NULL
 */
static char* get_env(const char *name)
{
    if (!g_environ) 
    {
        return NULL;
    }
    
    size_t name_len = strlen(name);
    
    // 遍历环境变量
    for (char **env = g_environ; *env; env++) 
    {
        if (strncmp(*env, name, name_len) == 0 && (*env)[name_len] == '=') 
        {
            return *env + name_len + 1;
        }
    }
    
    return NULL;
}

/**
 * 安全的字符串复制
 * 
 * @param dest: 目标缓冲区
 * @param src: 源字符串
 * @param size: 缓冲区大小
 * @return: 目标字符串
 */
static char* safe_strncpy(char *dest, const char *src, size_t size)
{
    if (size == 0) 
    {
        return dest;
    }

    size_t i;
    for (i = 0; i < size - 1 && src[i]; i++) 
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return dest;
}

/**
 * 安全的格式化字符串
 * 
 * @param str: 目标缓冲区
 * @param size: 缓冲区大小
 * @param format: 格式字符串
 * @param ...: 可变参数
 * @return: 写入的字符数
 */
static int safe_snprintf(char *str, size_t size, const char *format, ...)
{
    if (size == 0) 
    {
        return 0;
    }

    // 使用临时缓冲区
    char temp[1024];
    va_list args;
    va_start(args, format);
    int len = vsprintf(temp, format, args);
    va_end(args);

    // 确保不超过目标缓冲区大小
    if (len >= size) 
    {
        len = size - 1;
    }

    // 复制到目标缓冲区
    memcpy(str, temp, len);
    str[len] = '\0';

    return len;
}

/**
 * 字符串分割
 * 
 * @param str: 要分割的字符串
 * @param delim: 分隔符
 * @param saveptr: 保存分割位置的指针
 * @return: 分割后的子字符串
 */
static char* strtok_r(char *str, const char *delim, char **saveptr)
{
    if (!str) 
    {
        str = *saveptr;
    }
    
    if (!str) 
    {
        return NULL;
    }
    
    // 跳过开头的分隔符
    while (*str && strchr(delim, *str)) 
    {
        str++;
    }
    
    if (!*str) 
    {
        *saveptr = NULL;
        return NULL;
    }
    
    // 找到下一个分隔符
    char *end = str;
    while (*end && !strchr(delim, *end)) 
    {
        end++;
    }
    
    if (*end) 
    {
        *end = '\0';
        *saveptr = end + 1;
    }
    else 
    {
        *saveptr = NULL;
    }
    
    return str;
}

/**
 * 修改内存区域的访问权限
 * 用于设置内存映射区域的读/写/执行权限
 * 
 * @param addr: 要修改权限的内存区域起始地址
 * @param len: 要修改权限的内存区域长度
 * @param prot: 新的访问权限，可以是以下值的组合:
 *             PROT_NONE  - 不可访问
 *             PROT_READ  - 可读
 *             PROT_WRITE - 可写
 *             PROT_EXEC  - 可执行
 * @return: 成功返回0，失败返回-1
 */
int mprotect(void *addr, size_t len, int prot)
{
    register long x8 asm("x8") = 226;  // mprotect系统调用号
    register long x0 asm("x0") = (long)addr;
    register long x1 asm("x1") = len;
    register long x2 asm("x2") = prot;
    
    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory", "cc"
    );
    
    if (x0 < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * 设置错误信息
 * 用于在发生错误时记录错误原因，便于调试和错误处理
 * 
 * @param msg: 错误信息字符串
 */
static void set_error(const char *msg) 
{
    g_ctx.error = (char*)msg;
}

/**
 * 清理加载器上下文
 * 释放所有已分配的资源，包括：
 * - 已加载的对象
 * - 符号表
 * - 解释器路径
 * - 内存映射
 */
static void cleanup_context(void)
{
    // 清理已加载的对象
    struct LoadedObject *obj = g_ctx.loaded_objects;
    while (obj) 
    {
        struct LoadedObject *next = obj->next;
        if (obj->base) 
        {
            munmap(obj->base, obj->size);  // 解除内存映射
        }
        if (obj->path)
        {
            free(obj->path);  // 释放路径字符串
        }
        free(obj);
        obj = next;
    }
    
    // 清理符号表
    struct SymbolEntry *sym = g_ctx.symbols;
    while (sym) 
    {
        struct SymbolEntry *next = sym->next;
        if (sym->name)
        {
            free((void*)sym->name);  // 释放符号名
        }
        free(sym);
        sym = next;
    }
    
    // 清理解释器路径
    if (g_ctx.interp) 
    {
        free(g_ctx.interp);
    }
    
    // 重置上下文
    g_ctx.loaded_objects = NULL;
    g_ctx.symbols = NULL;
    g_ctx.error = NULL;
    g_ctx.interp = NULL;
}

/**
 * 查找符号
 * 在全局符号表中查找指定名称的符号
 * 
 * 实现原理：
 * 1. 遍历全局符号表
 * 2. 使用字符串比较查找匹配的符号名
 * 3. 返回找到的符号项或NULL
 * 
 * @param name: 要查找的符号名
 * @return: 找到返回符号项指针，否则返回NULL
 */
static struct SymbolEntry* find_symbol(const char *name)
{
    // 遍历全局符号表查找匹配的符号
    for (struct SymbolEntry *sym = g_ctx.symbols; sym; sym = sym->next)
    {
        if (strcmp(sym->name, name) == 0)
        {
            return sym;
        }
    }
    return NULL;
}

/**
 * 添加符号到全局符号表
 * 
 * 实现原理：
 * 1. 分配新的符号表项
 * 2. 复制符号名
 * 3. 设置符号值和所属对象
 * 4. 将符号添加到全局符号表
 * 
 * @param name: 符号名
 * @param value: 符号值（通常是地址）
 * @param obj: 符号所属的加载对象
 * @return: 成功返回0，失败返回-1
 */
static int add_symbol(const char *name, uint64_t value, struct LoadedObject *obj)
{
    // 分配新的符号表项
    struct SymbolEntry *entry = malloc(sizeof(*entry));
    if (!entry)
    {
        set_error("Failed to allocate symbol entry");
        return -1;
    }
    
    // 复制符号名
    char *name_copy = malloc(strlen(name) + 1);
    if (!name_copy)
    {
        free(entry);
        set_error("Failed to allocate symbol name");
        return -1;
    }
    
    // 初始化符号表项
    memcpy(name_copy, name, strlen(name) + 1);
    entry->name = name_copy;
    entry->value = value;
    entry->obj = obj;
    
    // 添加到符号表头部
    entry->next = g_ctx.symbols;
    g_ctx.symbols = entry;
    
    return 0;
}

/**
 * 处理重定位表
 * 修正代码中的地址引用，使其指向正确的目标位置
 * 
 * 实现原理：
 * 1. 从动态段获取重定位相关信息
 * 2. 遍历重定位表
 * 3. 根据重定位类型进行不同的处理：
 *    - R_AARCH64_GLOB_DAT: 全局数据引用
 *    - R_AARCH64_JUMP_SLOT: 函数调用跳转表
 *    - R_AARCH64_RELATIVE: 相对地址重定位
 * 
 * @param obj: 要处理重定位的加载对象
 * @return: 成功返回0，失败返回-1
 */
static int process_relocations(struct LoadedObject *obj)
{
    if (!obj->dynamic) 
    {
        return 0;  // 没有动态段，不需要重定位
    }
    
    // 获取重定位相关的表
    const char *strtab = NULL;     // 字符串表
    Elf64_Sym *symtab = NULL;      // 符号表
    Elf64_Rela *rela = NULL;       // 重定位表
    size_t rela_size = 0;          // 重定位表大小
    
    // 从动态段获取必要的表
    for (Elf64_Dyn *d = obj->dynamic; d->d_tag != DT_NULL; d++) 
    {
        switch (d->d_tag) 
        {
            case DT_STRTAB:
                // 加上基址偏移量获取实际地址
                strtab = (const char*)((char*)obj->base + d->d_un.d_ptr);
                LOG_DEBUG("strtab: 0x%x\n", strtab);
                break;
            case DT_SYMTAB:
                // 加上基址偏移量获取实际地址
                symtab = (Elf64_Sym*)((char*)obj->base + d->d_un.d_ptr);
                LOG_DEBUG("symtab: 0x%x\n", symtab);
                break;
            case DT_RELA:
                // 加上基址偏移量获取实际地址
                rela = (Elf64_Rela*)((char*)obj->base + d->d_un.d_ptr);
                LOG_DEBUG("rela: 0x%x\n", rela);
                break;
            case DT_RELASZ:
                rela_size = d->d_un.d_val;
                LOG_DEBUG("rela_size: 0x%x\n", rela_size);
                break;
        }
    }
    
    // 验证所有必需的表都存在
    if (!strtab || !symtab || !rela || !rela_size)
    {
        return 0;  // 没有需要处理的重定位
    }
    
    // 处理每个重定位项
    size_t rela_count = rela_size / sizeof(Elf64_Rela);
    printf("rela_count: 0x%x\n", rela_count);
    for (size_t i = 0; i < rela_count; i++) 
    {
        Elf64_Rela *r = &rela[i];
        uint32_t sym_idx = r->r_info >> 32;           // 符号表索引
        uint32_t type = r->r_info & 0xffffffff;       // 重定位类型
        
        // 获取符号信息
        Elf64_Sym *sym = &symtab[sym_idx];
        const char *sym_name = strtab + sym->st_name;
        LOG_DEBUG("sym_name: 0x%x\n", sym_name);
        
        // 计算重定位位置
        uint64_t *target = (uint64_t*)((char*)obj->base + r->r_offset);
        LOG_DEBUG("target: 0x%x base: 0x%x offset: 0x%x\n", target, obj->base, r->r_offset);
        // 根据重定位类型进行处理
        switch (type) 
        {
            case R_AARCH64_GLOB_DAT:
            case R_AARCH64_JUMP_SLOT:
            {
                // 查找符号的实际地址
                struct SymbolEntry *entry = find_symbol(sym_name);
                LOG_DEBUG("entry: 0x%x\n", entry);
                if (!entry)
                {
                    if (sym->st_shndx == SHN_UNDEF)
                    {
                        set_error("Undefined symbol");
                        return -1;
                    }
                    // 使用本地符号的地址
                    LOG_DEBUG("target: 0x%x base: 0x%x value: 0x%x\n", target, obj->base, sym->st_value);
                    *target = (uint64_t)obj->base + sym->st_value;
                }
                else 
                {
                    // 使用全局符号表中的地址
                    LOG_DEBUG("target: 0x%x value: 0x%x\n", target, entry->value);
                    *target = entry->value;
                }
                break;
            }
            case R_AARCH64_RELATIVE:
                // 基址重定位
                LOG_DEBUG("target: 0x%x addend: 0x%x\n", target, r->r_addend);
                *target = (uint64_t)obj->base + r->r_addend;
                break;
            default:
                set_error("Unknown relocation type");
                return -1;
        }
    }
    
    return 0;
}

/**
 * 添加搜索路径
 * 
 * @param path: 要添加的搜索路径
 * @return: 成功返回0，失败返回-1
 */
static int add_search_path(const char *path)
{
    if (g_search_paths.count >= MAX_SEARCH_PATHS) 
    {
        LOG_ERROR("Too many search paths\n");
        return -1;
    }

    char *path_copy = malloc(strlen(path) + 1);
    if (!path_copy) 
    {
        LOG_ERROR("Failed to allocate path\n");
        return -1;
    }

    memcpy(path_copy, path, strlen(path) + 1);
    g_search_paths.paths[g_search_paths.count++] = path_copy;
    LOG_DEBUG("Added search path: %s\n", path_copy);
    return 0;
}

/**
 * 初始化搜索路径
 * 添加默认搜索路径和环境变量指定的路径
 * 
 * @return: 成功返回0，失败返回-1
 */
static int init_search_paths(void)
{
    // 添加默认搜索路径
    const char *default_paths[] = {
        ".",                    // 当前目录
        "./lib",               // 相对路径
        "/lib",                // 系统库目录
        "/usr/lib",
        "/usr/local/lib",
        "out/lib",             // 项目输出目录
        NULL
    };

    for (const char **p = default_paths; *p; p++) 
    {
        if (add_search_path(*p) < 0) 
        {
            return -1;
        }
    }

    // 处理LD_LIBRARY_PATH环境变量
    char *ld_path = get_env("LD_LIBRARY_PATH");
    if (ld_path) 
    {
        char *path = malloc(strlen(ld_path) + 1);
        if (!path) 
        {
            return -1;
        }
        memcpy(path, ld_path, strlen(ld_path) + 1);

        // 分割路径字符串
        char *saveptr;
        char *p = strtok_r(path, ":", &saveptr);
        while (p) 
        {
            if (add_search_path(p) < 0) 
            {
                free(path);
                return -1;
            }
            p = strtok_r(NULL, ":", &saveptr);
        }
        free(path);
    }

    return 0;
}

/**
 * 在搜索路径中查找库文件
 * 
 * @param name: 库文件名
 * @param full_path: 输出缓冲区，用于存储找到的完整路径
 * @param size: 缓冲区大小
 * @return: 成功返回0，失败返回-1
 */
static int find_library(const char *name, char *full_path, size_t size)
{
    LOG_DEBUG("Finding library: %s\n", name);
    
    // 如果是绝对路径，直接检查文件是否存在
    if (name[0] == '/') 
    {
        if (file_exists(name) == 0) 
        {
            safe_strncpy(full_path, name, size);
            return 0;
        }
        return -1;
    }

    // 在所有搜索路径中查找
    for (int i = 0; i < g_search_paths.count; i++) 
    {
        safe_snprintf(full_path, size, "%s/%s", g_search_paths.paths[i], name);
        LOG_DEBUG("Trying path: %s\n", full_path);
        
        if (file_exists(full_path) == 0) 
        {
            LOG_DEBUG("Found library at: %s\n", full_path);
            return 0;
        }
    }

    LOG_ERROR("Library not found: %s\n", name);
    return -1;
}

/**
 * 清理搜索路径
 */
static void cleanup_search_paths(void)
{
    for (int i = 0; i < g_search_paths.count; i++) 
    {
        free(g_search_paths.paths[i]);
    }
    g_search_paths.count = 0;
}

/**
 * 处理命令行参数中的库路径
 * 
 * @param argc: 参数个数
 * @param argv: 参数数组
 * @return: 成功返回0，失败返回-1
 */
static int process_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) 
    {
        if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) 
        {
            if (add_search_path(argv[++i]) < 0) 
            {
                return -1;
            }
        }
    }
    return 0;
}

/**
 * 加载ELF对象（可执行文件或共享库）
 * 
 * 实现原理：
 * 1. 打开并读取ELF文件头
 * 2. 验证ELF文件的有效性和兼容性
 * 3. 计算内存布局
 * 4. 分配并映射内存空间
 * 5. 加载程序段到内存
 * 6. 设置内存权限
 * 
 * @param name: 文件名
 * @param is_exec: 是否是可执行文件
 * @return: 成功返回LoadedObject结构指针，失败返回NULL
 */
static struct LoadedObject* load_object(const char *name, int is_exec)
{
    int fd = -1;
    void *mapped_base = NULL;
    struct LoadedObject *obj = NULL;
    size_t total_size = 0;
    int result = -1;
    char full_path[MAX_PATH_LEN];

    // 检查是否已加载
    for (struct LoadedObject *l = g_ctx.loaded_objects; l; l = l->next) 
    {
        if (strcmp(l->path, name) == 0) 
        {
            return l;  // 已加载，直接返回
        }
    }

    // 如果不是可执行文件，需要在搜索路径中查找
    if (!is_exec) 
    {
        if (find_library(name, full_path, sizeof(full_path)) < 0) 
        {
            LOG_ERROR("Failed to find library: %s\n", name);
            goto cleanup;
        }
        name = full_path;
    }

    LOG_DEBUG("Loading object: %s\n", name);
    
    // 分配对象结构
    obj = (struct LoadedObject *)malloc(64);
    if (!obj) 
    {
        LOG_ERROR("Failed to allocate object info\n");
        goto cleanup;
    }
    memset((char *)obj, 0, sizeof(struct LoadedObject));
    // 复制文件路径
    obj->path = malloc(strlen(name) + 1);
    if (!obj->path) 
    {
        LOG_ERROR("Failed to allocate path\n");
        goto cleanup;
    }
    memcpy(obj->path, name, strlen(name) + 1);
    // 打开文件
    fd = open(name, O_RDONLY, 0);
    if (fd < 0) 
    {
        LOG_ERROR("Failed to open file\n");
        goto cleanup;
    }
    // 读取并验证ELF头
    Elf64_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) 
    {
        LOG_ERROR("Failed to read ELF header\n");
        goto cleanup;
    }

    // 验证ELF魔数
    if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') 
    {
        LOG_ERROR("Invalid ELF file");
        goto cleanup;
    }
    
    // 验证文件类型
    if (is_exec && ehdr.e_type != ET_EXEC) 
    {
        LOG_ERROR("Not an executable file");
        goto cleanup;
    } 
    else if (!is_exec && ehdr.e_type != ET_DYN) 
    {
        LOG_ERROR("Not a shared object");
        goto cleanup;
    }
    
    // 验证机器类型
    if (ehdr.e_machine != EM_AARCH64) 
    {
        LOG_ERROR("Invalid machine type");
        goto cleanup;
    }
    
    // 计算内存布局
    uint64_t min_vaddr = UINT_MAX;
    uint64_t max_vaddr = 0;
    LOG_DEBUG("ehdr.e_phnum: %d\n", ehdr.e_phnum);
    // 遍历程序头表计算地址范围
    for (int i = 0; i < ehdr.e_phnum; i++) 
    {
        Elf64_Phdr phdr;
        if (lseek(fd, ehdr.e_phoff + i * sizeof(phdr), SEEK_SET) < 0 ||
            read(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) 
        {
            LOG_ERROR("Failed to read program header");
            goto cleanup;
        }
        
        if (phdr.p_type == PT_LOAD) 
        {
            // 更新地址范围
            LOG_DEBUG("Processing segment %d: vaddr=0x%x, memsz=0x%x, min_vaddr=0x%x, max_vaddr=0x%x\n", 
                   i, phdr.p_vaddr, phdr.p_memsz, min_vaddr, max_vaddr);
            if (phdr.p_vaddr < min_vaddr)
            {
                min_vaddr = phdr.p_vaddr;
            }

            if (phdr.p_vaddr + phdr.p_memsz > max_vaddr)
            {
                max_vaddr = phdr.p_vaddr + phdr.p_memsz;
            }
        }
        else if (phdr.p_type == PT_INTERP && !g_ctx.interp) 
        {
            // 读取解释器路径
            char interp[256];
            if (lseek(fd, phdr.p_offset, SEEK_SET) < 0 ||
                read(fd, interp, phdr.p_filesz) != phdr.p_filesz) 
            {
                LOG_ERROR("Failed to read interpreter path");
                goto cleanup;
            }
            interp[phdr.p_filesz] = '\0';
            
            // 保存解释器路径
            g_ctx.interp = (char *)malloc(strlen(interp) + 1);
            if (!g_ctx.interp) 
            {
                LOG_ERROR("Failed to allocate interpreter path");
                goto cleanup;
            }
            LOG_DEBUG("interp: 0x%x\n", g_ctx.interp);
            memcpy((char *)g_ctx.interp, interp, strlen(interp) + 1);
        }
    }
    
    // 计算总大小并对齐到页大小
    total_size = max_vaddr - min_vaddr;
    total_size = (total_size + 0xfff) & ~0xfff;  // 4KB对齐

    LOG_DEBUG("total size: %d\n", total_size); 
    
    // 分配内存空间
    if (is_exec) 
    {
        // 可执行文件使用固定地址
        mapped_base = (void*)min_vaddr;
        LOG_DEBUG("before mmap mmaped_base: 0x%x\n", mapped_base);
        //mapped_base = mmap(mapped_base, total_size,
        //                  PROT_READ | PROT_WRITE,
        //                  MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
        //                  -1, 0);
        mapped_base = mmap(NULL, total_size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);                          
    } 
    else 
    {
        // 共享库使用动态地址
        mapped_base = mmap(NULL, total_size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);
    }

    if (mapped_base == NULL) 
    {
        LOG_ERROR("Failed to allocate memory");
    }

    LOG_DEBUG("mapped base: 0x%x\n", mapped_base);
    
    if (mapped_base == MAP_FAILED) 
    {
        LOG_ERROR("Failed to allocate memory\n");
        goto cleanup;
    }
    
    // 加载程序段
    for (int i = 0; i < ehdr.e_phnum; i++) 
    {
        Elf64_Phdr phdr;
        if (lseek(fd, ehdr.e_phoff + i * sizeof(phdr), SEEK_SET) < 0 ||
            read(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) 
        {
            LOG_ERROR("Failed to read program header\n");
            goto cleanup;
        }
        
        if (phdr.p_type == PT_LOAD) 
        {
            // 计算段的加载地址
            void *seg_addr = (char*)mapped_base + (phdr.p_vaddr - min_vaddr);
            
            // 确保段地址页对齐
            uint64_t aligned_addr = (uint64_t)seg_addr & ~0xfff;  // 4KB对齐
            size_t aligned_size = ((phdr.p_memsz + 0xfff) & ~0xfff);  // 向上取整到4KB
            
            // 读取段内容
            if (lseek(fd, phdr.p_offset, SEEK_SET) < 0 ||
                read(fd, seg_addr, phdr.p_filesz) != phdr.p_filesz) 
            {
                LOG_ERROR("Failed to read segment\n");
                goto cleanup;
            }
            
            // 清零未初始化数据
            if (phdr.p_filesz < phdr.p_memsz) 
            {
                memset((char*)seg_addr + phdr.p_filesz, 0,
                       phdr.p_memsz - phdr.p_filesz);
            }
            
            // 设置段的访问权限
            int prot = 0;
            if (phdr.p_flags & PF_R) prot |= PROT_READ;
            if (phdr.p_flags & PF_W) prot |= PROT_WRITE;
            if (phdr.p_flags & PF_X) prot |= PROT_EXEC;
            
            LOG_DEBUG("seg_addr: 0x%x\n", seg_addr);
            LOG_DEBUG("aligned_addr: 0x%x\n", aligned_addr);
            LOG_DEBUG("aligned_size: 0x%x\n", aligned_size);
            LOG_DEBUG("prot: 0x%x\n", prot);
            
            if (mprotect((void*)aligned_addr, aligned_size, prot) < 0) 
            {
                LOG_ERROR("Failed to set segment protection\n");
                goto cleanup;
            }
        }
        else if (phdr.p_type == PT_DYNAMIC) 
        {
            // 保存动态段地址
            obj->dynamic = (Elf64_Dyn*)((char*)mapped_base +
                                      (phdr.p_vaddr - min_vaddr));
        }
    }
    
    // 设置对象信息
    obj->base = mapped_base;
    obj->base_offset = (uint64_t)mapped_base - (is_exec ? min_vaddr : 0);  // 计算基址偏移
    obj->size = total_size;
    obj->entry = (void*)((char*)mapped_base + (ehdr.e_entry - min_vaddr));
    
    LOG_DEBUG("base: 0x%x, base_offset: 0x%x\n", obj->base, obj->base_offset);
    
    // 添加到已加载对象列表
    obj->next = g_ctx.loaded_objects;
    g_ctx.loaded_objects = obj;
    
    result = 0;

cleanup:
    // 清理资源
    if (fd >= 0) 
    {
        close(fd);
    }
    
    if (result < 0) 
    {
        if (mapped_base) 
        {
            munmap(mapped_base, total_size);
        }
        if (obj) 
        {
            if (obj->path) 
            {
                free(obj->path);
            }
            free(obj);
            obj = NULL;
        }
    }
    
    return obj;
}

/**
 * 加载依赖库
 * 递归加载对象的所有依赖库
 * 
 * 实现原理：
 * 1. 从动态段获取DT_NEEDED项
 * 2. 对每个依赖库：
 *    - 加载库文件
 *    - 递归加载其依赖
 * 
 * @param obj: 要处理依赖的对象
 * @return: 成功返回0，失败返回-1
 */
static int load_dependencies(struct LoadedObject *obj)
{
    if (!obj->dynamic) 
    {
        return 0;  // 没有动态段，不需要加载依赖
    }
    
    // 获取字符串表
    const char *strtab = NULL;
    for (Elf64_Dyn *d = obj->dynamic; d->d_tag != DT_NULL; d++) 
    {
        LOG_DEBUG("d->d_tag: %d\n", d->d_tag);
        if (d->d_tag == DT_STRTAB) 
        {
            // 使用保存的基址偏移量计算实际地址
            strtab = (const char*)d->d_un.d_ptr + obj->base_offset;
            LOG_DEBUG("d_ptr: 0x%x, base_offset: 0x%x, actual strtab: 0x%x\n", 
                     d->d_un.d_ptr, obj->base_offset, strtab);
            break;
        }
    }
    
    if (!strtab) 
    {
        return 0;
    }

    LOG_DEBUG("strtab: 0x%x\n", strtab);
    
    // 处理每个DT_NEEDED项
    for (Elf64_Dyn *d = obj->dynamic; d->d_tag != DT_NULL; d++) 
    {
        if (d->d_tag == DT_NEEDED) 
        {
            // 获取依赖库名称
            const char *name = strtab + d->d_un.d_val;
            LOG_DEBUG("load_dependencies: %s\n", name);
            
            // 加载依赖库
            struct LoadedObject *dep = load_object(name, 0);
            if (!dep) 
            {
                return -1;
            }
            
            // 递归加载依赖库的依赖
            if (load_dependencies(dep) < 0) 
            {
                return -1;
            }
        }
    }
    
    return 0;
}

/**
 * 动态链接器主函数
 * 
 * 实现流程：
 * 1. 加载主可执行文件
 * 2. 如果存在解释器，加载解释器
 * 3. 加载所有依赖库
 * 4. 处理所有重定位
 * 5. 跳转到程序入口点
 * 
 * @param argc: 参数个数
 * @param argv: 参数数组
 * @return: 成功返回0，失败返回非0
 */
int main(int argc, char **argv, char **envp)
{
    // 保存环境变量
    g_environ = envp;
    
    if (argc < 2) 
    {
        LOG_ERROR("Usage: %s [-L path] <executable>\n", argv[0]);
        return 1;
    }

    // 初始化搜索路径
    if (init_search_paths() < 0) 
    {
        LOG_ERROR("Failed to initialize search paths\n");
        return 1;
    }

    // 处理命令行参数
    if (process_args(argc, argv) < 0) 
    {
        LOG_ERROR("Failed to process arguments\n");
        cleanup_search_paths();
        return 1;
    }

    // 获取可执行文件路径（跳过-L参数）
    char *exec_path = NULL;
    for (int i = 1; i < argc; i++) 
    {
        if (strcmp(argv[i], "-L") == 0) 
        {
            i++;  // 跳过路径参数
            continue;
        }
        exec_path = argv[i];
        break;
    }

    if (!exec_path) 
    {
        LOG_ERROR("No executable specified\n");
        cleanup_search_paths();
        return 1;
    }

    // 加载可执行文件
    struct LoadedObject *exec = load_object(exec_path, 1);
    if (!exec) 
    {
        LOG_ERROR("Failed to load executable: %s\n",
               g_ctx.error ? g_ctx.error : "Unknown error");
        cleanup_context();
        cleanup_search_paths();
        return 1;
    }
    LOG_DEBUG("exec: %s\n", exec->path);

    // 加载所有依赖
    if (load_dependencies(exec) < 0) 
    {
        LOG_ERROR("Failed to load dependencies: %s\n",
               g_ctx.error ? g_ctx.error : "Unknown error");
        cleanup_context();
        cleanup_search_paths();
        return 1;
    }
    LOG_DEBUG("loaded_objects: %s\n", g_ctx.loaded_objects->path);
    // 处理所有重定位
    for (struct LoadedObject *obj = g_ctx.loaded_objects; obj; obj = obj->next) 
    {
        if (process_relocations(obj) < 0) 
        {
            LOG_ERROR("Failed to process relocations: %s\n",
                   g_ctx.error ? g_ctx.error : "Unknown error");
            cleanup_context();
            cleanup_search_paths();
            return 1;
        }
    }
    LOG_DEBUG("process_relocations: %s\n", g_ctx.loaded_objects->path);
    // 调用程序入口点
    void (*entry)(void) = exec->entry;
    entry();
    LOG_DEBUG("entry: 0x%x\n", exec->entry);
    // 清理资源
    cleanup_context();
    cleanup_search_paths();
    return 0;
}

