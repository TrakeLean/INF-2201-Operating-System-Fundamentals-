# bootblock.s

# .equ symbol, expression
# These directive set the value of the symbol to the expression
  .equ    BOOT_SEGMENT,      0x07c0
  .equ    DISPLAY_SEGMENT,   0xb800
  .equ    KERNEL_SEGMENT,    0x0000
  .equ    KERNEL_OFFSET,     0x1000

# You need to decide where to put the stack
  .equ    STACK_SEGMENT,     0x7E00  # https://wiki.osdev.org/Memory_Map_(x86)
  .equ    STACK_POINTER,     0x9fc00 # https://wiki.osdev.org/Stack

.text                   # Code segment
.globl    _start        # The entry point must be global
.code16                 # Real mode
.org 0x0

#
# The first instruction to execute in a program is called the entry
# point. The linker expects to find the entry point in the "symbol" _start
# (with underscore).
#

_start:
  # setup stack
  movw $STACK_SEGMENT, %ax
  movw %ax, %ss
  movw $STACK_POINTER, %sp

  # Setup datasegment for bootblock
  movw $BOOT_SEGMENT, %ax               # AX = 0x07c0
  movw %ax, %ds                         # DS = AX

  jmp beyondReservedSpace

kernelSize:
  .word 0                               # bootimage will write size of kernel, in sectors

beyondReservedSpace:
  movw $DISPLAY_SEGMENT, %bx
  movw %bx, %es
  
  # Clear screen
  movw $0x0a00, %ax                     # Fill with black background / green foreground
  movw $2000, %cx                       # Number of characters (80x25 screen in text mode = 2000)
  xorw %di, %di                         # DI = 0
  rep stosw

read_disc_sectors:
  movb $2, %ah                          # Pick which command to utilize
  movb $0, %dl                          # Chose which drive to read from
  movb $2, %cl                          # Sector number
  movb $9, %al                          # Sectors to read
  movb $0, %ch                          # Which cylinder to read
  movb $0, %dh                          # Head number
  
  # Point to where information will be stored
  movw    $KERNEL_SEGMENT, %bx          # BX = 0x0000
  movw    %bx, %es                      # ES = BX
  movw    $KERNEL_OFFSET,%bx            # BX = 0x1000

  # Setup datasegment for kernel
  movw $KERNEL_OFFSET, %ax              # AX = 0x1000
  movw %ax, %ds                         # DS = AX

  int $0x13                             # Interrupt system to read ah command
  jc error                              # If carry flag is set, jump to error


  ljmp $KERNEL_SEGMENT, $KERNEL_OFFSET  # Jump to kernel

  jmp forever                           # If we get here, something went wrong

forever:
  movb $1, %es:((20-1)*80+(60-1))*2    # Write ':)'
  jmp forever
error:
  movb $101, %es:((8-1)*80+(32-1))*2    # Write 'e'
  movb $114, %es:((8-1)*80+(33-1))*2    # Write 'r'
  movb $114, %es:((8-1)*80+(34-1))*2    # Write 'r'
  movb $111, %es:((8-1)*80+(35-1))*2    # Write 'o'
  movb $114, %es:((8-1)*80+(36-1))*2    # Write 'r'
  movb $1, %es:((8-1)*80+(37-1))*2      # Write ':('
  jmp forever
