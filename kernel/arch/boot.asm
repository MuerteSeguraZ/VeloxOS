; boot.asm — Velox OS
; Multiboot2 header, GDT, long mode entry, jumps to kernel_main

BITS 32

;; ─── Multiboot2 Header ───────────────────────────────────────────────────────
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

    ; Framebuffer tag — request 1024x768 32bpp
    align 8
    dw 5            ; type: framebuffer
    dw 0            ; flags
    dd 20           ; size
    dd 1024         ; width
    dd 768          ; height
    dd 32           ; depth (bpp)

    ; End tag
    align 8
    dw 0            ; type: end
    dw 0
    dd 8
multiboot_header_end:

;; ─── BSS: Page tables + stack ────────────────────────────────────────────────
; Strategy: use 2MB huge pages (PSE) to identity-map all 4GB
; PML4[0] → pdp_table
; PDP[0..3] → pd_table_0..3   (each PDP entry covers 1GB)
; Each PD has 512 entries x 2MB = 1GB, PS bit set = huge page, no PT needed
section .bss
align 4096
pml4_table: resb 4096
pdp_table:  resb 4096
pd_table_0: resb 4096   ; maps 0x00000000 - 0x3fffffff
pd_table_1: resb 4096   ; maps 0x40000000 - 0x7fffffff
pd_table_2: resb 4096   ; maps 0x80000000 - 0xbfffffff
pd_table_3: resb 4096   ; maps 0xc0000000 - 0xffffffff (framebuffer here)

align 16
stack_bottom:
    resb 65536      ; 64K stack
stack_top:

;; ─── Data: GDT ───────────────────────────────────────────────────────────────
section .data
align 8
gdt64:
    dq 0                                    ; null descriptor
.code: equ $ - gdt64
    dq (1<<44)|(1<<47)|(1<<41)|(1<<43)|(1<<53) ; 64-bit code
.data: equ $ - gdt64
    dq (1<<44)|(1<<47)|(1<<41)              ; 64-bit data
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

; Store multiboot info pointer for the kernel
global multiboot_info_ptr
multiboot_info_ptr: dq 0

;; ─── Text: 32-bit entry ──────────────────────────────────────────────────────
section .text
global _start
extern kernel_main

_start:
    ; Save multiboot info pointer (ebx) — store in memory first
    mov [multiboot_info_ptr], ebx

    ; Disable interrupts
    cli

    ; Set up stack
    mov esp, stack_top

    ; ── Check CPUID support ──
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

    ; ── Check long mode ──
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz .no_long_mode

    ; ── Set up identity-map page tables (2MB huge pages, covers all 4GB) ──

    ; Enable PSE (Page Size Extension) now so PD huge page bit works
    mov eax, cr4
    or eax, (1 << 4)    ; PSE bit
    mov cr4, eax

    ; PML4[0] → pdp_table
    mov eax, pdp_table
    or eax, 0x3
    mov [pml4_table], eax

    ; PDP[0..3] → pd_table_0..3
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

    ; Fill pd_table_0: maps 0x00000000..0x3fffffff in 2MB pages
    ; Each entry: base | 0x83 (present + writable + huge)
    mov ecx, 0
    mov eax, 0x00000083
.map_pd0:
    mov [pd_table_0 + ecx*8], eax
    add eax, 0x200000       ; +2MB
    inc ecx
    cmp ecx, 512
    jl .map_pd0

    ; Fill pd_table_1: maps 0x40000000..0x7fffffff
    mov ecx, 0
    mov eax, 0x40000083
.map_pd1:
    mov [pd_table_1 + ecx*8], eax
    add eax, 0x200000
    inc ecx
    cmp ecx, 512
    jl .map_pd1

    ; Fill pd_table_2: maps 0x80000000..0xbfffffff
    mov ecx, 0
    mov eax, 0x80000083
.map_pd2:
    mov [pd_table_2 + ecx*8], eax
    add eax, 0x200000
    inc ecx
    cmp ecx, 512
    jl .map_pd2

    ; Fill pd_table_3: maps 0xc0000000..0xffffffff (framebuffer at 0xfd000000)
    mov ecx, 0
    mov eax, 0xc0000083
.map_pd3:
    mov [pd_table_3 + ecx*8], eax
    add eax, 0x200000
    inc ecx
    cmp ecx, 512
    jl .map_pd3

    ; ── Load PML4 ──
    mov eax, pml4_table
    mov cr3, eax

    ; ── Enable PAE (PSE already set above) ──
    mov eax, cr4
    or eax, (1 << 5) | (1 << 4)    ; PAE + PSE
    mov cr4, eax

    ; ── Set LME in EFER ──
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; ── Enable paging + protected mode ──
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)
    mov cr0, eax

    ; ── Load 64-bit GDT and far jump to 64-bit code ──
    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

.no_cpuid:
.no_long_mode:
    hlt
    jmp $

;; ─── 64-bit entry ────────────────────────────────────────────────────────────
BITS 64
long_mode_start:
    ; Load data segments
    mov ax, gdt64.data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload stack pointer (64-bit)
    mov rsp, stack_top

    ; Pass multiboot info pointer to kernel_main
    mov rdi, [multiboot_info_ptr]
    and rdi, 0xFFFFFFFF     ; zero-extend from 32-bit stored value

    call kernel_main

    ; Should never return
.hang:
    cli
    hlt
    jmp .hang
