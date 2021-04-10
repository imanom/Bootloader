#!/bin/sh

# Compile the boot loader
clang -m64 -O2 -fshort-wchar -I ../Include -I ../Include/X64 -mcmodel=small -mno-red-zone -mno-stack-arg-probe -target x86_64-pc-mingw32 -Wall -c boot.c
lld-link /dll /nodefaultlib /safeseh:no /machine:AMD64 /entry:efi_main boot.o /out:boot.dll
../fwimage/fwimage app boot.dll boot.efi

# Compile the kernel
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -pie -fno-zero-initialized-in-bss -c kernel_entry.S
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -pie -fno-zero-initialized-in-bss -c apic.c
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -pie -fno-zero-initialized-in-bss -c kernel.c
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -pie -fno-zero-initialized-in-bss -c kernel_asm.S
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -pie -fno-zero-initialized-in-bss -c kernel_syscall.c
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -pie -fno-zero-initialized-in-bss -c printf.c
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -pie -fno-zero-initialized-in-bss -c fb.c
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./kerninc -pie -fno-zero-initialized-in-bss -c ascii_font.c
ld --oformat=binary -T ./kernel.lds -nostdlib -melf_x86_64 -pie kernel_entry.o apic.o kernel.o kernel_asm.o kernel_syscall.o printf.o fb.o ascii_font.o -o kernel

# Comple the user application
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./userinc -pie -fno-zero-initialized-in-bss -c user_entry.S
gcc -Wall -Wno-builtin-declaration-mismatch -O2 -mno-red-zone -nostdinc -fno-stack-protector -I ./userinc -pie -fno-zero-initialized-in-bss -c user.c
ld --oformat=binary -T ./user.lds -nostdlib -melf_x86_64 -pie user_entry.o user.o -o user

# Create an ISO image
rm -rf uefi_iso_image
mkdir -p uefi_iso_image/EFI/BOOT
cp boot.efi uefi_iso_image/EFI/BOOT/BOOTX64.EFI
cp kernel uefi_iso_image/EFI/BOOT/KERNEL
cp user uefi_iso_image/EFI/BOOT/USER
mkisofs -o boot.iso uefi_iso_image
