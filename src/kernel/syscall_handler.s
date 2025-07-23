.section .text
.global _syscall_handler
.type _syscall_handler, @function

_syscall_handler:
    pusha
    movl %esp, %eax
    pushl %eax
    call syscall_dispatch
    addl $4, %esp
    popa
    iret
