#define UART_BASE 0x09000000
#define UART_DR  (*(volatile unsigned int *)(UART_BASE + 0x00))
#define UART_FR  (*(volatile unsigned int *)(UART_BASE + 0x18))
#define UART_TXFF (1 << 5)

void uart_putc(char c) {
    while (UART_FR & UART_TXFF);
    UART_DR = c;
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

void uart_put_hex(unsigned long num) {
    char hex[] = "0123456789abcdef";
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(hex[(num >> i) & 0xF]);
    }
}

extern void enable_mmu(void);
extern void disable_mmu(void);
extern unsigned long get_sctlr(void);

// ==============================================
// 同步异常C处理函数
// ==============================================
void sync_exception_handler(unsigned long esr, unsigned long far, unsigned long elr) {
    unsigned int ec = (esr >> 26) & 0x3F;

    uart_puts("\n========================================\n");
    uart_puts("!!! Synchronous Exception Triggered !!!\n");
    uart_puts("Exception Class (EC): 0x");
    uart_put_hex(ec);
    uart_puts("\nFault Virtual Address (FAR_EL1): 0x");
    uart_put_hex(far);
    uart_puts("\nException Return Address (ELR_EL1): 0x");
    uart_put_hex(elr);
    uart_puts("\n");

    if (ec == 0x25) {
        uart_puts("Exception Type: Data Abort (Current EL1)\n");
        unsigned int fsc = esr & 0x3F;
        uart_puts("Fault Status Code (FSC): 0x");
        uart_put_hex(fsc);
        uart_puts("\n");

        if (fsc >= 0x4 && fsc <= 0x7) {
            uart_puts("Fault Nature: Translation Fault (Page Fault)\n");
            uart_puts("Fault Page Table Level: L");
            uart_putc('0' + (fsc - 0x4));
            uart_puts("\n");
        } else if (fsc == 0x9) {
            uart_puts("Fault Nature: Access Flag Fault\n");
        } else if (fsc == 0xD) {
            uart_puts("Fault Nature: Permission Fault\n");
        }
    } else if (ec == 0x21) {
        uart_puts("Exception Type: Instruction Abort (Current EL1)\n");
    }

    uart_puts("Skipping fault instruction, resuming...\n");
    uart_puts("========================================\n\n");
}

// ==============================================
// 主函数
// ==============================================
int main(void) {
    uart_puts("====== AArch64 MMU L3 Mapping Experiment ======\n");

    // 1. MMU 关闭状态测试
    unsigned long sctlr_before = get_sctlr();
    uart_puts("[1] MMU is OFF. SCTLR_EL1 = 0x");
    uart_put_hex(sctlr_before);
    uart_puts("\n");

    volatile unsigned int *test_addr = (volatile unsigned int *)0x40010000;
    *test_addr = 0x12345678;
    uart_puts("[2] Write 0x12345678 to physical address 0x40010000\n");

    // 2. 开启 MMU
    enable_mmu();
    unsigned long sctlr_after = get_sctlr();
    uart_puts("[3] MMU is ON.  SCTLR_EL1 = 0x");
    uart_put_hex(sctlr_after);
    uart_puts("\n");

    // 3. 验证虚拟地址读写（恒等映射，地址不变）
    unsigned int val = *test_addr;
    uart_puts("[4] Read from virtual address 0x40010000: 0x");
    uart_put_hex(val);
    uart_puts("\n");
    if (val == 0x12345678) {
        uart_puts("[5] L3 page table mapping verification: SUCCESS\n");
    } else {
        uart_puts("[5] L3 page table mapping verification: FAILED\n");
    }

    // 4. 故意访问非法地址，触发缺页异常
    uart_puts("\n[6] Attempting to access unmapped address 0x50000000...\n");
    *(volatile unsigned int *)0x50000000 = 0xDEADBEEF;

    // 异常处理跳过指令后回到这里
    uart_puts("[7] Program resumed after page fault.\n");
    uart_puts("\nExperiment completed. Entering idle loop.\n");

    while (1);
    return 0;
}
