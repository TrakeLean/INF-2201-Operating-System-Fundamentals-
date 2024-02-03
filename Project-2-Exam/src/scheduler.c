/*  scheduler.c
 */
#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "util.h"
#define MHZ 500000000 /* CPU clock rate */

// Call scheduler to run the 'next' process
void yield(void) {
    current_running->yield_count ++;
    // Store time after thread/process has ran
    current_running->user_end = get_timer();

    current_running->user_overhead = (current_running->user_end - current_running->user_start);
    clear_screen(56, current_running->pid+ 8, 80, current_running->pid+9);
    print_str(7, 56, "User-T");
    print_int(current_running->pid + 8, 56, (current_running->user_overhead/MHZ)*1000000);

    // Store time before thread/process enters kernel
    current_running->next->kernel_before = get_timer();

    scheduler_entry();
    
    // Store time after thread/process has returned from kernel
    current_running->kernel_after = get_timer();
    // Store time before thread/process starts running again
    current_running->user_start = get_timer();

    current_running->kernel_overhead = current_running->kernel_after - current_running->kernel_before;
    
    clear_screen(25, 15, 35, 16);
    print_str(15, 0, "Kernel overhead (ClCy) :");
    print_int(15, 25, current_running->kernel_overhead);

    print_str(6, 42, "Time spent in microseconds");
    clear_screen(46, current_running->pid+ 8, 56, current_running->pid+9);
    print_str(7, 42, "PID");
    print_int(current_running->pid + 8, 42, current_running->pid);
    print_str(7, 46, "Kernel-T");
    print_int(current_running->pid + 8, 46, (current_running->kernel_overhead/MHZ)*1000000);
    return;
}

/* Første gang jeg kjører så lagrer jeg tom data i entry, og neste gang den blir tilkalt så henter den den tomme dataen */

/* The scheduler picks the next job to run, and removes blocked and exited
 * processes from the ready queue, before it calls dispatch to start the
 * picked process.
 */
void scheduler(void) {
    // print_info(current_running, 0, 0);
    if ((current_running->state == STATUS_EXITED) || (current_running->state == STATUS_BLOCKED)){
        remove_from_queue(current_running);
    }
    current_running = current_running->next;
    dispatch();
    return;
}

/* dispatch() does not restore gpr's it just pops down the kernel_stack,
 * and returns to whatever called scheduler (which happens to be scheduler_entry,
 * in entry.S).
 */
void dispatch(void) {
    if (current_running->state == STATUS_FIRST_TIME)
    {
        current_running->state = STATUS_READY;
        if (current_running->is_thread == 1){
            asm volatile("movl %0, %%esp" : : "r" (current_running->kernel_stack_pointer));
            asm volatile("movl %%esp, %%ebp" : :);
        }else{
            asm volatile("movl %0, %%esp" : : "r" (current_running->user_stack_pointer));
            asm volatile("movl %%esp, %%ebp" : :);
        }
            asm volatile("jmp %0" : : "r" (current_running->program_counter));
    }
}

/* Remove the current_running process from the linked list so it
 * will not be scheduled in the future
 */
void exit(void) {
    current_running->state = STATUS_EXITED;
    scheduler_entry();
}

/* 'q' is a pointer to the waiting list where current_running should be
 * inserted
 */
void block(pcb_t **q) {
    // Remove the current process from the ready queue and insert it into the blocked queue
    for (int i = 0; i < NUM_TOTAL; i++)
    {
        if (q[i] == NULL)
        {
            q[i] = current_running;
            q[i]->state = STATUS_BLOCKED;
            break;
        }
    }
    return;
}


/* Must be called within a critical section.
 * Unblocks the first process in the waiting queue (q), (*q) points to the
 * last process.
 */
void unblock(pcb_t **q) {
    // Remove the first process from the blocked queue and insert it into the ready queues next position
    q[0]->state = STATUS_READY;
    q[0]->next = current_running->next;
    q[0]->previous = current_running;
    current_running->next->previous = q[0];
    current_running->next = q[0];

    // Shift the queue
    for (int j = 0; j < NUM_TOTAL; j++)
    {
        q[j] = q[j+1];
        if (q[j] == NULL)
        {
            break;
        } 
    }
    return;
}

// Remove from queue and set current_running to the next process
void remove_from_queue(pcb_t *source){
    source->previous->next = source->next;
    source->next->previous = source->previous;
    return;
}

