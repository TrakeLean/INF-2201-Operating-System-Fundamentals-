/*  lock.h
 */

#ifndef LOCK_H
#define LOCK_H

// Includes
#include "kernel.h"

// Constants
enum
{
	LOCKED = 1,
	UNLOCKED = 0
};

// Typedefs
typedef struct {
	int status;
	pcb_t *blocked_queue[NUM_TOTAL];
} lock_t;

//  Prototypes
void lock_init(lock_t *);
void lock_acquire(lock_t *);
void lock_release(lock_t *);

#endif
