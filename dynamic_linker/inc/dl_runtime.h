#ifndef DL_RUNTIME_H
#define DL_RUNTIME_H

#define NULL ((void *)0)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef unsigned long uintptr_t;
typedef unsigned long size_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long int64_t;
typedef signed long intptr_t; 
typedef signed long ssize_t;

#define AT_FDCWD (-100)
#define __MINI_ALIGN(__value, __alignment) (((__value) + (__alignment) - 1) & ~((__alignment) - 1))

#define va_list __builtin_va_list
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

// 辅助向量项：内核传给动态链接器的 (tag, value) 对，数组以 AT_NULL 结尾
typedef struct {
    uint64_t tag;   /* 条目类型，如 AT_BASE(7)、AT_ENTRY(9)、AT_PHDR(3) */
    uint64_t value; /* 含义由 tag 决定：地址、整数、或指向字符串/结构的指针 */
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

#define EI_NIDENT 16
#define EI_MAG0    0
#define EI_MAG1    1
#define EI_MAG2    2
#define EI_MAG3    3
#define ELFMAG0    0x7f
#define ELFMAG1    'E'
#define ELFMAG2    'L'
#define ELFMAG3    'F'

// ELF 文件头（64 位），描述整个 ELF 的布局与类型
typedef struct {
    unsigned char e_ident[EI_NIDENT]; /* ELF 标识：魔数(0x7F 'E' 'L' 'F')、位数(32/64)、
                                         字节序、版本、OS/ABI 等，见 EI_* 常量 */
    uint16_t e_type;                  /* 文件类型：ET_REL(1) 可重定位、ET_EXEC(2) 可执行、
                                         ET_DYN(3) 共享对象/PIE、ET_CORE(4) 核心转储 */
    uint16_t e_machine;               /* 目标架构，如 EM_AARCH64(183)、EM_X86_64(62) */
    uint32_t e_version;               /* ELF 版本，当前固定为 EV_CURRENT(1) */
    uint64_t e_entry;                 /* 程序入口虚拟地址（_start / _dynamic_entry） */
    uint64_t e_phoff;                 /* 程序头表(Phdr)在文件中的字节偏移 */
    uint64_t e_shoff;                 /* 节头表(Shdr)在文件中的字节偏移；无节头时为 0 */
    uint32_t e_flags;                 /* 处理器相关标志（AArch64 通常为 0） */
    uint16_t e_ehsize;                /* 本 ELF 头结构体的字节大小（64 位 ELF 为 64） */
    uint16_t e_phentsize;             /* 单个程序头表项的字节大小（64 位 ELF 为 56） */
    uint16_t e_phnum;                 /* 程序头表项数量 */
    uint16_t e_shentsize;             /* 单个节头表项的字节大小（64 位 ELF 为 64） */
    uint16_t e_shnum;                 /* 节头表项数量 */
    uint16_t e_shstrndx;              /* 节名字符串表(.shstrtab)在节头表中的索引 */
} Elf64_Ehdr;

// 程序头 / 段描述符（64 位）；仅 PT_LOAD 由内核 mmap，其余多为索引或标注
typedef struct {
    uint32_t p_type;   /* 段类型：PT_LOAD(1) 可加载、PT_DYNAMIC(2) 动态链接信息、
                          PT_INTERP(3) 解释器路径、PT_GNU_STACK 栈属性等 */
    uint32_t p_flags;  /* 段权限：PF_X(1) 可执行、PF_W(2) 可写、PF_R(4) 可读 */
    uint64_t p_offset; /* 该段内容在 ELF 文件中的起始字节偏移 */
    uint64_t p_vaddr;  /* 该段加载到进程虚拟地址空间后的起始地址 */
    uint64_t p_paddr;  /* 物理地址（现代 OS 加载时通常忽略，保留给裸机/固件） */
    uint64_t p_filesz; /* 该段在文件中占用的字节数（mmap/read 的长度） */
    uint64_t p_memsz;  /* 该段在内存中占用的字节数；大于 p_filesz 的差值为 BSS（零填充） */
    uint64_t p_align;  /* 对齐约束：p_vaddr 与 p_offset 对 p_align 同余，且 p_align 为 2 的幂 */
} Elf64_Phdr;

#define PT_LOAD 1
#define PT_DYNAMIC 2

// .dynamic 节中的动态链接条目（由 PT_DYNAMIC 指向，Elf64_Dyn 数组以 DT_NULL 结尾）
typedef struct {
    int64_t d_tag;   /* 条目类型：DT_NEEDED、DT_SYMTAB、DT_RELA 等；DT_NULL(0) 表示数组结束 */
    uint64_t d_val;  /* 含义由 d_tag 决定：常为虚拟地址(如 DT_STRTAB)、字节数(如 DT_STRSZ)、
                        对齐/标志(如 DT_FLAGS) 或其它整数属性 */
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

// RELA 重定位条目：带加数的重定位，AArch64 动态链接主要使用此类
typedef struct {
    uint64_t r_offset; /* 需修正的内存地址（通常为 GOT/数据段的虚拟地址） */
    uint64_t r_info;   /* 符号与类型打包：高 32 位符号表索引，低 32 位重定位类型
                          （见 ELF64_R_SYM / ELF64_R_TYPE） */
    int64_t  r_addend; /* 加数：最终值 = 符号值 + r_addend（REL 无此字段） */
} Elf64_Rela;

// AArch64 Elf64_Rela.r_info 为 64 位：高 32 位符号索引，低 32 位重定位类型
#define ELF64_R_SYM(i)    ((uint32_t)(((i) >> 32) & 0xffffffff))  /* 从 r_info 提取符号表索引 r_sym，查 .dynsym[sym] */
#define ELF64_R_TYPE(i)   ((uint32_t)((i) & 0xffffffff))         /* 从 r_info 提取重定位类型，如 R_AARCH64_RELATIVE */
#define ELF64_R_INFO(s,t) ((((uint64_t)(s)) << 32) | ((t) & 0xffffffff))  /* 将符号索引 s 与类型 t 打包为 r_info */

// AArch64 重定位类型
#define R_AARCH64_NONE     0
#define R_AARCH64_ABS64    257
#define R_AARCH64_GLOB_DAT 1025
#define R_AARCH64_JUMP_SLOT 1026
#define R_AARCH64_RELATIVE 1027

// 动态符号表条目（.dynsym），与 .dynstr 配合解析符号名
typedef struct {
    uint32_t st_name;      /* 符号名在 .dynstr 中的字节偏移 */
    unsigned char st_info; /* 高 4 位绑定(STB_LOCAL/GLOBAL/WEAK)，低 4 位类型(STT_FUNC 等) */
    unsigned char st_other;/* 可见性等扩展信息，通常为 0 */
    uint16_t st_shndx;     /* 关联节索引；SHN_UNDEF 表示未定义（需从其它库解析） */
    uint64_t st_value;     /* 符号虚拟地址或相对值（取决于符号类型与链接方式） */
    uint64_t st_size;      /* 符号占用字节数（如变量、函数体大小），无则为 0 */
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

#define O_RDONLY 00000000
#define O_WRONLY 00000001
#define O_RDWR 00000002
#define O_CREAT 00000100
#define O_TRUNC 00001000
#define O_APPEND 00002000

#define PROT_READ        0x1                /* Page can be read.  */
#define PROT_WRITE        0x2               /* Page can be written.  */
#define PROT_EXEC        0x4                /* Page can be executed.  */
#define PROT_NONE        0x0                /* Page can not be accessed.  */
#define PROT_GROWSDOWN        0x01000000    /* Extend change to start of
                                               growsdown vma (mprotect only).  */
#define PROT_GROWSUP        0x02000000      /* Extend change to start of
                                               growsup vma (mprotect only).  */

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

enum
{
    SEEK_SET = 0,
    SEEK_CUR,
    SEEK_END
};

/* 不依赖 PLT/GOT 的运行时辅助函数，供自举阶段及调试输出使用 */

size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);
void itoa(int64_t value, char *buf, int base);
void utoa_hex(uint64_t value, char *buf);
int write(int fd, const void *buf, size_t count);
int printf(const char *format, ...);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
char *get_env(char **environ, const char *name);
void print_all_env(char **environ);

int strcmp(const char *s1, const char *s2);
int vsprintf(char *buf, const char *format, va_list args);
int vsnprintf(char *buf, size_t size, const char *format, va_list args);
int snprintf(char *buf, size_t size, const char *format, ...);
int open(const char *pathname, int flags, int mode);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
int lseek(int fd, int offset, int whence);
void *malloc(size_t size);
void free(void *ptr);
void *mmap(void *addr, long size, int prot, int flags, int fd, long offset);
int munmap(void *addr, long size);

#endif
