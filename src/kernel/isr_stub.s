.section .text
.global isr_common_stub
.global irq_common_stub
.extern isr_handler
.extern irq_handler


.section .data
debug_fmt:
    .asciz "Debug: interrupt number %x\n"

# Common ISR Stub
isr_common_stub:
    cli
    pusha                   # Push all registers (32 bytes)
    movw %ds, %ax
    pushl %eax             # Save ds (4 bytes)

    # Load kernel data segment
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Push pointer to registers_t structure
    pushl %esp            # Push current stack pointer as argument to handler
    
    call isr_handler      # Call IRQ handler
    
    addl $4, %esp        # Remove registers_t pointer argument

    # Restore data segments
    popl %eax            # Restore original data segment
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    popa                 # Restore all registers
    addl $8, %esp       # Clean up error code and interrupt number
    sti                 # Re-enable interrupts
    iret                     # Return from interrupt

irq_common_stub:
    cli
    pusha                   # Push all registers (32 bytes)
    movw %ds, %ax
    pushl %eax             # Save ds (4 bytes)

    # Load kernel data segment
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Push pointer to registers_t structure
    pushl %esp            # Push current stack pointer as argument to handler
    
    call irq_handler      # Call IRQ handler
    
    addl $4, %esp        # Remove registers_t pointer argument

    # Restore data segments
    popl %eax            # Restore original data segment
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    popa                 # Restore all registers
    addl $8, %esp       # Clean up error code and interrupt number
    sti                 # Re-enable interrupts
    iret                # Return from interrupt

# Exception Handlers (ISRs)
.global isr0
isr0:
    cli
    pushl $0                # Push dummy error code
    pushl $0                # Push interrupt number
    jmp isr_common_stub

.global isr1
isr1:
    cli
    pushl $0                # Push dummy error code
    pushl $1                # Push interrupt number
    jmp isr_common_stub

.global isr2
isr2:
    cli
    pushl $0                # Push dummy error code
    pushl $2                # Push interrupt number
    jmp isr_common_stub

.global isr3
isr3:
    cli
    pushl $0                # Push dummy error code
    pushl $3                # Push interrupt number
    jmp isr_common_stub

.global isr4
isr4:
    cli
    pushl $0                # Push dummy error code
    pushl $4                # Push interrupt number
    jmp isr_common_stub

.global isr5
isr5:
    cli
    pushl $0                # Push dummy error code
    pushl $5                # Push interrupt number
    jmp isr_common_stub

.global isr6
isr6:
    cli
    pushl $0                # Push dummy error code
    pushl $6                # Push interrupt number
    jmp isr_common_stub

.global isr7
isr7:
    cli
    pushl $0                # Push dummy error code
    pushl $7                # Push interrupt number
    jmp isr_common_stub

.global isr8
isr8:
    cli
    pushl $8                # Interrupt number (CPU already pushes error code)
    jmp isr_common_stub

.global isr9
isr9:
    cli
    pushl $0                # Push dummy error code
    pushl $9                # Push interrupt number
    jmp isr_common_stub

.global isr10
isr10:
    cli
    pushl $10               # Interrupt number (CPU already pushes error code)
    jmp isr_common_stub

.global isr11
isr11:
    cli
    pushl $11               # Interrupt number (CPU already pushes error code)
    jmp isr_common_stub

.global isr12
isr12:
    cli
    pushl $12               # Interrupt number (CPU already pushes error code)
    jmp isr_common_stub

.global isr13
isr13:
    cli
    pushl $13               # Interrupt number (CPU already pushes error code)
    jmp isr_common_stub

.global isr14
isr14:
    cli
    pushl $14 # the CPU already pushes an error code
    jmp isr_common_stub


.global isr15
isr15:
    cli
    pushl $0                # Push dummy error code
    pushl $15               # Push interrupt number
    jmp isr_common_stub

.global isr16
isr16:
    cli
    pushl $0                # Push dummy error code
    pushl $16               # Push interrupt number
    jmp isr_common_stub

.global isr17
isr17:
    cli
    pushl $17               # Push interrupt number
    jmp isr_common_stub

.global isr18
isr18:
    cli
    pushl $0                # Push dummy error code
    pushl $18               # Push interrupt number
    jmp isr_common_stub

.global isr19
isr19:
    cli
    pushl $0                # Push dummy error code
    pushl $19               # Push interrupt number
    jmp isr_common_stub

.global isr20
isr20:
    cli
    pushl $0                # Push dummy error code
    pushl $20               # Push interrupt number
    jmp isr_common_stub

.global isr21
isr21:
    cli
    pushl $0                # Push dummy error code
    pushl $21               # Push interrupt number
    jmp isr_common_stub

.global isr22
isr22:
    cli
    pushl $0                # Push dummy error code
    pushl $22               # Push interrupt number
    jmp isr_common_stub

.global isr23
isr23:
    cli
    pushl $0                # Push dummy error code
    pushl $23               # Push interrupt number
    jmp isr_common_stub

.global isr24
isr24:
    cli
    pushl $0                # Push dummy error code
    pushl $24               # Push interrupt number
    jmp isr_common_stub

.global isr25
isr25:
    cli
    pushl $0                # Push dummy error code
    pushl $25               # Push interrupt number
    jmp isr_common_stub

.global isr26
isr26:
    cli
    pushl $0                # Push dummy error code
    pushl $26               # Push interrupt number
    jmp isr_common_stub

.global isr27
isr27:
    cli
    pushl $0                # Push dummy error code
    pushl $27               # Push interrupt number
    jmp isr_common_stub

.global isr28
isr28:
    cli
    pushl $0                # Push dummy error code
    pushl $28               # Push interrupt number
    jmp isr_common_stub

.global isr29
isr29:
    cli
    pushl $0                # Push dummy error code
    pushl $29               # Push interrupt number
    jmp isr_common_stub

.global isr30
isr30:
    cli
    pushl $30               # Interrupt number (CPU already pushes error code)
    jmp isr_common_stub

.global isr31
isr31:
    cli
    pushl $0                # Push dummy error code
    pushl $31               # Push interrupt number
    jmp isr_common_stub

# IRQ Handlers (IRQs 0-15)
.global irq0
irq0:
    cli
    pushl $0                # Push dummy error code
    pushl $32               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq1
irq1:
    cli
    pushl $0                # Push dummy error code
    pushl $33               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq2
irq2:
    cli
    pushl $0                # Push dummy error code
    pushl $34               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq3
irq3:
    cli
    pushl $0                # Push dummy error code
    pushl $35               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq4
irq4:
    cli
    pushl $0                # Push dummy error code
    pushl $36               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq5
irq5:
    cli
    pushl $0                # Push dummy error code
    pushl $37               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq6
irq6:
    cli
    pushl $0                # Push dummy error code
    pushl $38               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq7
irq7:
    cli
    pushl $0                # Push dummy error code
    pushl $39               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq8
irq8:
    cli
    pushl $0                # Push dummy error code
    pushl $40               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq9
irq9:
    cli
    pushl $0                # Push dummy error code
    pushl $41               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq10
irq10:
    cli
    pushl $0                # Push dummy error code
    pushl $42               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq11
irq11:
    cli
    pushl $0                # Push dummy error code
    pushl $43               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq12
irq12:
    cli
    pushl $0                # Push dummy error code
    pushl $44               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq13
irq13:
    cli
    pushl $0                # Push dummy error code
    pushl $45               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq14
irq14:
    cli
    pushl $0                # Push dummy error code
    pushl $46               # IRQs start at 32 in IDT
    jmp irq_common_stub

.global irq15
irq15:
    cli
    pushl $0                # Push dummy error code
    pushl $47               # IRQs start at 32 in IDT
    jmp irq_common_stub

.section .data
.global isr_stub_table
isr_stub_table:
    .long isr0
    .long isr1
    .long isr2
    .long isr3
    .long isr4
    .long isr5
    .long isr6
    .long isr7
    .long isr8
    .long isr9
    .long isr10
    .long isr11
    .long isr12
    .long isr13
    .long isr14
    .long isr15
    .long isr16
    .long isr17
    .long isr18
    .long isr19
    .long isr20
    .long isr21
    .long isr22
    .long isr23
    .long isr24
    .long isr25
    .long isr26
    .long isr27
    .long isr28
    .long isr29
    .long isr30
    .long isr31

.global irq_stub_table
irq_stub_table:
    .long irq0
    .long irq1
    .long irq2
    .long irq3
    .long irq4
    .long irq5
    .long irq6
    .long irq7
    .long irq8
    .long irq9
    .long irq10
    .long irq11
    .long irq12
    .long irq13
    .long irq14
    .long irq15

# Trampoline used by scheduler to finalize a context switch
.global switch_to_trampoline
.extern g_next_context
switch_to_trampoline:
    cli
    # EDX = pointer to CPUContext
    mov g_next_context, %edx
    # Load and set EFLAGS first
    mov 36(%edx), %eax
    push %eax
    popf
    # Switch stacks
    mov 4(%edx), %esp
    mov 8(%edx), %ebp
    # Restore general-purpose registers (order chosen to avoid clobbering pointer)
    mov 12(%edx), %eax      # EAX
    mov 16(%edx), %ebx      # EBX
    mov 20(%edx), %ecx      # ECX
    # Push next EIP onto new stack and finally restore remaining regs
    push 0(%edx)            # EIP
    mov 24(%edx), %edx      # EDX
    mov g_next_context, %esi
    mov 28(%esi), %esi      # ESI
    mov g_next_context, %edi
    mov 32(%edi), %edi      # EDI
    sti
    ret


