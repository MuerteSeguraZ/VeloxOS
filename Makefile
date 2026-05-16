# Velox OS Makefile

DISK = velox_disk.vhd
DISK_SIZE_MB = 64

CC  = gcc
AS  = nasm
LD  = ld

CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -fno-pic \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -nostdlib -O2 -Wall -Wextra \
         -m64 -std=c11 -Ikernel

LDFLAGS = -T kernel/arch/linker.ld -nostdlib -z max-page-size=0x1000

ASFLAGS = -f elf64

SRCS_C = kernel/kernel.c                    \
         kernel/mm/alloc.c                  \
         kernel/graphics/framebuffer.c      \
         kernel/graphics/font.c             \
         kernel/graphics/text.c             \
         kernel/ui/window.c                 \
         kernel/ui/desktop.c                \
         kernel/ui/menu.c                   \
         kernel/ui/input.c                  \
         kernel/drivers/mouse/mouse.c       \
         kernel/drivers/keyboard/keyboard.c \
         kernel/drivers/rtc/rtc.c           \
         kernel/drivers/ata/ata.c           \
         kernel/drivers/bus/bus.c           \
         kernel/drivers/pci/pci.c           \
         kernel/fs/fs.c                     \
         kernel/arch/idt.c                  \
         kernel/arch/pit.c                  \
         kernel/apps/explorer.c             \
				 kernel/arch/acpi.c                 \
				 kernel/drivers/apic/lapic.c        \
				 kernel/drivers/apic/ioapic.c       \
				 kernel/sched/scheduler.c           \


SRCS_S = kernel/arch/boot.asm \
         kernel/arch/isr.asm  \
				 kernel/sched/sched_context.asm

# Map all sources to obj/ directory preserving subdir structure
OBJS_C = $(patsubst kernel/%.c, obj/%.o, $(SRCS_C))
OBJS_S = $(patsubst kernel/%.asm, obj/%.o, $(SRCS_S))
OBJS   = $(OBJS_S) $(OBJS_C)

ISO    = velox.iso
KERNEL = iso/boot/velox.elf

# GRUB modules embedded into the bootloaders
GRUB_MODULES = iso9660 normal multiboot2 all_video gfxterm gfxterm_background \
               font part_gpt part_msdos search search_label

# System GRUB module directories (installed via grub-pc-bin / grub-efi-amd64-bin)
GRUB_DIR_BIOS = /usr/lib/grub/i386-pc
GRUB_DIR_EFI  = /usr/lib/grub/x86_64-efi

# UEFI firmware for QEMU (bundled with QEMU on MSYS2)
OVMF_CODE = /mingw64/share/qemu/edk2-x86_64-code.fd
OVMF_VARS = /mingw64/share/qemu/edk2-i386-vars.fd

.PHONY: all clean cleanall run

all: $(ISO)

# Compile ASM sources
obj/%.o: kernel/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Compile C sources
obj/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(DISK):
	qemu-img create -f vpc $(DISK) $(DISK_SIZE_MB)M

$(KERNEL): $(OBJS)
	@mkdir -p iso/boot/grub
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(ISO): $(KERNEL)
	cp boot/grub.cfg iso/boot/grub/grub.cfg

	@mkdir -p iso/boot/grub/i386-pc
	grub-mkimage -O i386-pc -d $(GRUB_DIR_BIOS) \
		-o /tmp/velox_core.img \
		-p /boot/grub \
		$(GRUB_MODULES)
	cat $(GRUB_DIR_BIOS)/cdboot.img /tmp/velox_core.img \
		> iso/boot/grub/i386-pc/eltorito.img

	grub-mkimage -O x86_64-efi -d $(GRUB_DIR_EFI) \
		-o /tmp/velox_BOOTX64.EFI \
		-p /boot/grub \
		$(GRUB_MODULES)

	dd if=/dev/zero of=/tmp/velox_efi.img bs=1K count=1440 2>/dev/null
	mformat -i /tmp/velox_efi.img -f 1440 ::
	mmd    -i /tmp/velox_efi.img ::/EFI
	mmd    -i /tmp/velox_efi.img ::/EFI/BOOT
	mcopy  -i /tmp/velox_efi.img /tmp/velox_BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI

	cp /tmp/velox_efi.img iso/efi.img

	xorriso -as mkisofs \
		-graft-points \
		-o $(ISO) \
		-r \
		--grub2-mbr $(GRUB_DIR_BIOS)/boot_hybrid.img \
		--protective-msdos-label \
		-b boot/grub/i386-pc/eltorito.img \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--grub2-boot-info \
		--efi-boot efi.img \
		-efi-boot-part --efi-boot-image \
		-append_partition 2 0xef iso/efi.img \
		iso/

run: $(ISO) $(DISK)
	qemu-system-x86_64 \
	    -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
	    -drive if=pflash,format=raw,file=$(OVMF_VARS) \
	    -cdrom $(ISO) \
	    -m 256M \
	    -vga std \
	    -no-reboot \
	    -drive file=$(DISK),format=vpc,if=ide \
	    -serial stdio

clean:
	rm -rf obj/
	rm -f $(KERNEL) $(ISO)
	rm -f iso/boot/grub/i386-pc/eltorito.img
	rm -f iso/efi.img

cleanall: clean
	rm -f $(DISK)