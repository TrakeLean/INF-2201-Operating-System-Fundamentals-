#include "lock.h"
#include "scheduler.h"
#include "util.h"

#define PRINT_LINE 21

void fibonacci(void){
    uint64_t current = 1;
    uint64_t next = 2;
    uint64_t fibb = 2;
    int i = 0;
    while(1){
    if (i % 10 == 0)
    {
        fibb = current+next;
    }
    
    
    print_str(PRINT_LINE,0, "Thread  8 (fibonacci)  : ");
    print_int(PRINT_LINE,25, fibb);

    current = next;
    next = fibb;
    delay(DELAY_VAL);
    yield();
    if (fibb > 33554432){
        print_str(PRINT_LINE,25, "INTEGER OVERFLOW :D");
        exit();
    }  
    i++;
    }
}