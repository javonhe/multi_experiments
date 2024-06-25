//https://en.wikipedia.org/wiki/Executable_and_Linkable_Format

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elf64.h"

#define ELF_MAGIC "\x7f""ELF"

char *ei_osabi_str[] = {
    [0] = "System V",
    [1] = "HP-UX",
    [2] = "NetBSD",
    [3] = "Linux",
    [4] = "GNU Hurd",
    [6] = "Solaris",
    [7] = "AIX",
    [8] = "IRIX",
    [9] = "FreeBSD",
    [10] = "Tru64",
    [11] = "Novell Modesto",
    [12] = "OpenBSD",
    [13] = "OpenVMS",
    [14] = "NonStop Kernel",
    [15] = "AROS",
    [16] = "Fenix OS",
    [17] = "CloudABI",
    [18] = "Stratus Technologies",
};

char *ei_class_str[] = {
    [0] = "None",
    [1] = "32-bit",
    [2] = "64-bit"
};

char *ei_data_str[] = {
    [0] = "None",
    [1] = "Little Endian",
    [2] = "Big Endian"
};

char *ei_version_str[] = {
    [0] = "Invalid",
    [1] = "Current"
};

enum
{
    ET_NONE = 0,
    ET_REL = 1,
    ET_EXEC = 2,
    ET_DYN = 3,
    ET_CORE = 4,
    ET_LOOS = 0xfe00,
    ET_HIOS = 0xfeff,
    ET_LOPROC = 0xff00,
    ET_HIPROC = 0xffff
};

char *e_type_str[] = {
    [ET_NONE] = "None",
    [ET_REL] = "Relocatable file",
    [ET_EXEC] = "Executable file",
    [ET_DYN] = "Shared object file",
    [ET_CORE] = "Core file"
};

char *e_machine_str[] = {
    [0x0] = "None",
    [0x01] = "AT&T WE 32100",
    [0x02] = "SPARC",
    [0x03] = "Intel 80386",
    [0x04] = "Motorola 68000",
    [0x05] = "Motorola 88000",
    [0x07] = "Intel 80860",
    [0x08] = "MIPS",
    [0x09] = "IBM System/370",
    [0x0a] = "MIPS RS3000",
    [0x0f] = "HPPA",
    [0x13] = "Intel 80960",
    [0x14] = "PowerPC",
    [0x15] = "PowerPC64",
    [0x17] = "IBM SPU/SPC",
    [0x24] = "NEC V800",
    [0x25] = "Fujitsu FR20",
    [0x26] = "TRW RH-32",
    [0x27] = "Motorola RCE",
    [0x28] = "ARM",
    [0x29] = "Digital Alpha",
    [0x2a] = "SuperH",
    [0x2b] = "SPARC Version 9",
    [0x2c] = "Siemens Tricore",
    [0x2d] = "Argonaut RISC Core",
    [0x2e] = "Hitachi H8/300",
    [0x2f] = "Hitachi H8/300H",
    [0x30] = "Hitachi H8S",
    [0x31] = "Hitachi H8/500",
    [0x32] = "Intel IA-64 Processor",
    [0x33] = "Stanford MIPS-X",
    [0x34] = "Motorola ColdFire",
    [0x35] = "Motorola M68HC12",
    [0x36] = "Fujitsu MMA Multimedia Accelerator",
    [0x37] = "Siemens PCP",
    [0x38] = "Sony nCPU embedded RISC processor",
    [0x39] = "Denso NDR1 microprocessor",
    [0x3a] = "Motorola Star*Core processor",
    [0x3b] = "Toyota ME16 processor",
    [0x3c] = "STMicroelectronics ST100 processor",
    [0x3d] = "Advanced Logic Corp. TinyJ embedded processor family",
    [0x3e] = "AMD x86-64",
    [0x3f] = "Sony DSP Processor",
    [0x40] = "Digital Equipment Corp. PDP-10",
    [0x41] = "Digital Equipment Corp. PDP-11",
    [0x42] = "Siemens FX66 microprocessor",
    [0x43] = "STMicroelectronics ST9+ 8/16 bit microprocessor",
    [0x44] = "STMicroelectronics ST7 8-bit microprocessor",
    [0x45] = "Motorola MC68HC16 microcontroller",
    [0x46] = "Motorola MC68HC11 microcontroller",
    [0x47] = "Motorola MC68HC08 microcontroller",
    [0x48] = "Motorola MC68HC05 microcontroller",
    [0x49] = "Silicon Graphics SVx",
    [0x4a] = "STMicroelectronics ST19 8-bit",
    [0x4b] = "Digital VAX",
    [0x4c] = "Axis Communications 32-bit embedded processor",
    [0x4d] = "Infineon Technologies 32-bit embedded processor",
    [0x4e] = "Element 14 64-bit DSP processor",
    [0x4f] = "LSI Logic 16-bit DSP processor",
    [0x8c] = "TMS320C6000 DSP processor",
    [0xaf] = "MCST Elbrus e2k",
    [0xb7] = "ARM64(Armv8/AArch64)",
    [0xdc] = "Zilog Z80",
    [0xf3] = "RISC-V",
    [0xf7] = "Berkeley Packet Filter",
    [0x101] = "WDC 65C816",
    [0x102] = "LoongArch"
};

enum
{
    PT_NULL = 0,
    PT_LOAD = 1,
    PT_DYNAMIC = 2,
    PT_INTERP = 3,
    PT_NOTE = 4,
    PT_SHLIB = 5,
    PT_PHDR = 6,
    PT_TLS = 7,
    PT_LOOS = 0x60000000,
    PT_HIOS = 0x6fffffff,
    PT_LOPROC = 0x70000000,
    PT_HIPROC = 0x7fffffff,
};

#define PT_GNU_EH_FRAME 	(PT_LOOS + 0x474e550) 
#define PT_GNU_STACK    	(PT_LOOS + 0x474e551) 
#define PT_GNU_RELRO    	(PT_LOOS + 0x474e552) 
#define PT_L4_STACK     	(PT_LOOS + 0x12) 
#define PT_L4_KIP       		(PT_LOOS + 0x13) 
#define PT_L4_AUX       		(PT_LOOS + 0x14)

char *pt_type_str[] = {
    [PT_NULL] = "NULL",
    [PT_LOAD] = "Loadable segment",
    [PT_DYNAMIC] = "Dynamic linking information",
    [PT_INTERP] = "Interpreter information",
    [PT_NOTE] = "Auxiliary information",
    [PT_SHLIB] = "Reserved",
    [PT_PHDR] = "Segment containing program header table",
    [PT_TLS] = "Thread-Local Storage segment"
};

enum
{
    PF_X = 1,
    PF_W = 2,
    PF_R = 4,
};

char *p_flags_str[] = {
    [PF_X] = "Executable segment",
    [PF_W] = "Writeable segment",
    [PF_R] = "Readable segment"
};


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: %s elf_file\n", argv[0]);
        return -1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp)
    {
        printf("Open file %s failed\n", argv[1]);
        return -1;
    }

    // read elf header
    elf64_hdr elf_header;
    fread(&elf_header, sizeof(elf_header), 1, fp);

    // check magic number
    if (memcmp(ELF_MAGIC, elf_header.e_ident, 4) != 0)
    {
        printf("Not a valid ELF file\n");
        return -1;
    }

    printf("ELF header:\n");
    printf("  Magic: %02x %02x %02x %02x\n", elf_header.e_ident[0],
           elf_header.e_ident[1], elf_header.e_ident[2], elf_header.e_ident[3]);
    printf("  Class: %s\n", ei_class_str[elf_header.e_ident[EI_CLASS_OFF]]);
    printf("  Data: %s\n", ei_data_str[elf_header.e_ident[EI_DATA_OFF]]);
    printf("  Version: %d\n", elf_header.e_ident[EI_VERSION_OFF]);
    printf("  OS/ABI: %s\n", ei_osabi_str[elf_header.e_ident[EI_OSABI_OFF]]);
    printf("  ABI Version: %d\n", elf_header.e_ident[EI_ABIVERSION_OFF]);
    printf("  Type: %s\n", e_type_str[elf_header.e_type]);
    printf("  Machine: %s\n", e_machine_str[elf_header.e_machine]);
    printf("  Version: %d\n", elf_header.e_version);
    printf("  Entry point: 0x%lx\n", elf_header.e_entry);
    printf("  Program header offset: 0x%lx\n", elf_header.e_phoff);
    printf("  Section header offset: 0x%lx\n", elf_header.e_shoff);
    printf("  Flags: 0x%x\n", elf_header.e_flags);
    printf("  Header size: %d\n", elf_header.e_ehsize);
    printf("  Program header entry size: %d\n", elf_header.e_phentsize);
    printf("  Program header entry count: %d\n", elf_header.e_phnum);
    printf("  Section header entry size: %d\n", elf_header.e_shentsize);
    printf("  Section header entry count: %d\n", elf_header.e_shnum);
    printf("  Section header string index: %d\n", elf_header.e_shstrndx);
    printf("\n");

    printf("ELF Program header:\n");
    fseek(fp, elf_header.e_phoff, SEEK_SET);
    for (int i = 0; i < elf_header.e_phnum; i++)
    {
        elf64_phdr phdr;
        fread(&phdr, sizeof(phdr), 1, fp);

        if (phdr.p_type > sizeof(pt_type_str) / sizeof(char *))
        {
            if (phdr.p_type == PT_GNU_EH_FRAME)
            {
                printf("  Type: Exception frame\n");
            }
            else if (phdr.p_type == PT_GNU_STACK)
            {
                printf("  Type: GNU Stack\n");
            }
            else if (phdr.p_type == PT_GNU_RELRO)
            {
                printf("  Type: GNU Read-only after relocation\n");
            }
            else if (phdr.p_type == PT_L4_STACK)
            {
                printf("  Type: L4 Stack\n");
            }
            else if (phdr.p_type == PT_L4_KIP)
            {
                printf("  Type: L4 Kernel Image Page\n");
            }
            else if (phdr.p_type == PT_L4_AUX)
            {
                printf("  Type: L4 Auxiliary Page\n");
            }
        }
        else
        {
            printf("  Type: %s\n", pt_type_str[phdr.p_type]);
        }

        printf("  Flags: %d\n", phdr.p_flags);
        printf("  Offset: 0x%lx\n", phdr.p_offset);
        printf("  Vaddr: 0x%lx\n", phdr.p_vaddr);
        printf("  Paddr: 0x%lx\n", phdr.p_paddr); 
        printf("  Filesz: 0x%lx\n", phdr.p_filesz);
        printf("  Memsz: 0x%lx\n", phdr.p_memsz);
        printf("  Align: 0x%lx\n", phdr.p_align);
        printf("\n");
    }

    fclose(fp);

    return 0;
}