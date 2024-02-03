## EXTRA CREDITS

**kill command**:

    usage: "kill" and then the process PID you want to kill to end it

    added a lot of failsafes, you're not allowed to kill anything under pid 5
    since those are threads (pid 5 is a process, but its the shell).

**kill all command**:

    usage: "kill all" to kill all active processes.

**PS command**

    usage: "ps" to show active processes

    Implementing this was harder than excpected since the shell sometimes
    became the next process in the queue, ruining the loop 
    when iterating next anZd ending it prematurely.

**Suspend command**

    usage: "suspend" and then the process PID you want to suspend

    This was a lot easier than I thought, I just had to set the process
    state to suspended.

**Resume command**

    usage: "resume" and then the process PID you want to resume

    Same logic as suspend, but instead setting the state to running. 