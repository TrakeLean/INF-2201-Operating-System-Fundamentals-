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

#include "common.h"
#include "mbox.h"
#include "thread.h"
#include "util.h"

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
	lock_acquire(&Q[key].l);
	Q[key].used = 1;
	lock_release(&Q[key].l);
	return key;
}

/* Close the mailbox with handle q  */
int mbox_close(int q)
{
	lock_acquire(&Q[q].l);
	Q[q].used = 0;
	lock_release(&Q[q].l);
	return 0;
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
	// Lock if there are no messages to recieve
	while (Q[q].count == 0) {
		condition_wait(&Q[q].l, &Q[q].moreData);
	}
	// Fetch the size of the message

	unsigned char temp = (Q[q].buffer[(Q[q].tail)]);
	m->size = (int) temp;

	// // Fetch message from buffer and store in "m"
	for (int i = 0; i < m->size; i++) {
		// M body already contains some values, and we dont want to read them
		m->body[i+1] = 0;
		m->body[i] = Q[q].buffer[(Q[q].tail + i + 1) % BUFFER_SIZE];
	}

	// Update tail
	Q[q].tail = (Q[q].tail + m->size + 1) % BUFFER_SIZE;
	Q[q].count--;

	// Tell send that there is more available space
	condition_signal(&Q[q].moreSpace);
	lock_release(&Q[q].l);
	return 0;
}

/* Insert 'm' into the mailbox 'q'  */
int mbox_send(int q, msg_t *m)
{
	lock_acquire(&Q[q].l);
	// Block if there is no available space
	while (m->size >= space_available(&Q[q])) {
		condition_wait(&Q[q].l, &Q[q].moreSpace);
	}

	// Store message size
	Q[q].buffer[(Q[q].head)] = m->size;

	// Store message into mailbox buffer
	for (int i = 0; i < m->size; i++) {
		Q[q].buffer[(Q[q].head + i + 1) % BUFFER_SIZE] = m->body[i];
	}

	// Update head
	Q[q].head = (Q[q].head + m->size + 1) % BUFFER_SIZE;
	Q[q].count++;


	// Tell reciever that there is more data to be dealt with
	condition_signal(&Q[q].moreData);
	lock_release(&Q[q].l);
	return 0;
}