/*
 * Implementation of the mailbox.
 * Implementation notes:
 *
 * The mailbox is protected with a lock to make sure that only
 * one process is within the queue at any time.
 *
 * It also uses condition variables to signal that more space or
 * more messages are available.
 * In other words, this code can be seen as an example of implementing a
 * producer-consumer problem with a monitor and condition variables.
 *
 * Note that this implementation only allows keys from 0 to 4
 * (key >= 0 and key < MAX_Q).
 *
 * The buffer is a circular array.
 */


#include "mbox.h"
#include "thread.h"
#include "stdio.h"
#include "stdlib.h"
#include "common.h"
#include "string.h"

/*
 * q is a pointer to the waiting list where current_running should be
 * inserted.
 */
void block(pcb_t **q, int *spinlock)
{

}

/*
 * Unblocks the first process in the waiting queue (q), (*q) points to
 * the last process.
 */
void unblock(pcb_t **q)
{

}


static int test_and_set(int volatile *addr)
{
	int volatile val = LOCKED;

	asm volatile("xchg (%%eax), %%ebx" : "=b"(val) : "0"(val), "a"(addr));
	return val;
}

void spinlock_init(int *s)
{

}

/*
 * Wait until lock is free, then acquire lock. Note that this isn't
 * strictly a spinlock, as it yields while waiting to acquire the
 * lock. This implementation is thus more "generous" towards other
 * processes, but might also generate higher latencies on acquiring
 * the lock.
 */
void spinlock_acquire(int *s)
{

}

/*
 * Releasing a spinlock is pretty easy, since it only consists of
 * writing UNLOCKED to the address. This does not have to be done
 * atomically.
 */
void spinlock_release(int *s)
{

}

/* lock functions  */

void lock_init(lock_t *l)
{

}

void lock_acquire(lock_t *l)
{

}

void lock_release(lock_t *l)
{

}

/* condition functions */
void condition_init(condition_t *c)
{

}

/*
 * unlock m and block the thread (enqued on c), when unblocked acquire
 * lock m
 */
void condition_wait(lock_t *m, condition_t *c)
{

}

/* unblock first thread enqued on c */
void condition_signal(condition_t *c)
{

}

/* unblock all threads enqued on c */
void condition_broadcast(condition_t *c)
{

}


mbox_t Q[MAX_MBOX];

/*
 * Returns the number of bytes available in the queue
 * Note: Mailboxes with count=0 messages should have head=tail, which
 * means that we return BUFFER_SIZE bytes.
 */
static int space_available(mbox_t *q)
{
	if ((q->tail == q->head) && (q->count != 0)) {
		/* Message in the queue, but no space  */
		return 0;
	}

	if (q->tail > q->head) {
		/* Head has wrapped around  */
		return q->tail - q->head;
	}
	/* Head has a higher index than tail  */
	return q->tail + BUFFER_SIZE - q->head;
}

/* Initialize mailbox system, called by kernel on startup  */
void mbox_init(void)
{
	for (int i = 0; i < MAX_MBOX; i++) {
		lock_init(&Q[i].l);
		condition_init(&Q[i].moreSpace);
		condition_init(&Q[i].moreData);

		Q[i].used = 0;
		Q[i].count = 0;
		Q[i].head = 0;
		Q[i].tail = 0;
		for (int j = 0; j < BUFFER_SIZE; j++) {
			Q[i].buffer[j] = 0;
		}
	}
}

/*
 * Open a mailbox with the key 'key'. Returns a mailbox handle which
 * must be used to identify this mailbox in the following functions
 * (parameter q).
 */
int mbox_open(int key)
{
	if (key >= MAX_MBOX) {
		return -1;
	}

	lock_acquire(&Q[key].l);
	if (Q[key].used == 0) {
		Q[key].used = 1;
		lock_release(&Q[key].l);
		return key;
	}
	lock_release(&Q[key].l);
	return -1;
}

/* Close the mailbox with handle q  */
int mbox_close(int q)
{
	if (q >= MAX_MBOX) {
		return -1;
	}

	lock_acquire(&Q[q].l);
	if (Q[q].used == 1) {
		Q[q].used = 0;
		lock_release(&Q[q].l);
		return 0;
	}
	lock_release(&Q[q].l);
	return -1;
}

/*
 * Get number of messages (count) and number of bytes available in the
 * mailbox buffer (space). Note that the buffer is also used for
 * storing the message headers, which means that a message will take
 * MSG_T_HEADER + m->size bytes in the buffer. (MSG_T_HEADER =
 * sizeof(msg_t header))
 */
int mbox_stat(int q, int *count, int *space)
{
	if (q < 0 || q >= MAX_MBOX) {
		return -1;
	}

	lock_acquire(&Q[q].l);
	*count = Q[q].count;
	*space = space_available(&Q[q]);
	lock_release(&Q[q].l);
	return 0;
}

/* Fetch a message from queue 'q' and store it in 'm'  */
int mbox_recv(int q, msg_t *m)
{
	lock_acquire(&Q[q].l);
	while (Q[q].count == 0) {
		condition_wait(&Q[q].l, &Q[q].moreData);
	}
	int i;
	for (i = 0; i < MSG_SIZE(m); i++) {
		m->body[i] = Q[q].buffer[(Q[q].head + i) % BUFFER_SIZE];
		// Clear buffer
        printf("%c \n", m->body[i]);
		Q[q].buffer[(Q[q].head + i) % BUFFER_SIZE] = 0;
	}
	Q[q].head = (Q[q].head + i) % BUFFER_SIZE;
	Q[q].count--;


	condition_signal(&Q[q].moreSpace);
	lock_release(&Q[q].l);
	return 0;
}

/* Insert 'm' into the mailbox 'q'  */
int mbox_send(int q, msg_t *m)
{
	lock_acquire(&Q[q].l);
	while (m->size > space_available(&Q[q])) {
		condition_wait(&Q[q].l, &Q[q].moreSpace);
	}
	int i;

	for (i = 0; i < MSG_SIZE(m); i++) {
		Q[q].buffer[(Q[q].tail + i) % BUFFER_SIZE] = m->body[i];
	}

	Q[q].tail = (Q[q].tail + i) % BUFFER_SIZE;
	Q[q].count++;


	condition_signal(&Q[q].moreData);
	lock_release(&Q[q].l);
	return 0;
}


int main()
{
	mbox_init();

	int q = mbox_open(0);
	if (q < 0) {
		printf("Failed to open mailbox\n");
		return 1;
	}

	msg_t m;
    char text[] = "Hello world";
	m.size = strlen(text);
    printf("size: %d\n", m.size);

    for (int i = 0; i < m.size; i++) {
        m.body[i] = text[i];
    }

	mbox_send(q, &m);

	mbox_recv(q, &m);

	for (int i = 0; i < m.size; i++) {
		printf("%c ", m.body[i]);
	}
    printf("\n");


	mbox_close(q);

	return 0;
}