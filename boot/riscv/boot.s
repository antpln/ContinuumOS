.section .text
.globl _start
_start:
    la sp, stack_top
    call kernel_main
1:
    wfi
    j 1b

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
