ENTRY(_start)

SECTIONS {
    /* 0x40080000 = RAM(0x40000000) + KERNEL64_LOAD_ADDR(0x80000)
     * QEMU -kernel 对 raw bin 按 Linux 协议加载到此地址；ELF 也按此 VMA 加载。
     * 若链接在 0x40000000 而用 -kernel kernel.bin，代码实际在 0x40000000 运行，
     * 但 VBAR 仍指向 0x40000800，缺页异常时会跳转到错误地址并挂死。 */
    . = 0x40001000;

    .text : {
        boot.o(.text)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        . = ALIGN(16);
        __bss_start = .;
        *(.bss)
        __bss_end = .;
    }

    /* 异常向量表必须 2048 字节对齐 */
    . = ALIGN(0x800);
    vector_table : {
        *(vector_table)
    }

    /* 各级页表必须 4KB 对齐，由链接脚本预留空间 */
    . = ALIGN(0x1000);
    pgtable_l0 = .;
    . += 0x1000;

    . = ALIGN(0x1000);
    pgtable_l1 = .;
    . += 0x1000;

    . = ALIGN(0x1000);
    pgtable_l2_periph = .;
    . += 0x1000;

    . = ALIGN(0x1000);
    pgtable_l2_dram = .;
    . += 0x1000;

    . = ALIGN(0x1000);
    pgtable_l3_dram = .;
    . += 0x1000;

    /* 栈顶，16字节对齐 */
    . = ALIGN(16);
    stack_top = . + 0x10000;
}
