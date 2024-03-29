#ifndef MEMORY_H
#define MEMORY_H

#include "kernel.h"
#include "tlb.h"
#include "util.h"

enum
{
	/* physical page facts */
	PAGE_SIZE = 4096,
	PAGE_N_ENTRIES = (PAGE_SIZE / sizeof(uint32_t)),
	SECTORS_PER_PAGE = (PAGE_SIZE / SECTOR_SIZE),

	PTABLE_SPAN = (PAGE_SIZE * PAGE_N_ENTRIES),

	/* page directory/table entry bits (PMSA p.235 and p.240) */
	PE_P = 1 << 0,                  /* present */
	PE_RW = 1 << 1,                 /* read/write */
	PE_US = 1 << 2,                 /* user/supervisor */
	PE_PWT = 1 << 3,                /* page write-through */
	PE_PCD = 1 << 4,                /* page cache disable */
	PE_A = 1 << 5,                  /* accessed */
	PE_D = 1 << 6,                  /* dirty */
	PE_BASE_ADDR_BITS = 12,         /* position of base address */
	PE_BASE_ADDR_MASK = 0xfffff000, /* extracts the base address */

	/* Constants to simulate a very small physical memory. */
	MEM_START = 0x100000, /* 1MB */
	PAGEABLE_PAGES = 47,
	MAX_PHYSICAL_MEMORY = (MEM_START + PAGEABLE_PAGES * PAGE_SIZE),

	/* number of kernel page tables */
	N_KERNEL_PTS = 1,

	PAGE_DIRECTORY_BITS = 22,         /* position of page dir index */
	PAGE_TABLE_BITS = 12,             /* position of page table index */
	PAGE_DIRECTORY_MASK = 0xffc00000, /* page directory mask */
	PAGE_TABLE_MASK = 0x003ff000,     /* page table mask */
	PAGE_MASK = 0x00000fff,           /* page offset mask */
	/* used to extract the 10 lsb of a page directory entry */
	MODE_MASK = 0x000003ff,

	PAGE_TABLE_SIZE = (1024 * 4096 - 1) /* size of a page table in bytes */
};

//	


/* Setup free list*/
void setup_page_list();

/* Allocate size bytes of mmeory aligned to a page boudary */
uint32_t alloc_memory(uint32_t size, uint32_t pinned);

/* Free a page */
static uint32_t reset_page();
/*
 * Allocates and sets up the page directory and any necessary page
 * tables for a given process or thread.
 */
void make_page_directory(pcb_t *p);

/*
 * This function is called from _start(). It is a initialization
 * function. It sets up the page directory for the kernel and kernel
 * threads. It also initializes the paging_lock.
 */
void make_kernel_page_directory(void);

/* Prototypes */
/* Initialize the memory system, called from kernel.c: _start() */
void init_memory(void);

/*
 * Set up a page directory and page table for the process. Fill in
 * any necessary information in the pcb.
 */
void setup_page_table(pcb_t *p);

/*
 * Page fault handler, called from interrupt.c: exception_14().
 * Should handle demand paging
 */
void page_fault_handler(void);

/* Use virtual address to get index in page directory. */
inline uint32_t get_directory_index(uint32_t vaddr) {
	return (vaddr & PAGE_DIRECTORY_MASK) >> PAGE_DIRECTORY_BITS;
}

/* Use virtual address to get index in a page table.  */
inline uint32_t get_table_index(uint32_t vaddr) {
	return (vaddr & PAGE_TABLE_MASK) >> PAGE_TABLE_BITS;
}

#endif /* !MEMORY_H */
