/*
 * Implementation of locks and condition variables
 */

#include "common.h"
#include "interrupt.h"
#include "scheduler.h"
#include "thread.h"
#include "util.h"

void lock_init(lock_t *l)
{
	/*
	 * no need for critical section, it is callers responsibility to
	 * make sure that locks are initialized only once
	 */
	l->status = UNLOCKED;
	l->waiting = NULL;
}

/* Acquire lock without critical section (called within critical section) */
static void lock_acquire_helper(lock_t *l)
{
	if (l->status == 0)
	{
		l->status = 1;
	}
	else
	{
		block((&l->waiting));
	}
}

void lock_acquire(lock_t *l)
{
	enter_critical();
	lock_acquire_helper(l);
	leave_critical();
}

void lock_release(lock_t *l)
{
	enter_critical();
	// If there are threads waiting, unblock the first one
	if (l->waiting != NULL)
	{
		unblock(&(l->waiting));
	}
	else
	{
		l->status = 0;
	}
	leave_critical();
}

/* condition functions */

void condition_init(condition_t *c)
{
	c->queue = NULL;
}

/*
 * unlock m and block the thread (enqued on c), when unblocked acquire
 * lock m
 */
void condition_wait(lock_t *m, condition_t *c)
{
	enter_critical();
	lock_release(m);
	block(&(c->queue));
	lock_acquire_helper(m);
	leave_critical();
}

/* unblock first thread enqued on c */
void condition_signal(condition_t *c)
{
	enter_critical();
	// If there are threads waiting, unblock the first one
	if (c->queue != NULL)
	{
		unblock(&(c->queue));
	}
	leave_critical();
}

/* unblock all threads enqued on c */
void condition_broadcast(condition_t *c)
{
	enter_critical();
	// Unblock all threads
	while (c->queue != NULL)
	{
		unblock(&(c->queue));
	}
	leave_critical();
}

/* Semaphore functions. */
void semaphore_init(semaphore_t *s, int value)
{
	s->value = value;
	s->queue = NULL;
}

void semaphore_up(semaphore_t *s)
{
	enter_critical();
	s->value++;
	// If the semaphore is positive, unblock the first thread in the queue
	if (s->value <= 0)
	{
		unblock(&(s->queue));
	}
	leave_critical();
}

void semaphore_down(semaphore_t *s)
{
	enter_critical();
	s->value--;
	// If the semaphore is negative, block the current thread
	if (s->value < 0)
	{
		block(&(s->queue));
		leave_critical();
	}
	else
	{
		leave_critical();
	}
}

/*
 * Barrier functions
 */

/* n = number of threads that waits at the barrier */
void barrier_init(barrier_t *b, int n)
{
	b->queue = 0;
	b->max = n;
}

/* Wait at barrier until all n threads reach it */
void barrier_wait(barrier_t *b)
{
	enter_critical();
	b->waiting++;
	// Check if all threads have reached the barrier
	if (b->waiting < b->max)
	{
		// If not, block the current thread
		block(&(b->queue));
	}
	// If all threads have reached the barrier, unblock all threads
	else
	{
		b->waiting = 0;
		// Unblock all threads
		while (b->queue != NULL)
		{
			unblock(&(b->queue));
		}
	}
	leave_critical();
}
