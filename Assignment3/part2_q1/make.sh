#!/bin/sh

# Compile the boot loader
clang -m64 -O2 -fshort-wchar -I ../Include -I ../Include/X64 -mcmodel=small -mno-red-zone -mno-stack-arg-probe -target x86_64-pc-mingw32 -Wall -c boot.c
lld-link /dll /nodefaultlib /safeseh:no /machine:AMD64 /entry:efi_main boot.o /out:boot.dll
../fwimage/fwimage app boot.dll boot.efi

# Compile the kernel
gcc -Wall -Wno-builtin-declaration-mismatch -D__XEN_INTERFACE_VERSION__=0x040601 -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -I/usr/include/xen -pie -fno-zero-initialized-in-bss -c kernel_entry.S
gcc -Wall -Wno-builtin-declaration-mismatch -D__XEN_INTERFACE_VERSION__=0x040601 -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -I/usr/include/xen -pie -fno-zero-initialized-in-bss -c apic.c
gcc -Wall -Wno-builtin-declaration-mismatch -D__XEN_INTERFACE_VERSION__=0x040601 -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -I/usr/include/xen -pie -fno-zero-initialized-in-bss -c kernel.c
gcc -Wall -Wno-builtin-declaration-mismatch -D__XEN_INTERFACE_VERSION__=0x040601 -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -I/usr/include/xen -pie -fno-zero-initialized-in-bss -c kernel_asm.S
gcc -Wall -Wno-builtin-declaration-mismatch -D__XEN_INTERFACE_VERSION__=0x040601 -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -I/usr/include/xen -pie -fno-zero-initialized-in-bss -c kernel_syscall.c
gcc -Wall -Wno-builtin-declaration-mismatch -D__XEN_INTERFACE_VERSION__=0x040601 -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -I/usr/include/xen -pie -fno-zero-initialized-in-bss -c printf.c
gcc -Wall -Wno-builtin-declaration-mismatch -D__XEN_INTERFACE_VERSION__=0x040601 -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -I/usr/include/xen -pie -fno-zero-initialized-in-bss -c fb.c
gcc -Wall -Wno-builtin-declaration-mismatch -D__XEN_INTERFACE_VERSION__=0x040601 -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -I/usr/include/xen -pie -fno-zero-initialized-in-bss -c ascii_font.c
gcc -Wall -Wno-builtin-declaration-mismatch -D__XEN_INTERFACE_VERSION__=0x040601 -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -I/usr/include/xen -pie -fno-zero-initialized-in-bss -c gnttab.c
ld --oformat=binary -T ./kernel.lds -nostdlib -melf_x86_64 -pie kernel_entry.o apic.o kernel.o kernel_asm.o kernel_syscall.o printf.o fb.o ascii_font.o gnttab.o -o kernel

# Comple the user application
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./userinc -pie -fno-zero-initialized-in-bss -c user_entry.S
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./userinc -pie -fno-zero-initialized-in-bss -c user.c
ld --oformat=binary -T ./user.lds -nostdlib -melf_x86_64 -pie user_entry.o user.o -o user

# Create a FAT image
rm -rf ./uefi_fat_mnt
mkdir ./uefi_fat_mnt
dd if=/dev/zero of=boot2.img bs=1M count=100
mkfs.vfat ./boot2.img
sudo mount -o loop ./boot2.img ./uefi_fat_mnt
sudo mkdir -p ./uefi_fat_mnt/EFI/BOOT
sudo cp boot.efi ./uefi_fat_mnt/EFI/BOOT/BOOTX64.EFI
sudo cp kernel ./uefi_fat_mnt/EFI/BOOT/KERNEL
sudo cp user ./uefi_fat_mnt/EFI/BOOT/USER
sudo umount ./uefi_fat_mnt
