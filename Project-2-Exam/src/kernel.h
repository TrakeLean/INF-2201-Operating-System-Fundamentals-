/* kernel.h
 *
 * Various definitions used by the kernel and related code.
 */
#ifndef KERNEL_H
#define KERNEL_H

#include "common.h"

/* Cast 0xf00 into pointer to pointer to function returning void
 * ENTRY_POINT is used in syslib.c to declare 'entry_point'
 */
#define ENTRY_POINT ((void (**)())0xf00)

// Constants
enum
{
	/* Number of threads and processes initially started by the kernel. Change
	 * this when adding to or removing elements from the start_addr array.
	 */
	NUM_THREADS = 8, // 8
	NUM_PROCS = 3,   // 3
	NUM_TOTAL = (NUM_PROCS + NUM_THREADS),

	SYSCALL_YIELD = 0,
	SYSCALL_EXIT,
	SYSCALL_COUNT,

	//  Stack constants
	STACK_MIN = 0x10000,
	STACK_MAX = 0x20000,
	STACK_OFFSET = 0x0ffc,
	STACK_SIZE = 0x1000
};

// Typedefs

/* The process control block is used for storing various information about
 *  a thread or process
 */
typedef struct pcb_t {
		uint32_t kernel_stack_pointer, kernel_base_pointer;   	 // Placement of kernel_stack
		uint32_t user_stack_pointer, user_base_pointer;       	 // Placement of user_stack
		struct pcb_t *next,   								  	 // Used when job is in the ready queue
	    *previous;						
		int pid;					   		 				  	 // Process ID
		int state; 					   		    			  	 // State of the process
		uint32_t program_counter;		        			  	 // Program counter
		int is_thread;       		    					  	 // Check if thread or not
		int yield_count;									 	 // Count the number of times a thread has yielded
		uint32_t fpu_state[108];			   			     	 // FPU state
		long double kernel_before, kernel_after, kernel_overhead,
				 user_start, user_end, user_overhead;

} pcb_t;


// Variables

// The currently running process, and also a pointer to the ready queue
extern pcb_t *current_running;

// Functions

/* Initialize the pcb's for each thread/process */
void init_pcb(pcb_t *pcb);

/* Set up the stacks for each thread/process */
void setup_stack(pcb_t *pcb);

/* Creates a thread and adds it to the ready-que*/
void thread_fork(unsigned int thread);

/* Starts the first thread/process */
void thread_start(pcb_t *pcb);

// Prototypes
void kernel_entry(int fn);
void kernel_entry_helper(int fn);

#endif