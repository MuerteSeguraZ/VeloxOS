BITS 32

MULTIBOOT2_MAGIC    equ 0xE85250D6
MULTIBOOT2_ARCH     equ 0           ; i386
MULTIBOOT_HEADER_LEN equ (multiboot_header_end - multiboot_header_start)
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + MULTIBOOT_HEADER_LEN)

section .multiboot2
align 8
multiboot_header_start:
    dd MULTIBOOT2_MAGIC
    dd MULTIBOOT2_ARCH
    dd MULTIBOOT_HEADER_LEN
    dd MULTIBOOT_CHECKSUM

    align 8
    dw 5            ; framebuffer
    dw 0            ; flags
    dd 20           ; size
    dd 1024         ; width
    dd 768          ; height
    dd 32           ; depth

    align 8
    dw 0            ; end
    dw 0
    dd 8
multiboot_header_end:

section .bss
align 4096
pml4_table: resb 4096
pdp_table:  resb 4096
pd_table_0: resb 4096
pd_table_1: resb 4096
pd_table_2: resb 4096
pd_table_3: resb 4096

align 16
stack_bottom:
    resb 65536
stack_top:

section .data
align 8
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1<<44)|(1<<47)|(1<<41)|(1<<43)|(1<<53)
.data: equ $ - gdt64
    dq (1<<44)|(1<<47)|(1<<41)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

global multiboot_info_ptr
multiboot_info_ptr: dq 0

section .text
global _start
extern kernel_main

_start:
    mov [multiboot_info_ptr], ebx
    cli
    mov esp, stack_top
    pushfd
    pop eax
    mov ecx, eax
    xor eax, (1 << 21)
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .no_cpuid
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz .no_long_mode
    mov eax, cr4
    or eax, (1 << 4)
    mov cr4, eax
    mov eax, pdp_table
    or eax, 0x3
    mov [pml4_table], eax
    mov eax, pd_table_0
    or eax, 0x3
    mov [pdp_table + 0*8], eax
    mov eax, pd_table_1
    or eax, 0x3
    mov [pdp_table + 1*8], eax
    mov eax, pd_table_2
    or eax, 0x3
    mov [pdp_table + 2*8], eax
    mov eax, pd_table_3
    or eax, 0x3
    mov [pdp_table + 3*8], eax
    mov ecx, 0
    mov eax, 0x00000083
.map_pd0:
    mov [pd_table_0 + ecx*8], eax
    add eax, 0x200000
    inc ecx
    cmp ecx, 512
    jl .map_pd0
    mov ecx, 0
    mov eax, 0x40000083
.map_pd1:
    mov [pd_table_1 + ecx*8], eax
    add eax, 0x200000
    inc ecx
    cmp ecx, 512
    jl .map_pd1
    mov ecx, 0
    mov eax, 0x80000083
.map_pd2:
    mov [pd_table_2 + ecx*8], eax
    add eax, 0x200000
    inc ecx
    cmp ecx, 512
    jl .map_pd2
    mov ecx, 0
    mov eax, 0xc0000083
.map_pd3:
    mov [pd_table_3 + ecx*8], eax
    add eax, 0x200000
    inc ecx
    cmp ecx, 512
    jl .map_pd3
    mov eax, pml4_table
    mov cr3, eax
    mov eax, cr4
    or eax, (1 << 5) | (1 << 4)
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)
    mov cr0, eax
    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

.no_cpuid:
.no_long_mode:
    hlt
    jmp $

BITS 64
long_mode_start:
    mov ax, gdt64.data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, stack_top
    mov rdi, [multiboot_info_ptr]
    and rdi, 0xFFFFFFFF

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
