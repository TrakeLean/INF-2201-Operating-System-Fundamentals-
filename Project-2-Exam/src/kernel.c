/*  kernel.c
 */
#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "th.h"
#include "util.h"

// Statically allocate some storage for the pcb's
pcb_t pcb[NUM_TOTAL];

// Ready queue and pointer to currently running process
pcb_t *current_running;

/* This is the entry point for the kernel.
 * - Initialize pcb entries
 * - set up stacks
 * - start first thread/process
 */
void _start(void)
{
	/* Declare entry_point as pointer to pointer to function returning void
	 * ENTRY_POINT is defined in kernel h as (void(**)()0xf00)
	 */
	void (**entry_point)() = ENTRY_POINT;

	// load address of kernel_entry into memory location 0xf00
	*entry_point = kernel_entry;
	clear_screen(0, 0, 80, 25);
	//print_str(20,0, "Hello world, this is your kernel speaking.");

	// Initialize the pcb's and set up the stacks for each thread/process
	init_pcb(pcb);
	setup_stack(pcb);

	// Setup threads
	uint32_t list_of_threads[] = {
								  (uint32_t)0x5000,
								  (uint32_t)0x7000,
								  (uint32_t)0x9000,
								  (uint32_t)&clock_thread,
								  (uint32_t)&thread2,
								  (uint32_t)&thread3,
								  (uint32_t)&mcpi_thread0,
								  (uint32_t)&mcpi_thread1,
								  (uint32_t)&mcpi_thread2,
								  (uint32_t)&mcpi_thread3,
								  (uint32_t)&fibonacci,
								  };
	// Create the threads/processs and add them to the ready queue
	fork_multiple(list_of_threads);

	// Start the first thread/process
	thread_start(&pcb[0]);

	while (1){
	};	
}

/* Helper function for kernel_entry, in entry.S. Does the actual work
 * of executing the specified syscall.
 */
void kernel_entry_helper(int fn)
{
	switch (fn) {
	case SYSCALL_YIELD:
		// print_int(3, 0, fn);
		yield();
		break;
	case SYSCALL_EXIT:
		// print_int(3, 0, fn);
		exit();
		break;
	case SYSCALL_COUNT:
		print_int(0, 0, SYSCALL_COUNT);
		break;
	default:
		print_str(0, 0, "ERROR: Unknown syscall");
		print_int(3, 0, fn);
		break;
	}
}

void init_pcb(pcb_t *pcb) {
	// Initialize the pcb's
	for (int i = 0; i < NUM_TOTAL; i++) {
		pcb[i].pid = i;
		pcb[i].state = STATUS_FIRST_TIME;
		// Set up the linked list
		pcb[i].next = &pcb[(NUM_TOTAL+i+1)%NUM_TOTAL];
		pcb[i].previous = &pcb[(NUM_TOTAL+i-1)%NUM_TOTAL];

	}
}

void setup_stack(pcb_t *pcb){
	// Set up the stacks for each thread/process
	int stack_top;
	for (int i = 0; i < NUM_TOTAL; i++) {
		stack_top = STACK_MAX - (i * STACK_SIZE);
		pcb[i].kernel_stack_pointer = stack_top - STACK_OFFSET;
		pcb[i].kernel_base_pointer = stack_top;
		for (int j = 0; j < STACK_SIZE; j ++) {
			*((unsigned int *)(pcb[i].kernel_stack_pointer + j)) = 0;
		}
	}
}

void fork_multiple(uint32_t *list_of_threads){
	// Find the next available pcb
	int stack_top;
	for (int i = 0; i < NUM_TOTAL; i++){
			// Set the program counter to the adress of the thread
			pcb[i].program_counter = list_of_threads[i];
			// Set is_thread to 1
			pcb[i].is_thread = 1;
			// Set yield to 0
			pcb[i].yield_count = 0;
			if (pcb[i].program_counter >= (uint32_t)0x5000){
				pcb[i].is_thread = 0;
				// Setup user stacks for the processes
				stack_top = STACK_MIN + (i%NUM_PROCS * STACK_SIZE);
				pcb[i%NUM_PROCS].user_stack_pointer = stack_top;
				pcb[i%NUM_PROCS].user_base_pointer = stack_top + STACK_OFFSET;
				for (int j = 0; j < STACK_SIZE; j ++) {
					*((unsigned int *)(pcb[i].user_stack_pointer - j)) = 0;
				}
			}
	}
}

void thread_start(pcb_t *pcb){
	// Set the current running process to the first process
	current_running = pcb;
	// Start the first thread/process
	dispatch();
}