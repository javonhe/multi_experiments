#include "dl_runtime.h"

size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
    {
        len++;
    }
    return len;
}

void itoa(int64_t value, char *buf, int base)
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

    while (i > 0)
    {
        *p++ = temp[--i];
    }
    *p = '\0';
}

void utoa_hex(uint64_t value, char *buf)
{
    char *p = buf;

    if (value == 0)
    {
        *p++ = '0';
        *p = '\0';
        return;
    }

    char temp[32];
    int i = 0;
    while (value > 0)
    {
        int digit = value % 16;
        temp[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= 16;
    }

    while (i > 0)
    {
        *p++ = temp[--i];
    }
    *p = '\0';
}

int write(int fd, const void *buf, size_t count)
{
    long result;
    asm volatile(
        "mov x8, #0x40\n"
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x2, %3\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(result)
        : "r"(fd), "r"(buf), "r"(count)
        : "x0", "x1", "x2", "x8"
    );
    return (int)result;
}

int printf(const char *format, ...)
{
    char buffer[1024];
    char num_buf[32];
    int buf_pos = 0;

    __builtin_va_list args;
    __builtin_va_start(args, format);

    const char *p = format;

    while (*p != '\0' && buf_pos < (int)sizeof(buffer) - 1)
    {
        if (*p == '%')
        {
            p++;
            switch (*p)
            {
                case 's':
                {
                    const char *str = __builtin_va_arg(args, const char *);
                    if (str)
                    {
                        size_t len = strlen(str);
                        if (buf_pos + (int)len < (int)sizeof(buffer) - 1)
                        {
                            for (size_t i = 0; i < len; i++)
                            {
                                buffer[buf_pos++] = str[i];
                            }
                        }
                    }
                    break;
                }
                case 'd':
                {
                    int val = __builtin_va_arg(args, int);
                    itoa(val, num_buf, 10);
                    size_t len = strlen(num_buf);
                    if (buf_pos + (int)len < (int)sizeof(buffer) - 1)
                    {
                        for (size_t i = 0; i < len; i++)
                        {
                            buffer[buf_pos++] = num_buf[i];
                        }
                    }
                    break;
                }
                case 'x':
                {
                    unsigned int val = __builtin_va_arg(args, unsigned int);
                    utoa_hex(val, num_buf);
                    size_t len = strlen(num_buf);
                    if (buf_pos + (int)len < (int)sizeof(buffer) - 1)
                    {
                        for (size_t i = 0; i < len; i++)
                        {
                            buffer[buf_pos++] = num_buf[i];
                        }
                    }
                    break;
                }
                case 'p':
                {
                    void *val = __builtin_va_arg(args, void *);
                    buffer[buf_pos++] = '0';
                    buffer[buf_pos++] = 'x';
                    utoa_hex((uint64_t)val, num_buf);
                    size_t len = strlen(num_buf);
                    if (buf_pos + (int)len < (int)sizeof(buffer) - 1)
                    {
                        for (size_t i = 0; i < len; i++)
                        {
                            buffer[buf_pos++] = num_buf[i];
                        }
                    }
                    break;
                }
                case 'c':
                {
                    char c = (char)__builtin_va_arg(args, int);
                    if (buf_pos < (int)sizeof(buffer) - 1)
                    {
                        buffer[buf_pos++] = c;
                    }
                    break;
                }
                case '%':
                {
                    if (buf_pos < (int)sizeof(buffer) - 1)
                    {
                        buffer[buf_pos++] = '%';
                    }
                    break;
                }
                case '\n':
                {
                    if (buf_pos < (int)sizeof(buffer) - 1)
                    {
                        buffer[buf_pos++] = '\n';
                    }
                    break;
                }
                default:
                    if (buf_pos < (int)sizeof(buffer) - 1)
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

    if (buf_pos > 0)
    {
        return write(1, buffer, (size_t)buf_pos);
    }

    return 0;
}

int strncmp(const char *s1, const char *s2, size_t n)
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

    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *get_env(char **environ, const char *name)
{
    if (!environ || !name)
    {
        return NULL;
    }

    size_t name_len = strlen(name);

    for (char **env = environ; *env != NULL; env++)
    {
        if (strncmp(*env, name, name_len) == 0 && (*env)[name_len] == '=')
        {
            return (*env) + name_len + 1;
        }
    }

    return NULL;
}

void print_all_env(char **environ)
{
    if (!environ)
    {
        return;
    }

    printf("=== Environment Variables ===\n");
    for (char **env = environ; *env != NULL; env++)
    {
        printf("%s\n", *env);
    }
    printf("============================\n");
}

void *memcpy(void *dest, const void *src, size_t n)
{
    char *d = (char *)dest;
    const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++)
    {
        d[i] = s[i];
    }
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    char *p = (char *)s;
    for (size_t i = 0; i < n; i++)
    {
        p[i] = (char)c;
    }
    return s;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }

    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static int is_utf8_continuation(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

static int get_utf8_char_length(unsigned char c)
{
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

int vsprintf(char *buf, const char *format, va_list args)
{
    char *str = buf;
    char num_buf[32];
    const char *s = format;

    while (*s)
    {
        if (*s != '%')
        {
            int char_len = get_utf8_char_length(*s);
            for (int i = 0; i < char_len && *s; i++)
            {
                *str++ = *s++;
            }
            continue;
        }

        s++;

        switch (*s)
        {
            case 'd':
            {
                int value = va_arg(args, int);
                itoa(value, num_buf, 10);
                size_t len = strlen(num_buf);
                memcpy(str, num_buf, len);
                str += len;
                break;
            }

            case 'x':
            {
                unsigned long value = va_arg(args, unsigned long);
                utoa_hex((uint64_t)value, num_buf);
                size_t len = strlen(num_buf);
                memcpy(str, num_buf, len);
                str += len;
                break;
            }

            case 'l':
                if (*(s + 1) == 'd')
                {
                    long value = va_arg(args, long);
                    itoa(value, num_buf, 10);
                    size_t len = strlen(num_buf);
                    memcpy(str, num_buf, len);
                    str += len;
                    s++;
                }
                break;

            case 's':
            {
                char *p = va_arg(args, char *);
                while (*p)
                {
                    int char_len = get_utf8_char_length(*p);
                    for (int i = 0; i < char_len && *p; i++)
                    {
                        *str++ = *p++;
                    }
                }
                break;
            }

            default:
                *str++ = *s;
                break;
        }

        s++;
    }

    *str = '\0';
    return (int)(str - buf);
}

int vsnprintf(char *buf, size_t size, const char *format, va_list args)
{
    if (!buf || !format || size == 0)
    {
        return -1;
    }

    char temp[1024];
    int ret = vsprintf(temp, format, args);

    if (ret < 0 || (size_t)ret >= size)
    {
        return -1;
    }

    memcpy(buf, temp, (size_t)ret);
    buf[ret] = '\0';

    return ret;
}

int snprintf(char *buf, size_t size, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = vsnprintf(buf, size, format, args);
    va_end(args);

    return ret;
}

int open(const char *pathname, int flags, int mode)
{
    int result;
    int dirfd = AT_FDCWD;
    asm volatile(
        "mov x8, #56\n"
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x2, %3\n"
        "mov x3, %4\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(result)
        : "r"(dirfd), "r"(pathname), "r"(flags), "r"(mode)
        : "x0", "x1", "x2", "x3", "x8"
    );

    return result;
}

int close(int fd)
{
    int result;
    asm volatile(
        "mov x8, #57\n"
        "mov x0, %1\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(result)
        : "r"(fd)
        : "x0", "x8"
    );

    return result;
}

ssize_t read(int fd, void *buf, size_t count)
{
    register long x8 asm("x8") = 63;
    register long x0 asm("x0") = fd;
    register long x1 asm("x1") = (long)buf;
    register long x2 asm("x2") = (long)count;

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory", "cc"
    );

    if (x0 < 0)
    {
        return -1;
    }

    return x0;
}

int lseek(int fd, int offset, int whence)
{
    int result;
    asm volatile(
        "mov x8, #62\n"
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x2, %3\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(result)
        : "r"(fd), "r"(offset), "r"(whence)
        : "x0", "x1", "x2", "x8"
    );

    return result;
}

static void *__mmap_raw(void *addr, long size, int prot, int flags, int fd, long offset)
{
    void *result;
    asm volatile(
        "mov x8, #222\n"
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x2, %3\n"
        "mov x3, %4\n"
        "mov x4, %5\n"
        "mov x5, %6\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(result)
        : "r"(addr), "r"(size), "r"(prot), "r"(flags), "r"(fd), "r"(offset)
        : "x0", "x1", "x2", "x3", "x4", "x5", "x8"
    );

    return result;
}

void *mmap(void *addr, long size, int prot, int flags, int fd, long offset)
{
    if (offset < 0)
    {
        return NULL;
    }

    long rounded = (long)__MINI_ALIGN((unsigned long)size, 4096);
    if (rounded < size)
    {
        return NULL;
    }

    return __mmap_raw(addr, size, prot, flags, fd, offset);
}

int munmap(void *addr, long size)
{
    int result;

    asm volatile(
        "mov x8, #215\n"
        "mov x0, %1\n"
        "mov x1, %2\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(result)
        : "r"(addr), "r"(size)
        : "x0", "x1", "x8"
    );

    return result;
}

#define MIN_BLOCK_SIZE 64
#define MAX_ORDER 32
#define INITIAL_POOL_SIZE (1 << 20)
#define EXPANSION_FACTOR 2

typedef struct block {
    size_t size;
    int order;
    int is_free;
    struct block *next;
    struct block *buddy;
} block_t;

typedef struct {
    block_t *free_lists[MAX_ORDER];
    void *heap_start;
    size_t heap_size;
} buddy_allocator_t;

static buddy_allocator_t *global_allocator = NULL;

static void merge_blocks(buddy_allocator_t *allocator, block_t *block);
static buddy_allocator_t *buddy_init(size_t initial_size);
static int expand_memory_pool(buddy_allocator_t *allocator, size_t required_size);
static block_t *split_block(buddy_allocator_t *allocator, block_t *block, int target_order);
static int get_order(size_t size);
static buddy_allocator_t *ensure_allocator_init(void);

static int get_order(size_t size)
{
    int order = 0;
    size_t block_size = MIN_BLOCK_SIZE;

    while (block_size < size && order < MAX_ORDER)
    {
        block_size *= 2;
        order++;
    }
    return order;
}

static buddy_allocator_t *buddy_init(size_t initial_size)
{
    if (initial_size < MIN_BLOCK_SIZE)
    {
        initial_size = MIN_BLOCK_SIZE;
    }

    buddy_allocator_t *allocator = mmap(NULL, sizeof(buddy_allocator_t),
                                        PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!allocator || allocator == MAP_FAILED)
    {
        return NULL;
    }

    memset(allocator->free_lists, 0, sizeof(allocator->free_lists));

    allocator->heap_size = initial_size;
    allocator->heap_start = mmap(NULL, (long)initial_size,
                                 PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!allocator->heap_start || allocator->heap_start == MAP_FAILED)
    {
        return NULL;
    }

    block_t *initial_block = (block_t *)allocator->heap_start;
    initial_block->size = initial_size - sizeof(block_t);
    initial_block->order = get_order(initial_size);
    initial_block->is_free = 1;
    initial_block->next = NULL;
    initial_block->buddy = NULL;

    allocator->free_lists[initial_block->order] = initial_block;

    return allocator;
}

static int expand_memory_pool(buddy_allocator_t *allocator, size_t required_size)
{
    size_t new_size = allocator->heap_size * EXPANSION_FACTOR;
    while (new_size < required_size)
    {
        new_size *= EXPANSION_FACTOR;
    }

    void *new_heap = mmap(NULL, (long)new_size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_heap == MAP_FAILED)
    {
        return 0;
    }

    block_t *new_block = (block_t *)new_heap;
    new_block->size = new_size - sizeof(block_t);
    new_block->order = get_order(new_size);
    new_block->is_free = 1;
    new_block->next = NULL;
    new_block->buddy = NULL;

    if (allocator->heap_start)
    {
        int order = new_block->order;
        new_block->next = allocator->free_lists[order];
        allocator->free_lists[order] = new_block;
        merge_blocks(allocator, new_block);
        allocator->heap_size += new_size;
    }
    else
    {
        allocator->heap_start = new_heap;
        allocator->heap_size = new_size;
        allocator->free_lists[new_block->order] = new_block;
    }

    return 1;
}

static block_t *split_block(buddy_allocator_t *allocator, block_t *block, int target_order)
{
    while (block->order > target_order)
    {
        int new_order = block->order - 1;
        size_t block_size = (1UL << new_order) * MIN_BLOCK_SIZE;

        uintptr_t block_addr = (uintptr_t)block;
        uintptr_t buddy_addr = block_addr + block_size;

        if (buddy_addr + block_size > (uintptr_t)allocator->heap_start + allocator->heap_size)
        {
            break;
        }

        block_t *buddy = (block_t *)buddy_addr;

        block->size = block_size - sizeof(block_t);
        block->order = new_order;
        block->buddy = buddy;

        buddy->size = block_size - sizeof(block_t);
        buddy->order = new_order;
        buddy->is_free = 1;
        buddy->next = allocator->free_lists[new_order];
        buddy->buddy = block;

        allocator->free_lists[new_order] = buddy;
    }
    return block;
}

static void merge_blocks(buddy_allocator_t *allocator, block_t *block)
{
    while (block->order < MAX_ORDER - 1)
    {
        block_t *buddy = block->buddy;

        if (!buddy || !buddy->is_free || buddy->order != block->order)
        {
            break;
        }

        size_t block_size = (1UL << block->order) * MIN_BLOCK_SIZE;
        uintptr_t block_addr = (uintptr_t)block;
        uintptr_t buddy_addr = (uintptr_t)buddy;

        if (buddy_addr < block_addr)
        {
            block_t *temp = block;
            block = buddy;
            buddy = temp;
            block_addr = (uintptr_t)block;
            buddy_addr = (uintptr_t)buddy;
        }

        if (buddy_addr != block_addr + block_size)
        {
            break;
        }

        block_t **list = &allocator->free_lists[buddy->order];
        while (*list && *list != buddy)
        {
            list = &(*list)->next;
        }
        if (*list)
        {
            *list = buddy->next;
        }

        block->order++;
        block->size = (1UL << block->order) * MIN_BLOCK_SIZE - sizeof(block_t);

        block_size = (1UL << block->order) * MIN_BLOCK_SIZE;
        buddy_addr = block_addr + block_size;

        if (buddy_addr < (uintptr_t)allocator->heap_start + allocator->heap_size)
        {
            block->buddy = (block_t *)buddy_addr;
        }
        else
        {
            block->buddy = NULL;
        }
    }

    block->next = allocator->free_lists[block->order];
    allocator->free_lists[block->order] = block;
}

static buddy_allocator_t *ensure_allocator_init(void)
{
    if (!global_allocator)
    {
        global_allocator = buddy_init(INITIAL_POOL_SIZE);
    }
    return global_allocator;
}

void *malloc(size_t size)
{
    buddy_allocator_t *allocator = ensure_allocator_init();
    if (!allocator)
    {
        return NULL;
    }

    size_t total_size = size + sizeof(block_t);

    if (total_size > allocator->heap_size)
    {
        if (!expand_memory_pool(allocator, total_size))
        {
            return NULL;
        }
    }

    int order = get_order(total_size);
    int current_order = order;
    block_t *block = NULL;

    while (current_order < MAX_ORDER)
    {
        if (allocator->free_lists[current_order])
        {
            block = allocator->free_lists[current_order];
            allocator->free_lists[current_order] = block->next;
            break;
        }
        current_order++;
    }

    if (!block)
    {
        if (!expand_memory_pool(allocator, total_size))
        {
            return NULL;
        }
        return malloc(size);
    }

    if (block && current_order > order && current_order < MAX_ORDER)
    {
        block = split_block(allocator, block, order);
    }

    block->is_free = 0;
    block->next = NULL;

    return (void *)((char *)block + sizeof(block_t));
}

void free(void *ptr)
{
    if (!ptr || !global_allocator)
    {
        return;
    }

    block_t *block = (block_t *)((char *)ptr - sizeof(block_t));
    block->is_free = 1;
    merge_blocks(global_allocator, block);
}
