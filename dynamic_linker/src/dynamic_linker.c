#include "dl_runtime.h"

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
#define EI_MAG0    0
#define EI_MAG1    1
#define EI_MAG2    2
#define EI_MAG3    3
#define ELFMAG0    0x7f
#define ELFMAG1    'E'
#define ELFMAG2    'L'
#define ELFMAG3    'F'
#define PT_LOAD 1
#define PT_DYNAMIC 2

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
    uint64_t r_info;     // AArch64/ELF64: 高32位是符号索引，低32位是类型
    int64_t  r_addend;   // 使用int64_t代替int32_t
} Elf64_Rela;

// AArch64 使用 64 位 r_info: 高 32 位符号索引，低 32 位类型
#define ELF64_R_SYM(i)    ((uint32_t)(((i) >> 32) & 0xffffffff))
#define ELF64_R_TYPE(i)   ((uint32_t)((i) & 0xffffffff))
#define ELF64_R_INFO(s,t) ((((uint64_t)(s)) << 32) | ((t) & 0xffffffff))

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

// 已加载库的信息结构
typedef struct loaded_library {
    char *path;                    // 库文件路径
    uint64_t base_addr;            // 加载基址
    Elf64_Ehdr *ehdr;              // ELF头
    Elf64_Phdr *phdr;              // 程序头表
    Elf64_Dyn *dynamic;            // 动态段
    const char *strtab;            // 字符串表
    Elf64_Sym *symtab;             // 符号表
    uint32_t symnum;               // 符号数量
    uint64_t init;                 // DT_INIT 地址
    uint64_t init_array;           // DT_INIT_ARRAY 地址
    uint64_t init_array_size;      // DT_INIT_ARRAYSZ
    struct loaded_library *next;   // 链表指针
} LoadedLibrary;

// 全局已加载库链表
static LoadedLibrary *g_loaded_libraries = NULL;

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
        return;
    }
    
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
        printf("Warning: dynamic pointer out of range: 0x%x\n", 
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
        if (symname && strncmp(symname, name, 256) == 0) {
            return &symtab[i];
        }
    }
    
    return NULL;
}

/**
 * 通过LOAD段映射关系计算符号的实际内存地址（前向声明）
 */
static uint64_t calculate_symbol_address(Elf64_Sym *sym_entry, uint64_t base_addr,
                                         Elf64_Phdr *phdr, uint16_t phnum);

/**
 * 全局符号解析：从所有已加载的库中查找符号
 * 返回符号的实际内存地址，如果找不到返回0
 */
static uint64_t resolve_symbol_global(const char *name)
{
    if (!name) {
        return 0;
    }
    
    // 遍历所有已加载的库
    LoadedLibrary *lib = g_loaded_libraries;
    while (lib) {
        if (lib->symtab && lib->strtab && lib->symnum > 0) {
            // 在当前库中查找符号
            Elf64_Sym *sym_entry = find_symbol(name, lib->symtab, lib->strtab, lib->symnum);
            if (sym_entry) {
                // 计算符号的实际地址
                uint64_t sym_addr = calculate_symbol_address(sym_entry, lib->base_addr, 
                                                             lib->phdr, lib->ehdr->e_phnum);
                if (sym_addr == 0) {
                    // 回退：使用简化方法
                    sym_addr = lib->base_addr + sym_entry->st_value;
                }
                
                return sym_addr;
            }
        }
        lib = lib->next;
    }
    
    return 0;
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
    if (!phdr)
    {
        return 0;
    }
    
    // 查找包含该虚拟地址的LOAD段
    for (uint16_t i = 0; i < phnum; i++)
    {
        Elf64_Phdr *load = &phdr[i];
        if (load->p_type == PT_LOAD)
        {
            uint64_t load_end = load->p_vaddr + load->p_memsz;
            // 检查虚拟地址是否在这个LOAD段中
            if (vaddr >= load->p_vaddr && vaddr < load_end)
            {
                // 通用地址计算：适用于自举和其他ELF文件
                // base_addr 的含义：
                //   - 自举：base_addr 是文件偏移0的地址（整个文件被连续加载）
                //   - 其他ELF文件：base_addr 是第一个LOAD段的加载地址
                // 
                // 对于两种情况，计算公式都是：
                //   实际地址 = base_addr + (vaddr - first_load_vaddr)
                // 
                // 首先找到第一个LOAD段
                uint64_t first_load_vaddr = 0;
                for (uint16_t j = 0; j < phnum; j++)
                {
                    if (phdr[j].p_type == PT_LOAD)
                    {
                        first_load_vaddr = phdr[j].p_vaddr;
                        break;
                    }
                }
                
                // 计算实际地址
                uint64_t result = base_addr + (vaddr - first_load_vaddr);
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
    if (!rela)
    {
        return;
    }
    
    // 安全检查：确保rela指针有效
    if ((uintptr_t)rela < base_addr || (uintptr_t)rela > base_addr + 0x200000)
    {
        printf("Error: rela pointer out of range: 0x%x\n", (unsigned int)(uintptr_t)rela);
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
    
    if (!ehdr || !phdr)
    {
        // 回退：直接使用 base_addr + offset（假设first_load_vaddr=0）
        addr = (uint64_t *)(base_addr + offset);
    } 
    else
    {
        // 首先尝试使用通用的 calculate_address_from_vaddr
        // 这适用于所有ELF文件（自举和其他共享库）
        uint64_t target_addr = calculate_address_from_vaddr(offset, base_addr, phdr, ehdr->e_phnum);
        
        if (target_addr != 0)
        {
            // 成功找到LOAD段，使用计算结果
            addr = (uint64_t *)target_addr;
        }
        else
        {
            // 回退：如果 calculate_address_from_vaddr 失败，使用简化方法
            // 找到第一个LOAD段，计算文件偏移
            uint64_t first_load_vaddr = 0;
            for (uint16_t i = 0; i < ehdr->e_phnum; i++)
            {
                Elf64_Phdr *load = &phdr[i];
                if (load->p_type == PT_LOAD)
                {
                    first_load_vaddr = load->p_vaddr;
                    break;
                }
            }
            // 通用公式：实际地址 = base_addr + (vaddr - first_load_vaddr)
            addr = (uint64_t *)(base_addr + offset - first_load_vaddr);
        }
    }
    
    // 安全检查：确保地址在合理范围内
    if (!addr)
    {
        printf("Error: addr is NULL\n");
        return;
    }

    if ((uintptr_t)addr < base_addr || (uintptr_t)addr > base_addr + 0x200000) 
    {
        printf("Warning: relocation target address out of range: 0x%x\n",
                     (unsigned int)(uintptr_t)addr);
        return;
    }
    
    switch (type)
    {
        case R_AARCH64_RELATIVE:  // 1027
            // 相对重定位：addr = base_addr + addend
            // addend 是文件偏移，需要转换为实际地址
            *addr = base_addr + rela->r_addend;
            break;
            
        case R_AARCH64_ABS64:     // 257
        case R_AARCH64_GLOB_DAT:  // 1025
            // 需要根据上下文区分：
            // - R_AARCH64_ABS64: 用于直接重定位（非GOT/PLT）
            // - R_AARCH64_GLOB_DAT: 用于GOT条目
            // 这里统一处理为需要查找符号的情况
            // 计算公式：S + A，其中 S = 符号地址，A = r_addend
            if (sym < symnum && symtab)
            {
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
            }
            else
            {
                printf("Warning: invalid symbol index %d for relocation\n", (unsigned int)sym);
            }
            break;
            
        case R_AARCH64_JUMP_SLOT: {
            // 跳转槽（PLT条目）：需要查找符号
            // 计算公式：S + A，其中 S = 符号地址，A = r_addend
            uint64_t sym_addr = 0;
            
            // 首先尝试从当前ELF文件的符号表中查找
            if (sym < symnum && symtab && strtab)
            {
                Elf64_Sym *sym_entry = &symtab[sym];
                const char *symname = get_symbol_name((uint64_t)strtab, sym_entry->st_name);
                
                if (symname && sym_entry->st_value != 0)
                {
                    // 通过LOAD段映射关系计算符号地址
                    if (ehdr && phdr)
                    {
                        sym_addr = calculate_symbol_address(sym_entry, base_addr, phdr, ehdr->e_phnum);
                    }
                    
                    // 如果通过LOAD段找不到，使用简化方法（回退）
                    if (sym_addr == 0)
                    {
                        sym_addr = base_addr + sym_entry->st_value;
                    }
                }
            }
            
            // 如果从当前ELF文件找不到符号，尝试从已加载的库中查找
            if (sym_addr == 0 && strtab && sym < symnum && symtab)
            {
                Elf64_Sym *sym_entry = &symtab[sym];
                const char *symname = get_symbol_name((uint64_t)strtab, sym_entry->st_name);
                if (symname)
                {
                    sym_addr = resolve_symbol_global(symname);
                }
            }
            
            if (sym_addr != 0)
            {
                *addr = sym_addr + rela->r_addend;
            }
            else
            {
                printf("Warning: Cannot resolve symbol for JUMP_SLOT relocation at 0x%x\n",
                             (unsigned int)(uintptr_t)addr);
                // 如果找不到符号，使用 base_addr + addend（可能不正确，但避免崩溃）
                *addr = base_addr + rela->r_addend;
            }
            break;
        }
            
        case 0:  // R_AARCH64_NONE (0)
            // 无操作重定位，忽略
            break;
            
        default:
            // 未知重定位类型，尝试作为相对重定位处理
            printf("Warning: unknown relocation type %d, treating as relative\n", type);
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
        printf("Error: AT_BASE not found in auxv\n");
        return 1;
    }
    
    // 获取ELF头
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)g_auxv_base;
    
    // 验证ELF魔数
    if (ehdr->e_ident[0] != 0x7f || 
        ehdr->e_ident[1] != 'E' || 
        ehdr->e_ident[2] != 'L' || 
        ehdr->e_ident[3] != 'F')
    {
        printf("Error: Invalid ELF magic\n");
        return 1;
    }
    
    // 计算程序头表地址
    Elf64_Phdr *phdr = (Elf64_Phdr *)(g_auxv_base + ehdr->e_phoff);
    
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
                    
                } else {
                    // 如果没找到包含动态段的LOAD段，使用简化方法
                    printf("Warning: Cannot find LOAD segment containing dynamic segment, using fallback\n");
                    // 简化计算：假设虚拟地址空间连续
                    dynamic_memaddr = g_auxv_base + (p->p_vaddr - first_load_vaddr);
                }
            }
            
            dynamic = (Elf64_Dyn *)dynamic_memaddr;
            break;
        }
    }
    
    if (!dynamic) {
        printf("Error: Dynamic segment not found\n");
        return 1;
    }

    // 验证动态段地址是否有效（简单检查：在合理范围内）
    if ((uintptr_t)dynamic < g_auxv_base || 
        (uintptr_t)dynamic > g_auxv_base + 0x1000000) {
        printf("Error: Dynamic segment address out of range: 0x%x\n", 
                     (unsigned int)(uintptr_t)dynamic);
        return 1;
    }
    
    // 尝试读取第一个动态条目来验证地址是否有效
    // 使用volatile避免编译器优化
    volatile Elf64_Dyn *test_dyn = dynamic;
    volatile int64_t test_tag = test_dyn->d_tag;
    (void)test_tag;  // 避免未使用变量警告
    
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
        printf("Error: Missing required dynamic tags (strtab=0x%x, symtab=0x%x)\n",
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
                //printf("1111strtab: vaddr=0x%x, memaddr=0x%x\n", 
                             //(unsigned int)strtab_addr, (unsigned int)strtab_memaddr);
                //printf("11111p_vaddr: 0x%x, p_offset: 0x%x\n", (unsigned int)load->p_vaddr, (unsigned int)load->p_offset);
            }
            // 计算symtab地址
            if (symtab_memaddr == 0 && symtab_addr >= load->p_vaddr && 
                symtab_addr < load->p_vaddr + load->p_memsz) {
                // 使用该LOAD段的映射关系
                uint64_t offset_in_segment = symtab_addr - load->p_vaddr;
                uint64_t file_offset = load->p_offset + offset_in_segment;
                symtab_memaddr = g_auxv_base + file_offset;
                //printf("symtab: vaddr=0x%x, memaddr=0x%x, p_vaddr=0x%x, p_offset=0x%x\n", 
                             //(unsigned int)symtab_addr, (unsigned int)symtab_memaddr, 
                             //(unsigned int)load->p_vaddr, (unsigned int)load->p_offset);
                //printf("22222symtab: vaddr=0x%x, memaddr=0x%x\n", 
                             //(unsigned int)symtab_addr, (unsigned int)symtab_memaddr);
                //printf("22222p_vaddr: 0x%x, p_offset: 0x%x\n", (unsigned int)load->p_vaddr, (unsigned int)load->p_offset);
            }
            if (strtab_memaddr != 0 && symtab_memaddr != 0) {
                break;
            }
        }
    }
    
    // 如果没找到，使用简化方法（假设第一个LOAD段的p_offset=0，且虚拟地址空间连续）
    // 这适用于大多数情况（PIE/共享库）
    if (strtab_memaddr == 0)
    {
        if (first_load_offset == 0)
        {
            // 如果第一个LOAD段的p_offset=0，可以使用简化公式
            strtab_memaddr = g_auxv_base + strtab_addr - first_load_vaddr;
        }
        else
        {
            printf("Warning: Cannot determine strtab address (first_load_offset != 0)\n");
        }
    }
    if (symtab_memaddr == 0)
    {
        if (first_load_offset == 0)
        {
            // 如果第一个LOAD段的p_offset=0，可以使用简化公式
            symtab_memaddr = g_auxv_base + symtab_addr - first_load_vaddr;
        }
        else
        {
            printf("Warning: Cannot determine symtab address (first_load_offset != 0)\n");
        }
    }
    
    // 最后的回退（不应该到达这里）
    if (strtab_memaddr == 0)
    {
        printf("Error: Cannot determine strtab address\n");
        return 1;
    }
    if (symtab_memaddr == 0)
    {
        printf("Error: Cannot determine symtab address\n");
        return 1;
    }
    
    
    // 计算符号表大小
    // 简化方法：从hash表获取，或者通过strtab大小估算
    // 这里我们使用一个较大的值，实际应该从hash表获取
    uint32_t symnum = 2000;  // 临时值，实际应该从hash表获取
    
    const char *strtab = (const char *)strtab_memaddr;
    Elf64_Sym *symtab = (Elf64_Sym *)symtab_memaddr;
    
    // 验证地址有效性（简单检查）
    if ((uintptr_t)strtab < g_auxv_base || (uintptr_t)symtab < g_auxv_base)
    {
        printf("Warning: strtab or symtab address seems invalid\n");
    }
    
    // 处理重定位表
    if (rela_addr && rela_size && rela_ent)
    {
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
        
        
        for (uint32_t i = 0; i < rela_count; i++)
        {
            // 安全检查：确保rela地址有效
            if ((uintptr_t)&rela[i] < g_auxv_base || 
                (uintptr_t)&rela[i] > g_auxv_base + 0x200000) {
                printf("Warning: rela[%d] address out of range: 0x%x\n", 
                             i, (unsigned int)(uintptr_t)&rela[i]);
                break;
            }
            // 先读取rela[i]的字段，避免在printf中重复访问
            uint32_t rel_type = ELF64_R_TYPE(rela[i].r_info);
            uint64_t rel_offset = rela[i].r_offset;
            // printf("Processing relocation[%d]: type=%d, offset=0x%x\n",
            //              i, rel_type, (unsigned int)rel_offset);
            
            // 处理重定位
            process_relocation(&rela[i], g_auxv_base, symtab, strtab, symnum, (uint64_t *)got_addr, ehdr, phdr);
        }

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
        
        
        for (uint32_t i = 0; i < pltrel_count; i++) {
            process_relocation(&pltrel[i], g_auxv_base, symtab, strtab, symnum, (uint64_t *)got_addr, ehdr, phdr);
        }

    }
    
    g_bootstrap_done = 1;
    return 0;
}

/**
 * 从动态段中获取指定标签的值（无安全检查版本，用于可执行文件或其他库）
 */
static uint64_t get_dynamic_tag_unsafe(Elf64_Dyn *dynamic, int64_t tag)
{
    if (!dynamic)
    {
        return 0;
    }
    
    // 限制搜索范围，避免无限循环
    for (int i = 0; i < 1000; i++)
    {
        if (dynamic[i].d_tag == DT_NULL)
        {
            break;
        }
        
        if (dynamic[i].d_tag == tag)
        {
            return dynamic[i].d_val;
        }
    }
    
    return 0;
}

/**
 * 查找已加载的库
 */
static LoadedLibrary *find_loaded_library(const char *path)
{
    LoadedLibrary *lib = g_loaded_libraries;
    while (lib)
    {
        if (lib->path && strcmp(lib->path, path) == 0)
        {
            return lib;
        }
        lib = lib->next;
    }
    return NULL;
}

/**
 * 加载共享库
 */
static LoadedLibrary *load_shared_library(const char *libname)
{
    if (!libname)
    {
        printf("Error: Library name is NULL\n");
        return NULL;
    }

    // 检查是否已经加载
    LoadedLibrary *existing = find_loaded_library(libname);
    if (existing)
    {
        printf("Library %s already loaded\n", libname);
        return existing;
    }
    
    
    // 构建库文件路径
    // 简化实现：直接尝试几个常见路径
    char libpath[512];
    const char *search_paths[] = {
        "/work/github/multi_experiments/mini_libc/out/lib",
        "/work/github/multi_experiments/mini_libc/out/bin",
        "/lib",
        "/usr/lib",
        NULL
    };
    
    const char *libpath_found = NULL;
    for (int i = 0; search_paths[i] != NULL; i++)
    {
        snprintf(libpath, sizeof(libpath), "%s/%s", search_paths[i], libname);
        // 尝试打开文件
        int fd = open(libpath, O_RDONLY, 0);
        if (fd >= 0)
        {
            close(fd);
            libpath_found = libpath;
            break;
        }
    }
    
    if (!libpath_found)
    {
        printf("Error: Cannot find library %s\n", libname);
        return NULL;
    }
    
    
    // 打开文件
    int fd = open(libpath_found, O_RDONLY, 0);
    if (fd < 0)
    {
        printf("Error: Failed to open library %s\n", libpath_found);
        return NULL;
    }
    
    // 读取 ELF 头
    Elf64_Ehdr ehdr;
    ssize_t n = read(fd, &ehdr, sizeof(ehdr));
    if (n != sizeof(ehdr))
    {
        printf("Error: Failed to read ELF header\n");
        close(fd);
        return NULL;
    }
    
    // 验证 ELF 魔数
    if (ehdr.e_ident[0] != 0x7f || 
        ehdr.e_ident[1] != 'E' || 
        ehdr.e_ident[2] != 'L' || 
        ehdr.e_ident[3] != 'F')
    {
        printf("Error: Invalid ELF magic\n");
        close(fd);
        return NULL;
    }
    
    // 读取程序头表
    if (lseek(fd, ehdr.e_phoff, SEEK_SET) < 0)
    {
        printf("Error: Failed to seek to program header\n");
        close(fd);
        return NULL;
    }
    
    Elf64_Phdr *phdr = malloc(ehdr.e_phnum * sizeof(Elf64_Phdr));
    if (!phdr)
    {
        printf("Error: Failed to allocate memory for program headers\n");
        close(fd);
        return NULL;
    }
    
    n = read(fd, phdr, ehdr.e_phnum * sizeof(Elf64_Phdr));
    if (n != (ssize_t)(ehdr.e_phnum * sizeof(Elf64_Phdr)))
    {
        printf("Error: Failed to read program headers\n");
        free(phdr);
        close(fd);
        return NULL;
    }
    
    // 查找第一个 LOAD 段
    uint64_t first_load_vaddr = 0;
    for (uint16_t i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdr[i].p_type == PT_LOAD)
        {
            first_load_vaddr = phdr[i].p_vaddr;
            break;
        }
    }
    
    // 计算所需的总内存大小
    uint64_t max_vaddr = 0;
    uint64_t max_memsz = 0;
    for (uint16_t i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdr[i].p_type == PT_LOAD)
        {
            uint64_t end = phdr[i].p_vaddr + phdr[i].p_memsz;
            if (end > max_vaddr + max_memsz)
            {
                max_vaddr = phdr[i].p_vaddr;
                max_memsz = end - phdr[i].p_vaddr;
            }
        }
    }
    
    // 页对齐
    uint64_t total_size = ((max_vaddr - first_load_vaddr + max_memsz + 4095) / 4096) * 4096;
    
    // 让内核选择基址，先映射匿名内存
    void *base_addr = mmap(NULL, total_size, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base_addr == MAP_FAILED || !base_addr)
    {
        printf("Error: Failed to allocate memory for library\n");
        free(phdr);
        close(fd);
        return NULL;
    }
    
    
    // 映射所有 LOAD 段
    for (uint16_t i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdr[i].p_type == PT_LOAD)
        {
            uint64_t offset = phdr[i].p_vaddr - first_load_vaddr;
            void *load_addr = (char *)base_addr + offset;
            
            // 页对齐
            uint64_t page_mask = 4095;
            uint64_t load_addr_aligned = ((uint64_t)load_addr) & ~page_mask;
            uint64_t offset_in_page = ((uint64_t)load_addr) & page_mask;
            uint64_t file_offset_aligned = phdr[i].p_offset & ~page_mask;
            
            uint64_t map_size = ((phdr[i].p_memsz + offset_in_page + 4095) / 4096) * 4096;
            
            int prot = 0;
            if (phdr[i].p_flags & 0x1) prot |= PROT_EXEC;
            if (phdr[i].p_flags & 0x2) prot |= PROT_WRITE;
            if (phdr[i].p_flags & 0x4) prot |= PROT_READ;
            
            // 映射段
            
            void *mapped = mmap((void *)load_addr_aligned, map_size, prot,
                               MAP_PRIVATE | MAP_FIXED, fd, file_offset_aligned);
            if (mapped != (void *)load_addr_aligned)
            {
                printf("Error: Failed to map LOAD segment %d\n", i);
                munmap(base_addr, total_size);
                free(phdr);
                close(fd);
                return NULL;
            }
        }
    }
    
    close(fd);
    
    // 查找动态段
    Elf64_Dyn *dynamic = NULL;
    uint64_t actual_base = (uint64_t)(uintptr_t)base_addr;
    
    for (uint16_t i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdr[i].p_type == PT_DYNAMIC)
        {
            uint64_t dynamic_vaddr = phdr[i].p_vaddr;
            uint64_t dynamic_addr = actual_base + (dynamic_vaddr - first_load_vaddr);
            
            // 验证地址是否在合理范围内
            if (dynamic_addr < actual_base || dynamic_addr > actual_base + total_size)
            {
                printf("  Error: Dynamic segment address out of range\n");
                break;
            }
            
            dynamic = (Elf64_Dyn *)dynamic_addr;
            
            // 验证地址是否可读（使用更安全的方式）
            
            // 使用 volatile 指针确保读取
            volatile Elf64_Dyn *test_dyn = (volatile Elf64_Dyn *)dynamic;
            volatile uint64_t test_tag = test_dyn->d_tag;
            (void)test_tag;  // 避免未使用变量警告
            break;
        }
    }
    
    if (!dynamic)
    {
        printf("Error: Dynamic segment not found\n");
        munmap(base_addr, total_size);
        free(phdr);
        return NULL;
    }
    
    // 获取动态段信息
    uint64_t strtab_addr = get_dynamic_tag_unsafe(dynamic, DT_STRTAB);
    uint64_t symtab_addr = get_dynamic_tag_unsafe(dynamic, DT_SYMTAB);
    
    // 计算字符串表和符号表的实际地址
    const char *strtab = NULL;
    Elf64_Sym *symtab = NULL;
    uint32_t symnum = 1000;  // 默认值
    
    if (strtab_addr)
    {
        strtab = (const char *)(actual_base + (strtab_addr - first_load_vaddr));
    }
    
    if (symtab_addr)
    {
        symtab = (Elf64_Sym *)(actual_base + (symtab_addr - first_load_vaddr));
    }
    
    // 创建 LoadedLibrary 结构
    LoadedLibrary *lib = malloc(sizeof(LoadedLibrary));
    if (!lib)
    {
        printf("Error: Failed to allocate memory for LoadedLibrary\n");
        munmap(base_addr, total_size);
        free(phdr);
        return NULL;
    }
    
    // 复制路径字符串
    size_t path_len = strlen(libpath_found) + 1;
    lib->path = malloc(path_len);
    if (lib->path)
    {
        memcpy(lib->path, libpath_found, path_len);
    }
    lib->base_addr = (uint64_t)(uintptr_t)base_addr;
    lib->ehdr = (Elf64_Ehdr *)base_addr;  // 简化：假设ELF头在基址
    lib->phdr = phdr;
    lib->dynamic = dynamic;
    lib->strtab = strtab;
    lib->symtab = symtab;
    lib->symnum = symnum;
    lib->init = get_dynamic_tag_unsafe(dynamic, DT_INIT);
    lib->init_array = get_dynamic_tag_unsafe(dynamic, DT_INIT_ARRAY);
    lib->init_array_size = get_dynamic_tag_unsafe(dynamic, DT_INIT_ARRAYSZ);
    lib->next = g_loaded_libraries;
    g_loaded_libraries = lib;
    
    
    // 处理库的重定位
    
    // 阶段1：处理 RELA 重定位
    uint64_t rela_addr = get_dynamic_tag_unsafe(dynamic, DT_RELA);
    uint64_t rela_size = get_dynamic_tag_unsafe(dynamic, DT_RELASZ);
    uint64_t rela_ent = get_dynamic_tag_unsafe(dynamic, DT_RELAENT);
    
    if (rela_addr && rela_size && rela_ent)
    {
        uint32_t rela_count = rela_size / rela_ent;
        
        // 计算重定位表的实际内存地址
        uint64_t rela_memaddr = calculate_address_from_vaddr(rela_addr, actual_base, phdr, ehdr.e_phnum);
        if (rela_memaddr == 0)
        {
            // 回退：使用简化方法
            printf("Error: Failed to calculate rela address\n");
            // 清理已分配的资源
            g_loaded_libraries = lib->next;  // 从链表中移除
            if (lib->path)
            {
                free(lib->path);
            }
            free(lib);
            munmap(base_addr, total_size);
            free(phdr);
            return NULL;
        }
        
        
        Elf64_Rela *rela = (Elf64_Rela *)rela_memaddr;
        
        // 处理每个重定位条目
        for (uint32_t i = 0; i < rela_count; i++)
        {
            // 安全检查
            if ((uintptr_t)&rela[i] < actual_base || 
                (uintptr_t)&rela[i] > actual_base + total_size)
            {
                printf("  Warning: rela[%d] address out of range\n", i);
                break;
            }
            
            // 使用 process_relocation 处理重定位
            process_relocation(&rela[i], actual_base, symtab, strtab, symnum, NULL, &ehdr, phdr);
        }
        
    }
    
    // 阶段2：处理 PLT 重定位（JUMP_SLOT）
    uint64_t pltrel_addr = get_dynamic_tag_unsafe(dynamic, DT_JMPREL);
    uint64_t pltrel_size = get_dynamic_tag_unsafe(dynamic, DT_PLTRELSZ);
    uint64_t got_addr = get_dynamic_tag_unsafe(dynamic, DT_PLTGOT);
    
    if (pltrel_addr && pltrel_size && rela_ent) {
        uint32_t pltrel_count = pltrel_size / rela_ent;
        
        // 计算PLT重定位表的实际内存地址
        uint64_t pltrel_memaddr = calculate_address_from_vaddr(pltrel_addr, actual_base, phdr, ehdr.e_phnum);
        if (pltrel_memaddr == 0) {
            // 回退：使用简化方法
            pltrel_memaddr = actual_base + (pltrel_addr - first_load_vaddr);
        }
        
        
        Elf64_Rela *pltrel = (Elf64_Rela *)pltrel_memaddr;
        
        // 计算GOT的实际地址
        uint64_t got_memaddr = 0;
        if (got_addr) {
            got_memaddr = calculate_address_from_vaddr(got_addr, actual_base, phdr, ehdr.e_phnum);
            if (got_memaddr == 0) {
                got_memaddr = actual_base + (got_addr - first_load_vaddr);
            }
        }
        
        // 处理每个PLT重定位条目
        for (uint32_t i = 0; i < pltrel_count; i++) {
            // 安全检查
            if ((uintptr_t)&pltrel[i] < actual_base || 
                (uintptr_t)&pltrel[i] > actual_base + total_size) {
                printf("  Warning: pltrel[%d] address out of range\n", i);
                break;
            }
            
            // 使用 process_relocation 处理PLT重定位
            process_relocation(&pltrel[i], actual_base, symtab, strtab, symnum, 
                             (uint64_t *)got_memaddr, &ehdr, phdr);
        }
        
        
        // 额外保障：逐条填充 GOT，避免遗漏
        for (uint32_t i = 0; i < pltrel_count; i++) {
            Elf64_Rela *rela = &pltrel[i];
            uint64_t sym_idx = ELF64_R_SYM(rela->r_info);
            
            if (sym_idx < symnum && symtab && strtab) {
                Elf64_Sym *sym_entry = &symtab[sym_idx];
                uint64_t sym_addr = calculate_symbol_address(sym_entry, actual_base, phdr, ehdr.e_phnum);
                if (sym_addr == 0) {
                    sym_addr = actual_base + sym_entry->st_value;
                }
                
                void *got_entry = (void *)calculate_address_from_vaddr(
                    rela->r_offset, actual_base, phdr, ehdr.e_phnum);
                if (got_entry) {
                    *(uint64_t *)got_entry = sym_addr + rela->r_addend;
                }
            }
        }
    } else {
        printf("  No PLT relocations found\n");
    }

    return lib;
}

__attribute__((visibility("hidden")))
int main(void *stack_pointer)
{
    // 保存原始栈指针（用于跳转到可执行文件入口点）
    void *original_stack_pointer = stack_pointer;
    
    int argc;
    char **argv;
    char **envp;
    uint64_t *auxv;

    // 从栈指针解析所有参数
    parse_stack(stack_pointer, &argc, &argv, &envp, &auxv);
    
    // 保存环境变量指针
    g_environ = envp;
    
    // 解析辅助向量 (auxv)
    parse_auxv(auxv);
    
    
    // 自举动态链接器
    if (bootstrap_self())
    {
        printf("Error: Bootstrap failed!\n");
        return 1;
    }

    
    Elf64_Phdr *exec_phdr = (Elf64_Phdr *)g_auxv_phdr;
    printf("exec_phdr: 0x%x\n", (uintptr_t)exec_phdr);
    
    // 读取ELF头（程序头表之前64字节）
    Elf64_Ehdr *exec_ehdr = (Elf64_Ehdr *)((uintptr_t)g_auxv_phdr - 64);
    printf("exec_ehdr: 0x%x\n", (uintptr_t)exec_ehdr);
    // 验证是否是有效的ELF头
    if (exec_ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        exec_ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        exec_ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        exec_ehdr->e_ident[EI_MAG3] != ELFMAG3)
    {
        printf("Error: Invalid ELF header\n");
        return 1;
    }

    // 查找可执行文件的动态段
    Elf64_Dyn *exec_dynamic = NULL;
    uint64_t exec_first_load_actual = (uint64_t)exec_ehdr;
    
    for (uint16_t i = 0; i < g_auxv_phnum; i++)
    {
        if (exec_phdr[i].p_type == PT_DYNAMIC)
        {
            // 计算动态段地址
            uint64_t dynamic_vaddr = exec_phdr[i].p_vaddr;
            // 使用 calculate_address_from_vaddr 计算实际地址
            // 这样可以正确处理跨LOAD段的情况
            dynamic_vaddr = calculate_address_from_vaddr(
                exec_phdr[i].p_vaddr, exec_first_load_actual, exec_phdr, g_auxv_phnum);
            if (dynamic_vaddr == 0)
            {
                printf("Error: Failed to calculate dynamic segment address\n");
                return 1;
            }

            exec_dynamic = (Elf64_Dyn *)dynamic_vaddr;
            printf("exec_dynamic: 0x%x\n", (uintptr_t)exec_dynamic);
            break;
        }
    }

        
    // 遍历动态段，查找 DT_NEEDED 条目
    for (int i = 0; i < 1000; i++)
    {
        volatile Elf64_Dyn *d = &exec_dynamic[i];
        volatile int64_t tag = d->d_tag;
        
        if (tag == DT_NULL)
        {
            break;
        }
        
        if (tag == DT_NEEDED)
        {
            // 获取库名称
            // DT_NEEDED 的 d_val 是字符串表索引
            uint64_t strtab_addr = 0;
            uint64_t strtab_offset = d->d_val;
            
            // 查找 DT_STRTAB
            for (int j = 0; j < 1000; j++)
            {
                volatile Elf64_Dyn *d2 = &exec_dynamic[j];
                volatile int64_t tag2 = d2->d_tag;
                if (tag2 == DT_NULL)
                {
                    break;
                }
                if (tag2 == DT_STRTAB) // 找到字符串表
                {
                    strtab_addr = d2->d_val;
                    break;
                }
            }
            
            if (strtab_addr != 0)
            {
                // 计算字符串表的实际内存地址
                uint64_t strtab_memaddr = calculate_address_from_vaddr(
                    strtab_addr, exec_first_load_actual, exec_phdr, g_auxv_phnum);
                if (strtab_memaddr == 0)
                {
                    strtab_memaddr = strtab_addr;
                }
                
                const char *libname = (const char *)(strtab_memaddr + strtab_offset);
                
                // 加载共享库
                LoadedLibrary *lib = load_shared_library(libname);
                if (!lib)
                {
                    printf("  Error: Failed to load library %s\n", libname);
                    return 1;
                }
            }
        }
    }

    // 处理可执行文件的重定位
    
    if (exec_dynamic)
    {
        // 获取可执行文件的重定位信息
        uint64_t pltrel_addr = 0;
        uint64_t pltrel_size = 0;
        uint64_t got_addr = 0;
        
        for (int i = 0; i < 1000; i++)
        {
            volatile Elf64_Dyn *d = &exec_dynamic[i];
            volatile int64_t tag = d->d_tag;
            if (tag == DT_NULL)
            {
                break;
            }
            if (tag == DT_JMPREL) // 找到PLT重定位表
            {
                pltrel_addr = d->d_val;
            }
            if (tag == DT_PLTRELSZ) // 找到PLT重定位表大小
            {
                pltrel_size = d->d_val;
            }
            if (tag == DT_PLTGOT) // 找到GOT的虚拟地址
            {
                got_addr = d->d_val;
            }
        }
        
        if (pltrel_addr && pltrel_size && got_addr)
        {
            
            // 获取可执行文件的符号表和字符串表
            uint64_t exec_strtab_addr = 0;
            uint64_t exec_symtab_addr = 0;
            uint64_t exec_rela_ent = 24;  // RELA 条目大小通常是 24 字节
            
            for (int i = 0; i < 1000; i++) // 遍历动态段，查找DT_STRTAB、DT_SYMTAB、DT_RELAENT
            {
                volatile Elf64_Dyn *d = &exec_dynamic[i];
                volatile int64_t tag = d->d_tag;
                if (tag == DT_NULL)
                {
                    break;
                }
                if (tag == DT_STRTAB) // 找到字符串表
                {
                    exec_strtab_addr = d->d_val;
                }
                if (tag == DT_SYMTAB) // 找到符号表
                {
                    exec_symtab_addr = d->d_val;
                }
                if (tag == DT_RELAENT) // 找到RELA条目大小
                {
                    exec_rela_ent = d->d_val;
                }
            }
            
            if (exec_strtab_addr && exec_symtab_addr && exec_rela_ent)
            {
                
                // 计算可执行文件的字符串表、符号表和PLT重定位表的实际内存地址
                // 使用 calculate_address_from_vaddr 以支持 PIE 和非 PIE 可执行文件
                uint64_t exec_strtab_memaddr = calculate_address_from_vaddr(
                    exec_strtab_addr, exec_first_load_actual, exec_phdr, g_auxv_phnum);
                if (exec_strtab_memaddr == 0)
                {
                    // 对于非 PIE，虚拟地址就是实际地址
                    exec_strtab_memaddr = exec_strtab_addr;
                }
                
                uint64_t exec_symtab_memaddr = calculate_address_from_vaddr(
                    exec_symtab_addr, exec_first_load_actual, exec_phdr, g_auxv_phnum);
                if (exec_symtab_memaddr == 0)
                {
                    exec_symtab_memaddr = exec_symtab_addr;
                }
                
                uint64_t pltrel_memaddr = calculate_address_from_vaddr(
                    pltrel_addr, exec_first_load_actual, exec_phdr, g_auxv_phnum);
                if (pltrel_memaddr == 0)
                {
                    pltrel_memaddr = pltrel_addr;
                }
                
                const char *exec_strtab = (const char *)exec_strtab_memaddr;
                Elf64_Sym *exec_symtab = (Elf64_Sym *)exec_symtab_memaddr;
                Elf64_Rela *pltrel = (Elf64_Rela *)pltrel_memaddr;
                uint32_t pltrel_count = pltrel_size / exec_rela_ent;
                
                // 验证地址是否可读（使用更安全的方式）
                // 先检查地址是否对齐
                if ((uintptr_t)pltrel % 8 != 0) {
                    printf("  ERROR: pltrel address 0x%x is not 8-byte aligned\n",
                           (unsigned int)(uintptr_t)pltrel);
                    return 1;
                }
                
                // 使用 volatile 确保读取，并添加异常处理
                volatile Elf64_Rela *test_pltrel = (volatile Elf64_Rela *)pltrel;
                volatile uint64_t test_offset = 0;
                
                // 尝试读取，如果失败会触发段错误
                __asm__ __volatile__(
                    "ldr %0, [%1]"
                    : "=r" (test_offset)
                    : "r" (test_pltrel)
                    : "memory"
                );
                
                
                // 处理每个 PLT 重定位
                for (uint32_t i = 0; i < pltrel_count && i < 28; i++) {  // 限制为28个
                    volatile Elf64_Rela *rela = &pltrel[i];
                    
                    // 直接访问结构体字段（volatile 确保读取）
                    volatile uint64_t r_offset_val = rela->r_offset;
                    volatile uint64_t r_info_val = rela->r_info;
                    volatile int64_t r_addend_val = rela->r_addend;
                    
                    // 通用方法：计算重定位类型和符号索引（64 位 r_info）
                    uint32_t type = ELF64_R_TYPE(r_info_val);
                    uint64_t sym_idx = ELF64_R_SYM(r_info_val);
                    
                    // 处理 JUMP_SLOT 重定位
                    if (type == R_AARCH64_JUMP_SLOT && sym_idx > 0 && sym_idx < 100000) {
                        Elf64_Sym *sym_entry = &exec_symtab[sym_idx];
                        uint32_t st_name = sym_entry->st_name;
                        
                        if (st_name > 0 && st_name < 100000) {  // 合理的字符串表偏移
                            const char *symname = exec_strtab + st_name;
                            
                            // 使用全局符号解析函数从已加载的库中查找符号
                            uint64_t sym_addr = resolve_symbol_global(symname);
                            
                            if (sym_addr != 0) {
                                // 填充 GOT 条目
                                // r_offset_val 是可执行文件中的虚拟地址。
                                // 对于非 PIE 可执行文件，first_load_vaddr 即为实际装载基址，
                                // 使用它作为 base，可以得到真实内存地址（结果等于原始 vaddr）。
                                uint64_t got_entry_vaddr = r_offset_val;
                                void *got_entry = (void *)calculate_address_from_vaddr(
                                    got_entry_vaddr, exec_first_load_actual, exec_phdr, g_auxv_phnum);
                                
                                if (got_entry != NULL) {
                                    *(uint64_t *)got_entry = sym_addr;
                                }
                            }
                        }
                    }
                }
                
            }

        }
    }
    
    // 调用初始化函数
    LoadedLibrary *lib = g_loaded_libraries;
    while (lib) {
        if (lib->init) {
            
            // 计算初始化函数的实际地址
            uint64_t init_addr = 0;
            if (lib->phdr && lib->ehdr) {
                // 使用 calculate_address_from_vaddr 计算地址
                init_addr = calculate_address_from_vaddr(lib->init, lib->base_addr, 
                                                         lib->phdr, lib->ehdr->e_phnum);
            }
            if (init_addr == 0) {
                // 回退：使用简化方法
                init_addr = lib->base_addr + lib->init;
            }
            
            if (init_addr != 0) {
                // 调用初始化函数（无参数）
                void (*init_func)(void) = (void (*)(void))(uintptr_t)init_addr;
                init_func();
            } else {
                printf("  Warning: Cannot calculate init function address\n");
            }
        }
        
        // 处理初始化数组
        if (lib->init_array && lib->init_array_size > 0) {
            uint32_t init_array_count = lib->init_array_size / sizeof(uint64_t);
            
            for (uint32_t i = 0; i < init_array_count; i++) {
                uint64_t init_array_entry = lib->init_array + i * sizeof(uint64_t);
                uint64_t init_func_addr = 0;
                
                if (lib->phdr && lib->ehdr) {
                    init_func_addr = calculate_address_from_vaddr(init_array_entry, lib->base_addr,
                                                                  lib->phdr, lib->ehdr->e_phnum);
                }
                if (init_func_addr == 0) {
                    init_func_addr = lib->base_addr + init_array_entry;
                }
                
                // 读取初始化函数地址
                uint64_t *init_array_ptr = (uint64_t *)init_func_addr;
                uint64_t func_addr = *init_array_ptr;
                
                if (func_addr != 0) {
                    // 计算函数实际地址
                    uint64_t actual_func_addr = 0;
                    if (lib->phdr && lib->ehdr) {
                        actual_func_addr = calculate_address_from_vaddr(func_addr, lib->base_addr,
                                                                        lib->phdr, lib->ehdr->e_phnum);
                    }
                    if (actual_func_addr == 0) {
                        actual_func_addr = lib->base_addr + func_addr;
                    }
                    
                    if (actual_func_addr != 0) {
                        void (*init_func)(void) = (void (*)(void))(uintptr_t)actual_func_addr;
                        init_func();
                    }
                }
            }
        }
        
        lib = lib->next;
    }

    // 优先尝试直接调用可执行文件的 main（使用当前栈），避免再次切换栈导致未知问题
    uint64_t exec_main_addr = 0;
    if (exec_dynamic) {
        // 复用之前解析的符号表/字符串表
        uint64_t exec_strtab_addr = 0;
        uint64_t exec_symtab_addr = 0;
        for (int i = 0; i < 1000; i++) {
            volatile Elf64_Dyn *d = &exec_dynamic[i];
            volatile int64_t tag = d->d_tag;
            if (tag == DT_NULL) {
                break;
            }
            if (tag == DT_STRTAB) {
                exec_strtab_addr = d->d_val;
            }
            if (tag == DT_SYMTAB) {
                exec_symtab_addr = d->d_val;
            }
        }
        
        if (exec_strtab_addr && exec_symtab_addr) {
            // 计算可执行文件的字符串表和符号表的实际内存地址
            // 使用 calculate_address_from_vaddr 以支持 PIE 和非 PIE 可执行文件
            uint64_t exec_strtab_memaddr = calculate_address_from_vaddr(
                exec_strtab_addr, exec_first_load_actual, exec_phdr, g_auxv_phnum);
            if (exec_strtab_memaddr == 0) {
                // 回退：对于非 PIE，虚拟地址就是实际地址
                exec_strtab_memaddr = exec_strtab_addr;
            }
            
            uint64_t exec_symtab_memaddr = calculate_address_from_vaddr(
                exec_symtab_addr, exec_first_load_actual, exec_phdr, g_auxv_phnum);
            if (exec_symtab_memaddr == 0) {
                // 回退：对于非 PIE，虚拟地址就是实际地址
                exec_symtab_memaddr = exec_symtab_addr;
            }
            
            const char *exec_strtab = (const char *)exec_strtab_memaddr;
            Elf64_Sym *exec_symtab = (Elf64_Sym *)exec_symtab_memaddr;
            
            // 简单遍历符号表查找 main
            for (int i = 0; i < 5000; i++) {
                Elf64_Sym *sym = &exec_symtab[i];
                if (sym->st_name == 0) {
                    continue;
                }
                const char *name = exec_strtab + sym->st_name;
                if (name && name[0] == 'm' && name[1] == 'a' && name[2] == 'i' &&
                    name[3] == 'n' && name[4] == '\0') {
                    // 计算 main 的实际地址
                    exec_main_addr = calculate_address_from_vaddr(
                        sym->st_value, exec_first_load_actual, exec_phdr, g_auxv_phnum);
                    break;
                }
            }
        }
    }
    
    if (exec_main_addr) {
        // 直接使用当前栈调用 main，按照 AArch64 调用约定传递 argc/argv
        int (*exec_main_fn)(int, char **) = (int (*)(int, char **))(uintptr_t)exec_main_addr;
        int rc = exec_main_fn(argc, argv);
        return rc;
    }
    
    // 跳转到可执行文件入口点
    // 入口点函数从栈中读取 argc 和 argv：
    //   ldr x0, [sp, #0]  // argc
    //   add x1, sp, #8    // argv
    //   bl main
    
    // 确保栈指针 16 字节对齐（AArch64 要求）
    uintptr_t aligned_sp = ((uintptr_t)original_stack_pointer) & ~0xF;
    
    // 使用内联汇编恢复栈指针并跳转
    // 注意：入口点函数从栈中读取 argc 和 argv，所以我们需要恢复原始栈指针
    // 入口点函数：ldr x0, [sp, #0] (argc), add x1, sp, #8 (argv), bl main
    // 使用 blr 而不是 br，这样可以设置链接寄存器（虽然入口点不会返回）
    
    // 计算可执行文件的实际入口点地址
    // 对于PIE可执行文件：g_auxv_entry 是相对地址，需要加上实际加载基址
    // 对于非PIE可执行文件：g_auxv_entry 已经是绝对地址
    // 判断方法：如果第一个LOAD段的虚拟地址很小（< 0x10000），通常是PIE
    uint64_t entry_point_addr = g_auxv_entry;
    entry_point_addr = calculate_address_from_vaddr(g_auxv_entry, exec_first_load_actual, exec_phdr, g_auxv_phnum);
    if (entry_point_addr == 0) {
        printf("Error: Failed to calculate entry point address\n");
        return 1;
    }
    void *entry_point = (void *)entry_point_addr;
    // 注意：这是一个跳转操作，不会返回到这个函数
    // 因此我们修改栈指针是安全的，不需要在 clobber list 中列出 "sp"
    asm volatile(
        "mov sp, %0\n"           // 恢复原始栈指针（栈上已经有 argc 和 argv）
        "mov x29, xzr\n"         // 清除帧指针
        "mov x30, xzr\n"         // 清除链接寄存器（设置为 0，因为这是最终跳转）
        "blr %1\n"               // 跳转到入口点（使用 blr 设置链接寄存器）
        :
        : "r"(aligned_sp), "r"(entry_point)
        : "x29", "x30", "memory"
    );
    
    // 不应该到达这里
    printf("ERROR: Should not reach here after jumping to entry point!\n");
    return 1;
    
    // 如果跳转失败，返回
    return 0;
}