BITS 64
global sched_context_switch
sched_context_switch:
    pop rax
    mov [rdi + 0x10], rax
    mov [rdi + 0x18], rsp
    mov [rdi + 0x20], rbp
    mov [rdi + 0x28], rax
    mov [rdi + 0x30], rbx
    mov [rdi + 0x38], rcx
    mov [rdi + 0x40], rdx
    mov rax, rsi
    mov [rdi + 0x48], rsi
    mov [rdi + 0x50], rdi
    mov rsi, rax
    mov [rdi + 0x58], r8
    mov [rdi + 0x60], r9
    mov [rdi + 0x68], r10
    mov [rdi + 0x70], r11
    mov [rdi + 0x78], r12
    mov [rdi + 0x80], r13
    mov [rdi + 0x88], r14
    mov [rdi + 0x90], r15
    pushf
    pop rax
    mov [rdi + 0x98], rax
    mov rdi, rsi
    mov rsp, [rdi + 0x18]
    mov rbp, [rdi + 0x20]
    mov rax, [rdi + 0x28]
    mov rbx, [rdi + 0x30]
    mov rcx, [rdi + 0x38]
    mov rdx, [rdi + 0x40]
    mov rsi, [rdi + 0x48]
    mov r8,  [rdi + 0x58]
    mov r9,  [rdi + 0x60]
    mov r10, [rdi + 0x68]
    mov r11, [rdi + 0x70]
    mov r12, [rdi + 0x78]
    mov r13, [rdi + 0x80]
    mov r14, [rdi + 0x88]
    mov r15, [rdi + 0x90]
    mov rax, [rdi + 0x98]
    push rax
    popf
    mov rax, [rdi + 0x10]
    mov rdi, [rdi + 0x50]
    jmp rax
global sched_restore_context
sched_restore_context:
    mov rsp, [rdi + 0x18]
    mov rbp, [rdi + 0x20]
    mov rax, [rdi + 0x28]
    mov rbx, [rdi + 0x30]
    mov rcx, [rdi + 0x38]
    mov rdx, [rdi + 0x40]
    mov rsi, [rdi + 0x48]
    mov r8,  [rdi + 0x58]
    mov r9,  [rdi + 0x60]
    mov r10, [rdi + 0x68]
    mov r11, [rdi + 0x70]
    mov r12, [rdi + 0x78]
    mov r13, [rdi + 0x80]
    mov r14, [rdi + 0x88]
    mov r15, [rdi + 0x90]
    mov rax, [rdi + 0x98]
    push rax
    popf
    mov rax, [rdi + 0x10]
    mov rdi, [rdi + 0x50]
    jmp rax
global sched_get_stack_pointer
sched_get_stack_pointer:
    mov rax, rsp
    ret