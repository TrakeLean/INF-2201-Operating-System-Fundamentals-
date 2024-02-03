/*
 * memory.c
 * Note:
 * There is no separate swap area. When a data page is swapped out,
 * it is stored in the location it was loaded from in the process'
 * image. This means it's impossible to start two processes from the
 * same image without screwing up the running. It also means the
 * disk image is read once. And that we cannot use the program disk.
 *
 * Best viewed with tabs set to 4 spaces.
 */

#include "common.h"
#include "interrupt.h"
#include "kernel.h"
#include "memory.h"
#include "scheduler.h"
#include "thread.h"
#include "tlb.h"
#include "usb/scsi.h"
#include "util.h"

// Use virtual address to get index in page directory.  
inline uint32_t get_directory_index(uint32_t vaddr);

/*
 * Use virtual address to get index in a page table.  The bits are
 * masked, so we essentially get a modulo 1024 index.  The selection
 * of which page table to index into is done with
 * get_directory_index().
 */
inline uint32_t get_table_index(uint32_t vaddr);


// spinlock to control the access to memory allocation 
static int memory_lock;
static int paging_lock;

// Statically allocate some storage for the pages 
static pcb_t free_page_list[PAGEABLE_PAGES];
static pcb_t *free_page = NULL;
static pcb_t *used_page = NULL;

static char binary[34];

/*
 * The kernel's page directory, which is shared with the kernel
 * threads. The first call to setup_page_table allocates and
 * initializes this.
 */
static uint32_t *kernel_page_directory = NULL;


int get_bin(num){
    int i;
    
    for(i = 0; i < 32; i++) {
        binary[i] = ((num >> (31 - i)) & 1) ? '1' : '0';
    }
    binary[i] = '\0'; // add null terminator at end of string
    
	for(i = 33; i>20; i--){
		binary[i] = binary[i-1];
	}
	binary[i] = '|';

    return binary;
}


/* Debug-function. 
 * Write all memory addresses and values by with 4 byte increment to output-file.
 * Output-file name is specified in bochsrc-file by line:
 * com1: enabled=1, mode=file, dev=serial.out
 * where 'dev=' is output-file. 
 * Output-file can be changed, but will by default be in 'serial.out'.
 * 
 * Arguments
 * title:		prefix for memory-dump
 * start:		memory address
 * end:			memory address
 * inclzero:	binary; skip address and values where values are zero
 */
static void rsprintf_memory(char *title, uint32_t start, uint32_t end, uint32_t inclzero){
	uint32_t numpage, paddr;
	char *header;

	rsprintf("%s\n", title);

	numpage = 0;
	header = "========================== PAGE NUMBER %02d ==========================\n";

	for(paddr = start; paddr < end; paddr += sizeof(uint32_t)) {

		// Print header if address is page-aligned. 
		if(paddr % PAGE_SIZE == 0) {
			rsprintf(header, numpage);
			numpage++;
		}
		// Avoid printing address entries with no value. 
		if(	!inclzero && *(uint32_t*)paddr == 0x0) {
			continue;
		}
		/* Print: 
		 * Entry-number from current page. 
		 * Physical main memory address. 
		 * Value at address.
		 */
		rsprintf("%04d - Memory Loc: 0x%08x ~ Mem Val: 0x%08x ~ Binary: %s\n",
					((paddr - start) / sizeof(uint32_t)) % PAGE_N_ENTRIES,
					paddr,
					*(uint32_t*)paddr,
					get_bin(*(uint32_t*)paddr) );
	}
}


/*
 * Allocate size bytes of physical memory. This function is called by
 * allocate_page() to allocate memory for a page directory or a page
 * table,and by loadproc() to allocate memory for code + data + user
 * stack.
 *
 * Note: if size < 4096 bytes, then 4096 bytes are used, beacuse the
 * memory blocks must be aligned to a page boundary.
 */
uint32_t alloc_memory(uint32_t size, uint32_t pinned) {
	uint32_t ptr;
	pcb_t *tmp;

	spinlock_acquire(&memory_lock);

	free_page->pinned = pinned;

	ptr = free_page->address;
	free_page = free_page->next;

	if(free_page->next == free_page){
		// HALT("Memory exhausted!");
		reset_page();
	}

	// Remove from free list
	tmp = free_page->previous;
	free_page->previous->previous->next = free_page->previous->next;
    free_page->previous->next->previous = free_page->previous->previous;


	// and insert into the used list 
    if (used_page != NULL) {
        tmp->next = used_page;
        tmp->previous = used_page->previous;
        used_page->previous->next = tmp;
        used_page->previous = tmp;
    } else {
        tmp->next = tmp;
        tmp->previous = tmp;
        used_page = tmp;
    }

	if ((free_page->previous->address & 0xfff) != 0) {
		// align free_page_list to page boundary 
		free_page->previous->address = (free_page->previous->address & 0xfffff000) + 0x1000;
	}
#ifdef DEBUG
	scrprintf(11, 1, "%x", free_page->previous);
	scrprintf(12, 1, "%x", MAX_PHYSICAL_MEMORY);
	scrprintf(13, 1, "%x", size);
#endif // DEBUG 

	spinlock_release(&memory_lock);
	return ptr;
}

/*
 * Maps a page as present in the page table.
 *
 * 'vaddr' is the virtual address which is mapped to the physical
 * address 'paddr'.
 *
 * If user is nonzero, the page is mapped as accessible from a user
 * application.
 */
inline void table_map_present(uint32_t *table, uint32_t vaddr, uint32_t paddr, int user) {
	int access = PE_RW | PE_P, index = get_table_index(vaddr);

	if (user)
		access |= PE_US;

	table[index] = (paddr & ~PAGE_MASK) | access;
}

/*
 * Make an entry in the page directory pointing to the given page
 * table.  vaddr is the virtual address the page table start with
 * table is the physical address of the page table
 *
 * If user is nonzero, the page is mapped as accessible from a user
 * application.
 */
inline void directory_insert_table(uint32_t *directory, uint32_t vaddr, uint32_t *table, int user) {
	int access = PE_RW | PE_P, index = get_directory_index(vaddr);
	uint32_t taddr;

	if (user)
		access |= PE_US;

	taddr = (uint32_t)table;

	directory[index] = (taddr & ~PAGE_MASK) | access;
}

/*
 * Returns a pointer to a freshly allocated page in physical
 * memory. The address is aligned on a page boundary.  The page is
 * zeroed out before the pointer is returned.
 */
 
static uint32_t allocate_page(pinned) {
	uint32_t *page = (uint32_t *)alloc_memory(PAGE_SIZE, pinned);
	int i;
	scrprintf(23, 70, "%x", (uint32_t)page);

	for (i = 0; i < 1024; page[i++] = 0)
		;
	return page;
}

static uint32_t reset_page() {
	uint32_t *ptr;
	pcb_t *tmp, *current;

	spinlock_acquire(&memory_lock);

	current = used_page;
	do {
		used_page = used_page->next;
		if (used_page == current)
		{
			HALT("All used pages are pinned!"); 
		}
	} while (used_page->pinned == 1);

	ptr = used_page->address;
	used_page = used_page->next;

	// Remove from used list
	tmp = used_page->previous;
	used_page->previous->previous->next = used_page->previous->next;
	used_page->previous->next->previous = used_page->previous->previous;

	// and insert into the free list 
	if (free_page != NULL) {
		tmp->next = free_page;
		tmp->previous = free_page->previous;
		free_page->previous->next = tmp;
		free_page->previous = tmp;
	} else {
		tmp->next = tmp;
		tmp->previous = tmp;
		free_page = tmp;
	}

	for (int i = 0; i < 1024; ptr[i++] = 0){

	}
	spinlock_release(&memory_lock);
}

void setup_page_list() {
    // Set up the initial free list
    for (int i = 0; i < PAGEABLE_PAGES - 1; i++) {
        free_page_list[i].next = &free_page_list[i + 1];
        free_page_list[i + 1].previous = &free_page_list[i];
    }
    
    // Make the last node point to the first node
    free_page_list[PAGEABLE_PAGES - 1].next = &free_page_list[0];
    free_page_list[0].previous = &free_page_list[PAGEABLE_PAGES - 1];
    
    // Set the head of the free list to the first node
    free_page = &free_page_list[0];
    
    fill_free_list();
}

void fill_free_list(){
	int counter = 0;
	for (int i = MEM_START; i < MAX_PHYSICAL_MEMORY; i+=PAGE_SIZE)
	{
		counter ++;
		free_page->address = i;
		free_page->index = counter;
		free_page->pinned = 0;
		free_page = free_page->next;
	}	
}

void print_list(pcb_t *list){
	pcb_t* current = list;
	do {
		rsprintf("Page: %i, addr: %x | prev: %x - curr: %x - next: %x | MM: %x\n", current->index, current->address, current->previous, current, current->next, MAX_PHYSICAL_MEMORY);
		current = current->next;
	} while (current != list);
	rsprintf("---------------------------------------------------------------\n");
}

int size_list(pcb_t *list){
	pcb_t* current = list;
	int counter = 0;
	do {
		current = current->next;
		counter++;
	} while (current != list);
	return counter;
}



/*PROCESS_START
 * This sets up mapping for memory that should be shared between the
 * kernel and the user process. We need this since an interrupt or
 * exception doesn't change to another page directory, and we need to
 * have the kernel mapped in to handle the interrupts. So essentially
 * the kernel needs to be mapped into the user process address space.
 *
 * The user process can't access the kernel internals though, since
 * the processor checks the privilege level set on the pages and we
 * only set USER privileges on the pages the user process should be
 * allowed to access.
 *
 * Note:
 * - we identity map the pages, so that physical address is
 *   the same as the virtual address.
 *
 * - The user processes need access video memory directly, so we set
 *   the USER bit for the video page if we make this map in a user
 *   directory.
 */
 
static void make_common_map(uint32_t *page_directory, int user) {
	uint32_t *page_table, addr;

	/* Allocate memory for the page table  */
	page_table = allocate_page(1);

	/* Identity map the first 640KB of base memory */
	for (addr = 0; addr < 640 * 1024; addr += PAGE_SIZE){
		table_map_present(page_table, addr, addr, 0);
	}

	for(addr = MEM_START; addr < MAX_PHYSICAL_MEMORY; addr += PAGE_SIZE){
        table_map_present(page_table, addr, addr, user);
    }

	/* Identity map the video memory, from 0xb8000-0xb8fff. */
	table_map_present(page_table, (uint32_t)SCREEN_ADDR, (uint32_t)SCREEN_ADDR, user);

	/*
	 * Insert in page_directory an entry for virtual address 0
	 * that points to physical address of page_table.
	 */
	directory_insert_table(page_directory, 0, page_table, user);
	
}

/*
 * init_memory()
 *
 * called once by _start() in kernel.c
 * You need to set up the virtual memory map for the kernel here.
 */
void init_memory(void) {
	spinlock_init(&memory_lock);
	lock_init(&paging_lock);
	// Setup list of free pages and map them to physical memory.
	setup_page_list();

	// Allocate page directory for kernel and setup common map
	kernel_page_directory = allocate_page(1);
	make_common_map(kernel_page_directory, 0);
}

// Sets up a page directory and page table for a new process or thread.
 
void setup_page_table(pcb_t *p) {
	uint32_t *page_directory, offset, paddr, vaddr;

	lock_acquire(&paging_lock);

	if (p->is_thread) {
		/*
		 * Threads use the kernels page directory, so just set
		 * a pointer to that one and return.
		 */
		p->page_directory = kernel_page_directory;
		lock_release(&paging_lock);
		return;
	}

	// Allocate new page directory and set up common map 
	page_directory = allocate_page(p->pinned);
	make_common_map(page_directory, 1);

	// Set process's page directory 
	p->page_directory = page_directory;

	lock_release(&paging_lock);

}

/*
 * called by exception_14 in interrupt.c (the faulting address is in
 * current_running->fault_addr)
 *
 * Interrupts are on when calling this function.
 */
void page_fault_handler(void) {
	lock_acquire(&paging_lock);

	uint32_t *page_table, *table, *page, directory_index, table_index, P_bit_table, P_bit_page, user, pinned, i;

	// Get page directory index for faulting address 
	directory_index = get_directory_index(current_running->fault_addr);
	// Get page table index for faulting address 
	table_index = get_table_index(current_running->page_directory[directory_index] & PE_BASE_ADDR_MASK);

	// Check present bits on both directory and table
	P_bit_table = current_running->page_directory[directory_index] & PE_P;
	P_bit_page = table_index & PE_P;

	// Check if user/kernel level
	user = (current_running->error_code & PE_US) >> 2;
	pinned = current_running->pinned;
	rsprintf("========================== ENTER PAGE FAULT %i ==========================\n",current_running->error_code);
	rsprintf("|| Error in binary: %s\n",get_bin(current_running->error_code));
	rsprintf("|| Fault address: %x - PID: %i\n", current_running->fault_addr, current_running->pid);
	rsprintf("|| pinned: %i\n", current_running->pinned);

	// If the table is not present
	if(P_bit_table == 0){
		// Allocate new stack table and map to page directory 
		table = allocate_page(pinned);
		directory_insert_table(current_running->page_directory, current_running->fault_addr, table, user);

		rsprintf("|| Table not present...\n");
		rsprintf("|| Setting up table for PID: %i - directory[%i]...\n", current_running->pid, directory_index);
		rsprintf("|| directory[%i][%i] = %x...\n", directory_index, table_index, 0);

	}
	// If the table is present
	else{
		// If the page is not present
		if(P_bit_page == 0)
		{
			rsprintf("|| Page not present...\n");
			// Allocate new page and map to table 
			page = allocate_page(pinned);
			// Mask out the lower 12 bits to remove offset
			int base_address = current_running->fault_addr & PE_BASE_ADDR_MASK;
			// Find sector to load, this calculates how many sectors to jump from swap loc.
			int sector_location = current_running->swap_loc + ((base_address - current_running->start_pc) / SECTOR_SIZE);

			// Fill page with process data
			scsi_read(sector_location, SECTORS_PER_PAGE, (char *)page);
		
			table_map_present((current_running->page_directory[directory_index] & PE_BASE_ADDR_MASK), current_running->fault_addr, page, user);

			rsprintf("|| Setting up page: %x for PID: %i - table[%i]...\n",page ,current_running->pid, table_index);
			rsprintf("|| Writting sector: %i into table[%i] on page %x...\n",sector_location, table_index, page);
			rsprintf("|| directory[%i][%i] = %x...\n", directory_index, table_index, page);
			
			// rsprintf_memory("", MEM_START, MAX_PHYSICAL_MEMORY, 0);
		}
		// If the page is present
		else if (P_bit_page == 1)
		{
			rsprintf("|| data found in directory[%i][%i] but is not present...\n", directory_index, table_index, page);
			rsprintf("|| MAKING IT PRESENT!...\n");
		    table_map_present((current_running->page_directory[directory_index] & PE_BASE_ADDR_MASK ), current_running->fault_addr, page, user);
		}
	}


	rsprintf("========================== EXIT PAGE FAULT %i ===========================\n\n",current_running->error_code);
	// rsprintf_memory("", MEM_START, MAX_PHYSICAL_MEMORY, 0);
	current_running->page_fault_count++;

	scrprintf(23, 0, "Pages used: [%i/%i]", size_list(used_page), PAGEABLE_PAGES);

	// if (current_running->page_fault_count > 5)
	// {
	// 	rsprintf_memory("", MEM_START, MAX_PHYSICAL_MEMORY, 0);
	// 	HALT("ASd");
	// }
	


	lock_release(&paging_lock);
}

