#ifndef SYSLIB_H
#define SYSLIB_H

#include "common.h"

enum { IGNORE = 0 };

void yield(void);
void exit(void);
int getpid(void);
int getpriority(void);
void setpriority(int);
int cpuspeed(void);
int mbox_open(int key);
int mbox_close(int q);
int mbox_stat(int q, uint32_t *count, uint32_t *space);
int mbox_recv(int q, msg_t *m);
int mbox_send(int q, msg_t *m);
int getchar(int *c);
int readdir(char *buf);
int loadproc(int location, int size);
int killproc(int pid);
int psproc(int pid, ps_t *ps);
int susproc(int pid);
int resproc(int pid);

#endif /* !SYSLIB_H */
