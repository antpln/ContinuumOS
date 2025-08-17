.section .text
.global _syscall_handler
.type _syscall_handler, @function

# Common syscall stub mirroring isr_common_stub layout
# Builds a registers_t-compatible frame and calls syscall_dispatch(regs*)
.global syscall_common_stub
.extern syscall_dispatch

syscall_common_stub:
    cli
    pusha                    # Save general purpose registers
    movw %ds, %ax
    pushl %eax               # Save original DS

    # Switch to kernel data segments
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Pass pointer to registers_t (points at saved DS on stack)
    pushl %esp
    call syscall_dispatch
    addl $4, %esp

    # Restore original segments
    popl %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    popa
    addl $8, %esp            # Pop dummy err_code and int_no
    sti
    iret

_syscall_handler:
    cli
    pushl $0                 # Push dummy error code
    pushl $128               # Push interrupt number (0x80)
    jmp syscall_common_stub
