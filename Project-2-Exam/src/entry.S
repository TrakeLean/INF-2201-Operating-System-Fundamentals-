#	entry.S
#	This file contains the code for kernel_entry, as well as
#	scheduler_entry, which saves the context of the current process
#	before calling the scheduler. The file is pre-processed before it
#	is assembled, allowing (among other things) the use of macros
#	and C-style comments.

.data
	.align 4

.text
.code32
	.align 4

# Make symbols visible for ld
.globl	kernel_entry
.globl	scheduler_entry

# void kernel_entry(int fn)
#
# SYSCALL(i) in syslib.c calls this function
# fn = system call number (defined in kernel.h)
#
ke_scratch:
	.long	0
ke_arg:
	.long	0

kernel_entry:
	# Save eax
	movl	%eax, (ke_scratch)

	# Save argument
	movl	4(%esp), %eax
	movl	%eax, (ke_arg)

	# %eax = pcb_t *current_running;
	movl (current_running), %eax

	// Store user stack
	movl %esp, 8(%eax)

	// Fetch kernel stack
	movl 0(%eax), %esp

	// Push argument for system call
	pushl (ke_arg)
	call kernel_entry_helper
	popl (ke_arg)

	# %eax = pcb_t *current_running;
	movl (current_running), %eax

	// Store kernel stack
	movl %esp, 0(%eax)

	// Fetch user stack
	movl 8(%eax), %esp

	ret
	// jmp scheduler_entry
	# Now it's up to you!

loop:
	jmp		loop


#	This function gets called to enter the scheduler, saving registers before doing so.
scheduler_entry:
	# Save registers
	pushal # EAX, ECX, EDX, EBX, temp(ESP), EBP, ESI, EDI

	# Save flags
	pushfl

	# %eax = pcb_t *current_running;
	movl (current_running), %eax

	# Save FPU state
	fsave 108(%eax)

	# Save stack pointer
	movl %esp, 0(%eax)

	# Call scheduler
	call scheduler

	# %eax = pcb_t *current_running;
	movl (current_running), %eax

	# Restore FPU state
	frstor 108(%eax)

	# Restore stack pointer
	movl 0(%eax), %esp

	# Restore flags
	popfl

	# Restore registers
	popal # EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX

	ret