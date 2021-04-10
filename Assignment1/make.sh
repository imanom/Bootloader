#!/bin/sh

# Compile the boot loader
clang -m64 -O2 -fshort-wchar -I ../Include -I ../Include/X64 -mcmodel=small -mno-red-zone -mno-stack-arg-probe -target x86_64-pc-mingw32 -Wall -c boot.c
lld-link /dll /nodefaultlib /safeseh:no /machine:AMD64 /entry:efi_main boot.o /out:boot.dll
../fwimage/fwimage app boot.dll boot.efi

# Compile the kernel
gcc -Wall -O2 -mno-red-zone -pie -fno-zero-initialized-in-bss -c kernel_entry.S
gcc -Wall -O2 -mno-red-zone -pie -fno-zero-initialized-in-bss -c kernel.c
ld --oformat=binary -T ./kernel.lds -nostdlib -melf_x86_64 kernel_entry.o kernel.o -o kernel

# Create an ISO image
rm -rf uefi_iso_image
mkdir -p uefi_iso_image/EFI/BOOT
cp boot.efi uefi_iso_image/EFI/BOOT/BOOTX64.EFI
cp kernel uefi_iso_image/EFI/BOOT/KERNEL
mkisofs -o boot.iso uefi_iso_image
