/* lock.c
 *
 * Implementation of locks.
 */

#include "common.h"
#include "lock.h"
#include "scheduler.h"

/*
* l->status = 0 | unlocked
* l->status = 1 | locked
*/


void lock_init(lock_t *l) {
    l->status = 0;
    for (int i = 0; i < NUM_TOTAL; i++)
    {
        l->blocked_queue[i] = NULL;
    }
    return;
}

void lock_acquire(lock_t *l) {
    if (l->status == 0)
    {
        l->status = 1;
    }
    else
    {
        block(l->blocked_queue);
        yield();
        
    }
    return;
}

void lock_release(lock_t *l) {
    // DO NOT RELEASE LOCK IF QUEUE HAS CONTENTS
    if (l->blocked_queue[0] == NULL){
        l->status = 0;
    }else{
        unblock(l->blocked_queue);
    }
    return;
}