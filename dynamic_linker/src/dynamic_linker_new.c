// 临时文件：新的 calculate_address_from_vaddr_ex 函数
// 移除所有 (void)0 表达式

/**
 * 通过LOAD段映射关系计算虚拟地址对应的实际内存地址
 * 
 * @param vaddr 虚拟地址
 * @param base_addr 加载基址
 * @param first_load_vaddr 第一个LOAD段的虚拟地址（地址计算基准）
 * @param phdr 程序头表
 * @param phnum 程序头表项数量
 * @return 实际内存地址，如果找不到返回0
 */
static uint64_t calculate_address_from_vaddr_ex(uint64_t vaddr, uint64_t base_addr,
                                                     uint64_t first_load_vaddr,
                                                     Elf64_Phdr *phdr, uint16_t phnum)
{
    if (!phdr) {
        return 0;
    }
    
    // 查找包含该虚拟地址的LOAD段
    for (uint16_t i = 0; i < phnum; i++) {
        Elf64_Phdr *load = &phdr[i];
        if (load->p_type == PT_LOAD) {
            uint64_t load_end = load->p_vaddr + load->p_memsz;
            
            // 检查虚拟地址是否在这个LOAD段中
            if (vaddr >= load->p_vaddr && vaddr < load_end) {
                // 通用地址计算：使用第一个LOAD段的虚拟地址作为基准
                // 实际地址 = base_addr + (vaddr - first_load_vaddr)
                uint64_t result = base_addr + (vaddr - first_load_vaddr);
                return result;
            }
        }
    }
    
    return 0;
}

