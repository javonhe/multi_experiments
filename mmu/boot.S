// =============================================================================
// boot.S — AArch64 裸机 MMU 启动与异常处理
//
// 整体流程：设置栈 → 清零 BSS → 配置 MAIR/TCR → 建页表 → 设 TTBR0/VBAR → main
// 本文件在 MMU 关闭状态下完成页表构建；enable_mmu() 在 main.c 中调用。
//
// 主要参考（ARM 官方文档，可在 developer.arm.com/documentation 检索编号）：
//   DDI 0487 / DDI 0600 — ARMv8-A 体系结构手册（寄存器、页表、异常）
//   DDI 0406 — Cortex-A Programmer's Guide for ARMv8-A（MMU 入门，Chapter 12）
//   Cortex-A57 TRM — CPU 实现细节（TLB、Cache 行为）
// =============================================================================

.section .text
.global _start

// -----------------------------------------------------------------------------
// _start — 内核入口
// QEMU -kernel 从 EL1/AArch64 启动：ELF 直跳 _start；raw bin 经 stub 跳到 0x40080000
// 此时 SCTLR_EL1.M = 0（MMU 关）。链接基址须为 0x40080000（见 link.ld）。
// -----------------------------------------------------------------------------
_start:
    // --- 1. 设置栈指针 ---
    // EL1 下使用 SP_EL1；stack_top 由 link.ld 定义（页表区之后，约 0x40017000）
    // 参考：DDI 0487 — EL1 栈指针 SP_EL0 / SP_EL1
    ldr x0, =stack_top
    mov sp, x0

    // --- 2. 清零 BSS 段 ---
    // BSS 在 ELF 中占 VMA 但不占文件体积；未初始化全局/静态变量必须为 0
    ldr x0, =__bss_start
    ldr x1, =__bss_end
bss_loop:
    cmp x0, x1
    b.ge bss_done
    str xzr, [x0], #8
    b bss_loop
bss_done:

    // --- 3. 配置 MAIR_EL1（Memory Attribute Indirection Register）---
    // 页表项 AttrIndx[2:0] 只是索引，真实内存属性在此寄存器查表
    // mair_el1 寄存器是用来设置内存属性的，比如是Normal memory还是Device memory，Cacheable还是非Cacheable等
    // 页表项通过AttrIndx[2:0]索引到mair_el1寄存器中对应的属性，来确定页表项对应的内存属性
    // 比如一个页表项的AttrIndx[2:0] = 0，那么它对应的内存属性就是mair_el1寄存器中Attr0的属性
    // 比如一个页表项的AttrIndx[2:0] = 1，那么它对应的内存属性就是mair_el1寄存器中Attr1的属性
    // mair_el1寄存器的布局是[63:0] = [Attr7:Attr0]，每个AttrN占8位，总共8个AttrN
    // 比如Attr0 = 0xFF，那么它对应的内存属性就是Normal memory，Inner/Outer WB Read/Write Allocate
    // 比如Attr1 = 0x00，那么它对应的内存属性就是Device nGnRnE（最严格设备内存序）
    // 比如Attr2 = 0x00，那么它对应的内存属性就是Device nGnRnE（最严格设备内存序）
    // 比如Attr3 = 0x00，那么它对应的内存属性就是Device nGnRnE（最严格设备内存序）
    // 比如Attr4 = 0x00，那么它对应的内存属性就是Device nGnRnE（最严格设备内存序）
    // 比如Attr5 = 0x00，那么它对应的内存属性就是Device nGnRnE（最严格设备内存序）
    // 比如Attr6 = 0x00，那么它对应的内存属性就是Device nGnRnE（最严格设备内存序）
    // 比如Attr7 = 0x00，那么它对应的内存属性就是Device nGnRnE（最严格设备内存序）
    // L3 DRAM 页用 AttrIndx=0；L2 UART 块用 AttrIndx=1
    // 参考：DDI 0487 MAIR_EL1，Table D5-10（Memory attribute fields）
    mov x0, #0xFF
    msr mair_el1, x0
    isb                         // 系统寄存器写后需 ISB，保证后续页表 walk 可见

    // --- 4. 配置 TCR_EL1（Translation Control Register）---
    // TCR_EL1 控制地址翻译：VA 有效位数、页粒度、TTBR0/1 使能、Walk Cache 属性等。
    // 本实验相关字段布局（DDI 0487 Table D13-58，其余位 RES0/RES1 或为 0）：
    //   [5:0]   T0SZ    TTBR0 侧 VA 偏移（有效 VA 位数 = 64 - T0SZ）
    //   [9:8]   IRGN0   TTBR0 Walk 时 Inner Cacheability
    //   [11:10] ORGN0   TTBR0 Walk 时 Outer Cacheability
    //   [13:12] SH0     TTBR0 Walk 时 Shareability
    //   [15:14] TG0     TTBR0 翻译粒度（00 = 4KB）
    //   [21:16] T1SZ    TTBR1 侧 VA 偏移（有效 VA 位数 = 64 - T1SZ）
    //   [23]    EPD1    1 = 禁用 TTBR1 翻译
    //   [34:32] IPS     物理地址宽度（101 = 48 位 PA）
    // 值 0x500903510 各字段含义：
    //   T0SZ [5:0]   = 16  → 64-16 = 48 位 VA（TTBR0 侧）
    //   IRGN0 [9:8]  = 01  → Inner Write-Back Read-Allocate
    //   ORGN0 [11:10]= 01  → Outer Write-Back Read-Allocate
    //   SH0 [13:12]  = 3   → Inner Shareable（编码 0b11）
    //   TG0 [15:14]  = 00  → 4KB 页粒度
    //   T1SZ [21:16] = 16  → TTBR1 侧 48 位 VA（64-16）
    //   EPD1 [23]    = 1   → 禁用 TTBR1 地址转换（仅使用 TTBR0）
    //   IPS [34:32]  = 101 → 48 位物理地址
    ldr x0, =0x500903510
    msr tcr_el1, x0
    isb

    // --- 5. 构建 4 级页表（MMU 仍关闭，直接写物理地址）---
    bl build_page_table

    // --- 6. 设置 TTBR0_EL1 ---
    // L0 页表物理基址，须 4KB 对齐；MMU 开启后 VA 翻译从此 walk
    // 参考：DDI 0487 TTBR0_EL1，BADDR 在 [47:1]
    ldr x0, =pgtable_l0
    msr ttbr0_el1, x0
    isb

    // --- 7. 设置 VBAR_EL1（异常向量表基址）---
    // 须 2048 字节对齐（见 .text.vector 段 .align 11）
    // 参考：DDI 0487 VBAR_EL1，Chapter D1.13 向量表布局
    ldr x0, =vector_table
    msr vbar_el1, x0
    isb

    // --- 8. 跳转 C 主函数（MMU 仍关闭，main 内再调用 enable_mmu）---
    b main

// =============================================================================
// build_page_table — 构建四级页表恒等映射（VA = PA）
//
// 地址空间布局：
//   VA 0x00000000~0x3FFFFFFF  L0→L1[0]→L2_periph  L2 块映射（UART 等外设）
//   VA 0x40000000~0x401FFFFF  L0→L1[1]→L2_dram→L3  L3 4KB 页映射（内核 2MB）
//
// 四级 walk 索引（4KB 粒度，48 位 VA，DDI 0487 Table D5-1）：
//   L0: VA[47:39]  每档 512GB
//   L1: VA[38:30]  每档 1GB
//   L2: VA[29:21]  每档 2MB
//   L3: VA[20:12]  每档 4KB
// =============================================================================
build_page_table:
    // AArch64 的 bl 会覆盖 x30（LR）；内部多次 bl memzero 后 x30 不再是 _start
    // 的返回地址。必须在入口保存、出口恢复，否则 ret 会跳回函数内部死循环。
    stp x29, x30, [sp, #-16]!

    // 清空 5 张页表（各 4KB = 512 项 × 8 字节），初始为 Invalid
    ldr x0, =pgtable_l0
    mov x1, #0x1000
    bl memzero
    ldr x0, =pgtable_l1
    mov x1, #0x1000
    bl memzero
    ldr x0, =pgtable_l2_periph
    mov x1, #0x1000
    bl memzero
    ldr x0, =pgtable_l2_dram
    mov x1, #0x1000
    bl memzero
    ldr x0, =pgtable_l3_dram
    mov x1, #0x1000
    bl memzero

    // -------------------------------------------------------------------------
    // AArch64 VMSAv8-64 页表项（descriptor）64 位布局（4KB 粒度，DDI 0487）
    //
    // 每项 8 字节；bits[1:0] 为描述符类型（DDI 0487 Table D5-16，4KB 粒度）：
    //   [1:0]  含义（由「当前 walk 级别 + 编码」共同决定）
    //   ─────────────────────────────────────────────────────────
    //   00     Invalid：无效项，访问 → Translation Fault（任意级别）
    //   01     Block：块描述符（仅 L1/L2 合法；L1=1GB 块，L2=2MB 块）
    //          L0 写 Block → 未定义/Translation Fault；L3 不支持 Block
    //   10     RES0：保留编码，软件不得使用
    //   11     同一编码，级别不同语义不同（手册称 Table 或 Page，非两种 bit 模式）：
    //          L0/L1/L2 → Table 描述符，[47:12] 为下一级页表地址（本实验 orr #0x3）
    //          L3       → Page  描述符，[47:12] 为 4KB 页物理地址（本实验 orr 0x703）
    //          L0 只能 Table；L3 只能 Page；在非法级别写 11 → Fault
    //
    //   等价记法（Linux 常用）：bit[0]=V 有效位；bit[1] 在 L0–L2 为 0=Block/1=Table，L3 必须为 1=Page
    //
    // ── Table 描述符（L0 / L1 / L2 → 下一级页表）── 本实验 orr #0x3
    //   ARM 手册对保留位的约定（Table 描述符中 [11:2]/[58:52]/[51:48] 等）：
    //   RES0  = Reserved, Should Be Zero：软件必须写 0，保留位，必须写0
    //   RES1  = Reserved, Should Be One ：软件必须写 1，保留位，必须写1
    //   IGNORED = 软件写 0 或 1 均可，硬件不使用该位，0 与 1 效果相同
    //   [1:0]    = 11       表描述符（valid + table）
    //   [11:2]   IGNORED    写 0 或 1 无区别（本实验 memzero 后为 0）
    //   [47:12]  下一级页表物理基址（4KB 对齐，有效位；非 RES0）
    //   [51:48]  RES0       保留位，必须写0
    //   [58:52]  IGNORED    写 0 或 1 无区别
    //   上级「层级限制位」在 [63:59]（DDI 0487：Stage 1 only；Stage 2 Table 项中同位置为 RES0，须写 0）：
    //   本实验 EL1 裸机仅 Stage 1（TTBR0_EL1），无 Guest Stage 2（VTTBR_EL2），故按下列语义使用即可。
    //   [63] NSTable   0=不强制；1=下级表/页 NS 位视为 1（Non-secure，仅 Secure 态访问时有意义）
    //   [62:61] APTable  00=不限制下级 AP；01=禁止 EL0 访问；10=禁止 EL1 写；11=禁止 EL0/EL1 一切访问
    //   [60] UXNTable  0=不限制；1=下级所有叶项 UXN 视为 1（EL0 不可执行该区间指令）
    //   [59] PXNTable  0=不限制；1=下级所有叶项 PXN 视为 1（EL1+ 不可执行该区间指令）
    //   本实验 pgtable | 0x3 时 [63:59] 均为 0 = 不限制下级。
    //   意义：Table 项本身不给出最终 PA，只指向下一级；*Table 位向下级「传染」XN/AP/NS 限制
    //
    // ── Block 描述符（L2 叶子，映射 2MB）── 本实验 UART 用 0x605 | PA
    //   DDI 0487G Figure D5-15 将 64 位划分为（L2 Block / 2MB granule，48 位 OA）：
    //   [63:50]  Upper attributes（手册称 Upper block attributes）
    //   [49:48]  RES0（48 位 OA 时；FEAT_LPA2 扩展时可为 OA[49:48]）
    //   [47:21]  输出地址（2MB 对齐；映射 VA[29:21] → 此 PA 块）
    //   [20:12]  RES0
    //   [11:2]   Lower attributes（手册称 Lower block attributes）
    //   [1:0]    = 01       类型：块描述符
    //
    //   ── Lower attributes [11:2] ──
    //   [4:2]    AttrIndx  MAIR_EL1 索引（0x605 → 001 → Attr1 Device）
    //   [5]      NS        Non-secure 位：0=Secure PA，1=Non-secure PA（仅 Secure 态有效；
    //                        Non-secure 态下被忽略，只能访问 Non-secure 内存；本实验 0）
    //   [7:6]    AP[2:1]   数据访问权限（与 PXN/UXN 管「取指」分开）
    //              00 → EL1 可读可写，EL0 不可访问（本实验 0x605/0x703）
    //              01 → EL1 可读可写，EL0 可读可写
    //              10 → EL1 只读，    EL0 不可访问
    //              11 → EL1 只读，    EL0 只读
    //   [9:8]    SH[1:0]   共享属性（仅 Normal 有效；Device 硬件忽略）
    //              00 → Non-shareable
    //              01 → 保留编码，非法
    //              10 → Outer Shareable（0x605 实际取值）
    //              11 → Inner Shareable（0x703 实际取值）
    //   [10]     AF        Access Flag，必须 1，否则 Access Flag Fault
    //   [11]     nG        非全局 TLB（本实验 0）
    //
    //   ── Upper attributes [63:50]（D5-2726 各子字段）──
    //   [63]     IGNORED    硬件不使用（Block/Page 与 Table 的 [63] NSTable 不同）
    //   [62:59]  PBHA       FEAT_HPDS2 时有效，否则 IGNORED
    //   [58:55]  SW use     软件自定义，硬件 IGNORED
    //   [54]     UXN/XN     用户态/执行-never（0x605 为 0）
    //   [53]     PXN        特权态 execute-never（0x605 为 0）
    //   [52]     Contiguous 连续描述符 hint（本实验 0）
    //   [51]     DBM        Dirty Bit Management（FEAT_HAFDBS，本实验 0）
    //   [50]     GP         Guarded Page（FEAT_BTI，本实验 0）
    //
    //   注：AP 管「读写数据」；UXN/PXN 管「取指」。0x605 非零属性在 [11:2]，[63:50]/[49:48] 均为 0。
    //   意义：2MB 窗口 VA→PA 一次完成，不再进入 L3
    //
    // ── Page 描述符（L3 叶子，映射 4KB）── 本实验 DRAM 用 0x703 | PA
    //   DDI 0487G Figure D5-15：分组同 Block，地址域为 [47:12]（无 [20:12] RES0）：
    //   [63:50]  Upper attributes（字段含义同 Block）
    //   [49:48]  RES0（48 位 OA 时）
    //   [47:12]  输出地址（4KB 对齐）
    //   [11:2]   Lower attributes（字段含义同 Block）
    //   [1:0]    = 11       类型：页描述符
    //
    //   ── Lower attributes [11:2]（0x703）──
    //   [4:2]    AttrIndx  000 → Attr0 Normal
    //   [5]      NS        0（Non-secure EL1 下忽略，同 Block 说明）
    //   [7:6]    AP        00，EL1 RW / EL0 无访问
    //   [9:8]    SH        11，Inner Shareable（Normal 内存有效）
    //   [10]     AF        必须 1
    //   [11]     nG        0
    //
    //   ── Upper attributes [63:50]（0x703 均为 0，允许内核从此区域取指）──
    //   [54] UXN  [53] PXN  [52] Contiguous  [51] DBM  [50] GP
    //
    //   意义：4KB 窗口 VA→PA 完成翻译；本实验 512 项覆盖 2MB DRAM
    //
    // Walk 索引与每级覆盖范围（48 位 VA，TCR.TG0=4KB）：
    //   级别   VA 索引位    每项覆盖    本实验用途
    //   L0     [47:39]      512GB      L0[0]→L1
    //   L1     [38:30]      1GB        L1[0]→L2_periph，L1[1]→L2_dram
    //   L2     [29:21]      2MB        L2[0x48] UART 块；L2[0]→L3
    //   L3     [20:12]      4KB        L3[0..511] DRAM 页
    // -------------------------------------------------------------------------

    // --- L0：L0[0] → L1 页表（Table 描述符）---
    ldr x0, =pgtable_l0
    ldr x1, =pgtable_l1
    orr x1, x1, #0x3
    str x1, [x0, #0]

    // --- L1 ---
    // L1[0] → 外设 L2（VA 0~1GB）；L1[1] → DRAM L2（VA 1~2GB，含 0x40000000）
    ldr x0, =pgtable_l1
    ldr x1, =pgtable_l2_periph
    orr x1, x1, #0x3
    str x1, [x0, #8*0]

    ldr x1, =pgtable_l2_dram
    orr x1, x1, #0x3
    str x1, [x0, #8*1]

    // --- L2 外设：UART 区域 2MB 块映射 ---
    // QEMU virt PL011 UART 物理/虚拟地址均为 0x09000000
    // L2 索引 = VA[29:21] = 0x09000000 >> 21 = 0x48
    // 【重要】恒等映射时块描述符物理基址也必须是 0x09000000（不能写 0x08000000）
    //
    // 块描述符 0x605（DDI 0487 Table D5-17 Block descriptor）：
    //   [1:0]  = 01   Block
    //   [4:2]  = 001  AttrIndx → MAIR Attr1（Device nGnRnE）
    //   [7:6]  = 00   AP：EL1 RW，EL0 无访问
    //   [9:8]  = 10   SH=Outer Shareable（Device 内存硬件忽略 SH，此处写 10 与 00/11 效果相同）
    //   [10]   = 1    AF（Access Flag，必须置 1，否则 Access Flag Fault）
    //   [47:21]= 物理块基址（2MB 对齐）
    ldr x0, =pgtable_l2_periph
    ldr x1, =0x09000000
    ldr x2, =0x605
    orr x1, x1, x2
    str x1, [x0, #8*0x48]

    // --- L2 DRAM：L2[0] → L3 页表 ---
    ldr x0, =pgtable_l2_dram
    ldr x1, =pgtable_l3_dram
    orr x1, x1, #0x3
    str x1, [x0, #8*0]

    // --- L3 DRAM：512 个 4KB 页，映射 0x40000000~0x401FFFFF（2MB）---
    // 页描述符 0x703（DDI 0487 Table D5-18 Page descriptor）：
    //   [1:0]  = 11   Page
    //   [4:2]  = 000  AttrIndx → MAIR Attr0（Normal memory）
    //   [7:6]  = 00   AP：EL1 RW，EL0 无访问
    //   [9:8]  = 11   SH=Inner Shareable（Normal 内存有效，多核 cache 一致）
    //   [10]   = 1    AF
    //   [47:12]= 物理页基址（4KB 对齐）
    ldr x0, =pgtable_l3_dram
    mov x1, #0x40000000
    mov x2, #512
l3_loop:
    mov x3, x1
    ldr x4, =0x703
    orr x3, x3, x4
    str x3, [x0], #8
    add x1, x1, #0x1000
    subs x2, x2, #1
    b.ne l3_loop

    ldp x29, x30, [sp], #16
    ret

// =============================================================================
// memzero — 内存清零
// 参数：x0 = 起始地址，x1 = 字节数
// 注意：会修改 x2；调用方若依赖 x2 须自行保存
// =============================================================================
memzero:
    mov x2, #0
memzero_loop:
    cmp x2, x1
    b.ge memzero_done
    str xzr, [x0, x2]
    add x2, x2, #8
    b memzero_loop
memzero_done:
    ret

// =============================================================================
// vector_table — 异常向量表
// 须 2048 字节对齐（.align 11 = 2^11）
//
// AArch64 向量表布局（DDI 0487 D1.13.3），每槽 128 字节：
//   +0x000~+0x180  EL1t（使用 SP_EL0）— 未处理
//   +0x200        EL1h 同步异常 → sync_exception_el1h（缺页、指令/数据 abort）
//   +0x280        EL1h IRQ — 未处理
//   +0x300        EL1h FIQ — 未处理
//   +0x380        EL1h SError — 未处理
//   +0x400~       EL0 相关 — 未处理
//
// 本内核在 EL1h + SP_EL1 运行，同步异常走 +0x200
// =============================================================================
.section .text.vector
.align 11
.global vector_table
vector_table:
    . = vector_table + 0x000
    b unhandled_exception       // EL1t 同步
    . = vector_table + 0x080
    b unhandled_exception       // EL1t IRQ
    . = vector_table + 0x100
    b unhandled_exception       // EL1t FIQ
    . = vector_table + 0x180
    b unhandled_exception       // EL1t SError

    . = vector_table + 0x200
    b sync_exception_el1h       // EL1h 同步（缺页、指令中止、数据 abort）
    . = vector_table + 0x280
    b unhandled_exception       // EL1h IRQ
    . = vector_table + 0x300
    b unhandled_exception       // EL1h FIQ
    . = vector_table + 0x380
    b unhandled_exception       // EL1h SError

    . = vector_table + 0x400
    b unhandled_exception
    . = vector_table + 0x480
    b unhandled_exception
    . = vector_table + 0x500
    b unhandled_exception
    . = vector_table + 0x580
    b unhandled_exception
    . = vector_table + 0x600
    b unhandled_exception
    . = vector_table + 0x680
    b unhandled_exception
    . = vector_table + 0x700
    b unhandled_exception
    . = vector_table + 0x780
    b unhandled_exception

// =============================================================================
// sync_exception_el1h — EL1h 同步异常处理入口
//
// 硬件触发异常后：PC → VBAR + 0x200，此处保存上下文、调 C 处理、修正 ELR 后 eret
//
// 异常寄存器（DDI 0487）：
//   ESR_EL1 — 异常 syndrome；EC=ESR[31:26]，如 EC=0x25 为 EL1 数据 abort
//   FAR_EL1 — Fault 虚拟地址
//   ELR_EL1 — 返回地址（触发异常的那条指令）
//
// 栈布局：31 寄存器 × 8 字节 = 248 字节，对齐到 256；偏移须连续 0,16,32,...
// =============================================================================
sync_exception_el1h:
    sub sp, sp, #256
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]

    mrs x0, esr_el1
    mrs x1, far_el1
    mrs x2, elr_el1
    bl sync_exception_handler

    // AArch64 指令定长 4 字节；ELR+4 跳过 fault 指令，避免 eret 后重复触发
    mrs x0, elr_el1
    add x0, x0, #4
    msr elr_el1, x0

    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    add sp, sp, #256

    eret                        // 从 ELR_EL1/SPSR_EL1 恢复 PC/PSTATE

// 未处理异常：死循环，便于 GDB 调试
unhandled_exception:
    b .

// =============================================================================
// enable_mmu — 开启 MMU 与 Cache
//
// 开启后所有取指/访存走 VA→PA 翻译，页表须已建好且映射覆盖当前代码与栈。
//
// 步骤与原理（DDI 0487 SCTLR_EL1、Barrier、TLBI）：
//   1. dsb sy       — 完成此前所有内存访问
//   2. tlbi vmalle1 — 无效化 EL1 全部 TLB（避免旧条目）
//   3. dsb ish/isb  — 保证 TLBI 及后续 msr 可见
//   4. SCTLR_EL1.M=1 — 开启 MMU
//   5. SCTLR_EL1.C=1 — 开启 D-Cache
//   6. SCTLR_EL1.I=1 — 开启 I-Cache
//   7. isb          — 后续取指必须走 MMU
//
// 调试提示：GDB 单步穿过 msr sctlr_el1/isb 可能报 Cannot access memory，
//           建议用 continue + 断点，不要 step 穿过 MMU 使能点。
// =============================================================================
.global enable_mmu
enable_mmu:
    dsb sy
    tlbi vmalle1
    dsb ish
    isb
    mrs x0, sctlr_el1
    orr x0, x0, #(1 << 0)       // M  — MMU enable
    orr x0, x0, #(1 << 2)       // C  — Data cache enable
    orr x0, x0, #(1 << 12)      // I  — Instruction cache enable
    msr sctlr_el1, x0
    isb
    ret

// =============================================================================
// disable_mmu — 关闭 MMU 与 Cache（实验预留；完整 teardown 应先刷 cache/无效 TLB）
// SCTLR_EL1：M=0 关 MMU，C=0 关 D-Cache，I=0 关 I-Cache
// =============================================================================
.global disable_mmu
disable_mmu:
    dsb sy
    mrs x0, sctlr_el1
    bic x0, x0, #(1 << 0)
    bic x0, x0, #(1 << 2)
    bic x0, x0, #(1 << 12)
    msr sctlr_el1, x0
    isb
    ret

// =============================================================================
// get_sctlr — 读取 SCTLR_EL1，供 main.c 验证 MMU 是否开启（检查 M 位）
// =============================================================================
.global get_sctlr
get_sctlr:
    mrs x0, sctlr_el1
    ret
