#include <mini_lib.h>

// 基本配置参数
#define MIN_BLOCK_SIZE 64            
#define MAX_ORDER 32                 
#define INITIAL_POOL_SIZE (1 << 20)  
#define EXPANSION_FACTOR 2           

/**
 * 内存块结构体
 * 每个内存块的元数据信息，位于实际可用内存之前
 */
typedef struct block {
    size_t size;              // 块的实际可用大小（不包含头部）
    int order;                // 块的阶数，表示大小为 2^order * MIN_BLOCK_SIZE
    int is_free;             // 标记块是否空闲
    struct block* next;       // 指向同一order中下一个空闲块
    struct block* buddy;      // 指向该块的伙伴块
} block_t;

/**
 * 内存分配器结构体
 * 管理整个内存池和空闲块链表
 */
typedef struct {
    block_t* free_lists[MAX_ORDER];  // 每个order的空闲块链表头
    void* heap_start;                // 堆内存起始地址
    size_t heap_size;                // 当前堆的总大小
} buddy_allocator_t;

// 全局唯一的分配器实例
static buddy_allocator_t* global_allocator = NULL;

// 前向声明所有静态函数
static void merge_blocks(buddy_allocator_t* allocator, block_t* block);
static buddy_allocator_t* buddy_init(size_t initial_size);
static int expand_memory_pool(buddy_allocator_t* allocator, size_t required_size);
static block_t* split_block(buddy_allocator_t* allocator, block_t* block, int target_order);
static int get_order(size_t size);
static buddy_allocator_t* ensure_allocator_init(void);

/**
 * 计算给定大小所需的order
 * @param size: 需要的内存大小
 * @return: 满足size大小的最小order值
 */
static int get_order(size_t size) 
{
    int order = 0;
    size_t block_size = MIN_BLOCK_SIZE;
    
    // 找到第一个大于等于所需size的2的幂次
    while (block_size < size && order < MAX_ORDER) 
    {
        block_size *= 2;
        order++;
    }
    return order;
}

/**
 * 内存分配器初始化
 * @param initial_size: 初始内存池大小
 * @return: 初始化好的分配器指针，失败返回NULL
 */
static buddy_allocator_t* buddy_init(size_t initial_size) 
{
    // 确保初始大小至少为最小块大小
    if (initial_size < MIN_BLOCK_SIZE) 
    {
        initial_size = MIN_BLOCK_SIZE;
    }
    
    // 分配分配器结构体空间
    buddy_allocator_t* allocator = mmap(NULL, sizeof(buddy_allocator_t),
                                      PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    // 初始化空闲链表数组
    memset(allocator->free_lists, 0, sizeof(allocator->free_lists));
    
    // 分配初始内存池
    allocator->heap_size = initial_size;
    allocator->heap_start = mmap(NULL, initial_size,
                                PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    // 初始化第一个内存块
    block_t* initial_block = (block_t*)allocator->heap_start;
    initial_block->size = initial_size - sizeof(block_t);
    initial_block->order = get_order(initial_size);
    initial_block->is_free = 1;
    initial_block->next = NULL;
    initial_block->buddy = NULL;
    
    // 将初始块加入对应order的空闲链表
    allocator->free_lists[initial_block->order] = initial_block;
    
    return allocator;
}

/**
 * 扩展内存池
 * @param allocator: 分配器实例
 * @param required_size: 需要的最小大小
 * @return: 成功返回1，失败返回0
 */
static int expand_memory_pool(buddy_allocator_t* allocator, size_t required_size) 
{
    // 计算新的内存池大小，确保足够大
    size_t new_size = allocator->heap_size * EXPANSION_FACTOR;
    while (new_size < required_size) 
    {
        new_size *= EXPANSION_FACTOR;
    }

    // 分配新的内存空间
    void* new_heap = mmap(NULL, new_size,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_heap == MAP_FAILED) 
    {
        return 0;
    }

    // 初始化新的内存块
    block_t* new_block = (block_t*)new_heap;
    new_block->size = new_size - sizeof(block_t);
    new_block->order = get_order(new_size);
    new_block->is_free = 1;
    new_block->next = NULL;
    new_block->buddy = NULL;

    // 将新块整合到现有系统
    if (allocator->heap_start) 
    {
        // 如果不是首次扩展，添加到对应order的空闲链表
        int order = new_block->order;
        new_block->next = allocator->free_lists[order];
        allocator->free_lists[order] = new_block;
        merge_blocks(allocator, new_block);  // 尝试与相邻块合并
        allocator->heap_size += new_size;
    } 
    else 
    {
        // 首次扩展，直接设置为堆的起始
        allocator->heap_start = new_heap;
        allocator->heap_size = new_size;
        allocator->free_lists[new_block->order] = new_block;
    }

    return 1;
}

/**
 * 分割内存块
 * 将大块分割成更小的块，直到达到目标order
 */
static block_t* split_block(buddy_allocator_t* allocator, block_t* block, int target_order) 
{
    while (block->order > target_order) 
    {
        // 计算新的order和大小
        int new_order = block->order - 1;
        size_t new_size = (1 << new_order) * MIN_BLOCK_SIZE;
        
        // 创建伙伴块
        block_t* buddy = (block_t*)((char*)block + new_size);
        buddy->size = new_size - sizeof(block_t);
        buddy->order = new_order;
        buddy->is_free = 1;
        buddy->next = allocator->free_lists[new_order];
        buddy->buddy = block;
        
        // 更新原块信息
        block->size = new_size - sizeof(block_t);
        block->order = new_order;
        block->buddy = buddy;
        
        // 将新的伙伴块加入空闲链表
        allocator->free_lists[new_order] = buddy;
    }
    return block;
}

/**
 * 合并内存块
 * 尝试将空闲块与其伙伴块合并，直到无法继续合并
 */
static void merge_blocks(buddy_allocator_t* allocator, block_t* block) 
{
    while (block->order < MAX_ORDER - 1) 
    {
        block_t* buddy = block->buddy;
        
        // 检查是否可以合并
        if (!buddy || !buddy->is_free || buddy->order != block->order) 
        {
            break;
        }

        // 验证伙伴块是否在有效范围内
        void* buddy_end = (char*)buddy + (1 << buddy->order) * MIN_BLOCK_SIZE;
        if ((void*)buddy < allocator->heap_start || 
            buddy_end > (char*)allocator->heap_start + allocator->heap_size) 
        {
            break;
        }
        
        // 从空闲链表中移除伙伴块
        block_t** list = &allocator->free_lists[buddy->order];
        while (*list && *list != buddy) 
        {
            list = &(*list)->next;
        }
        if (*list) 
        {
            *list = buddy->next;
        }
        
        // 选择地址较小的块作为合并后的块
        if ((char*)buddy < (char*)block) 
        {
            block = buddy;
        }
        // 更新合并后块的信息
        block->order++;
        block->size = (1 << block->order) * MIN_BLOCK_SIZE - sizeof(block_t);
        block->buddy = NULL;

        if (block->order >= MAX_ORDER) 
        {
            break;
        }

        // 计算新的伙伴块地址
        size_t block_size = 1 << block->order;
        uintptr_t block_addr = (uintptr_t)block;
        uintptr_t buddy_addr = block_addr ^ (block_size * MIN_BLOCK_SIZE);
        block->buddy = (block_t*)buddy_addr;
    }
    
    // 将合并后的块加入对应的空闲链表
    block->next = allocator->free_lists[block->order];
    allocator->free_lists[block->order] = block;
}

/**
 * memset标准实现
 * 将指定内存区域设置为指定值
 * @param s: 要设置的内存起始地址
 * @param c: 要设置的值
 * @param n: 要设置的字节数
 * @return: 返回内存区域的起始地址
 */
void* memset(void* s, int c, size_t n) 
{
    // 检查参数有效性
    if (!s) {
        return NULL;
    }

    // 将要置的值转换为无符号字符
    unsigned char value = (unsigned char)c;
    unsigned char* p = (unsigned char*)s;

    // 如果内存区域足够大，使用字对齐优化
    if (n >= sizeof(unsigned long)) 
    {
        // 对齐到字边界
        while (((uintptr_t)p & (sizeof(unsigned long) - 1)) && n) 
        {
            *p++ = value;
            n--;
        }

        // 创建一个填充字
        unsigned long fill_word = 0;
        for (size_t i = 0; i < sizeof(unsigned long); i++) 
        {
            fill_word = (fill_word << 8) | value;
        }

        // 按字填充
        unsigned long* wp = (unsigned long*)p;
        while (n >= sizeof(unsigned long)) 
        {
            *wp++ = fill_word;
            n -= sizeof(unsigned long);
        }

        // 恢复字节指针
        p = (unsigned char*)wp;
    }

    // 处理剩余的字节
    while (n--) 
    {
        *p++ = value;
    }

    return s;
}

/**
 * 确保分配器已初始化
 */
static buddy_allocator_t* ensure_allocator_init(void) 
{
    if (!global_allocator) 
    {
        global_allocator = buddy_init(INITIAL_POOL_SIZE);
    }
    return global_allocator;
}

/**
 * malloc实现 - 从内存池中分配指定大小的内存块
 * 
 * 分配流程：
 * 1. 确保分配器已初始化
 * 2. 计算实际需要的内存大小（包含块头）
 * 3. 必要时扩展内存池
 * 4. 查找合适的空闲块
 * 5. 必要时分割大块
 * 6. 返回可用内存区域
 * 
 * @param size: 请求分配的内存大小（字节数）
 * @return: 成功返回分配的内存地址，失败返回NULL
 */
void* malloc(size_t size) 
{
    // 1. 确保分配器已初始化
    buddy_allocator_t* allocator = ensure_allocator_init();
    if (!allocator) 
    {
        return NULL;  // 初始化失败
    }

    // 2. 计算实际需要的总大小（包含块头部信息）
    size_t total_size = size + sizeof(block_t);
    
    // 3. 如果请求的大小超过当前堆大小，尝试扩展内存池
    if (total_size > allocator->heap_size) 
    {
        if (!expand_memory_pool(allocator, total_size)) 
        {
            return NULL;  // 扩展失败
        }
    }

    // 4. 计算所需内存块的order并开始查找
    int order = get_order(total_size);        // 目标order
    int current_order = order;                // 当前查找的order
    block_t* block = NULL;                    // 找到的内存块

    // 5. 在空闲链表中查找合适的块
    // 从所需order开始，逐级向上查找，直到找到可用块或达到最大order
    while (current_order < MAX_ORDER) 
    {
        if (allocator->free_lists[current_order]) 
        {
            // 找到可用块，从空闲链表中移除
            block = allocator->free_lists[current_order];
            allocator->free_lists[current_order] = block->next;
            break;
        }
        current_order++;  // 尝试更大的order
    }

    // 6. 如果没找到合适的块，尝试扩展内存池并重新分配
    if (!block) 
    {
        if (!expand_memory_pool(allocator, total_size)) 
        {
            return NULL;  // 扩展失���
        }
        return malloc(size);  // 递归调用，重新尝试分配
    }

    // 7. 如果找到的块比需要的大，进行分割
    // 分割会创建伙伴块并将多余的部分加入到对应的空闲链表
    if (current_order > order) 
    {
        block = split_block(allocator, block, order);
    }

    // 8. 标记块为已使用状态
    block->is_free = 0;   // 设置为非空闲
    block->next = NULL;   // 从空闲链表中断开

    // 9. 返回可用内存区域的起始地址（跳过块头部）
    // 用户获得的是去除了block_t头部之后的实际可用内存区域
    return (void*)((char*)block + sizeof(block_t));
}

/**
 * free实现
 */
void free(void* ptr) 
{
    if (!ptr || !global_allocator) 
    {
        return;
    }
    
    block_t* block = (block_t*)((char*)ptr - sizeof(block_t));
    block->is_free = 1;
    
    merge_blocks(global_allocator, block);
}