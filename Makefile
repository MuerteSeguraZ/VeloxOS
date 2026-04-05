# Velox OS Makefile

CC  = gcc
AS  = nasm
LD  = ld

CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -fno-pic \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -nostdlib -O2 -Wall -Wextra \
         -m64 -std=c11 -Ikernel

LDFLAGS = -T kernel/arch/linker.ld -nostdlib -z max-page-size=0x1000

ASFLAGS = -f elf64

SRCS_C = kernel/kernel.c                  \
         kernel/mm/alloc.c                \
         kernel/graphics/framebuffer.c    \
         kernel/graphics/font.c           \
         kernel/graphics/text.c           \
         kernel/ui/window.c               \
         kernel/ui/desktop.c              \
         kernel/drivers/mouse.c           \
         kernel/drivers/rtc.c             \
         kernel/arch/idt.c                \
         kernel/arch/pit.c

SRCS_S = kernel/arch/boot.asm \
         kernel/arch/isr.asm

OBJS = $(addprefix obj/,$(SRCS_S:.asm=.o) $(SRCS_C:.c=.o))

ISO    = velox.iso
KERNEL = iso/boot/velox.elf

.PHONY: all clean run

all: $(ISO)

obj/kernel/%.o: kernel/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

obj/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS)
	mkdir -p iso/boot/grub
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(ISO): $(KERNEL)
	cp boot/grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue --directory=/usr/lib/grub/i386-pc -o $(ISO) iso/

run: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -m 256M -vga std -no-reboot

clean:
	rm -rf obj/ $(KERNEL) $(ISO)