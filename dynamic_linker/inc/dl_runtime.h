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

#define EI_NIDENT 16

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
