#ifndef __ELF_H__
#define __ELF_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#define EI_NIDENT	 16		/* Size of e_ident[] */
#define EI_EMG_OFF	 0x000
#define EI_CLASS_OFF 0x04
#define EI_DATA_OFF	 0x05
#define EI_VERSION_OFF 0x06
#define EI_OSABI_OFF	 0x07
#define EI_ABIVERSION_OFF 0x08
#define EI_PAD_OFF	 0x09

/* ELF Header */
typedef struct 
{
	unsigned char	e_ident[EI_NIDENT]; /* ELF Identification */
	uint16_t	e_type;		/* object file type */
	uint16_t	e_machine;	/* machine */
	uint32_t	e_version;	/* object file version */
	uint64_t	e_entry;	/* virtual entry point */
	uint64_t	e_phoff;	/* program header table offset */
	uint64_t	e_shoff;	/* section header table offset */
	uint32_t	e_flags;	/* processor-specific flags */
	uint16_t	e_ehsize;	/* ELF header size */
	uint16_t	e_phentsize;	/* program header entry size */
	uint16_t	e_phnum;	/* number of program header entries */
	uint16_t	e_shentsize;	/* section header entry size */
	uint16_t	e_shnum;	/* number of section header entries */
	uint16_t	e_shstrndx;	/* section header table's "section
					   header string table" entry offset */
} elf64_hdr;

// ELF Program Header
typedef struct
{
	uint32_t 	p_type;
	uint32_t 	p_flags;
	uint64_t 	p_offset;
	uint64_t 	p_vaddr;
	uint64_t 	p_paddr;
	uint64_t 	p_filesz;
	uint64_t 	p_memsz;
	uint64_t 	p_align;
} elf64_phdr;

// ELF Section Header
typedef struct
{
	uint32_t 	sh_name;
	uint32_t 	sh_type;
	uint64_t 	sh_flags;
	uint64_t 	sh_addr;
	uint64_t 	sh_offset;
	uint64_t 	sh_size;
	uint32_t 	sh_link;
	uint32_t 	sh_info;
	uint64_t 	sh_addralign;
	uint64_t 	sh_entsize;
}elf64_shdr;

#ifdef __cplusplus
}
#endif
#endif