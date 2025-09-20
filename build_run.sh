#!/bin/bash
set -e

# Files
KERNEL_C="src/kernel.c"
START_ASM="src/start.S"
LINKER_SCRIPT="linker.ld"
KERNEL_ELF="build/kernel.elf"

# Clean and create another build folder
rm -rf build
mkdir -p build

# Assemble the startup assembly
echo "[*] Assembling start.S..."
nasm -f elf32 $START_ASM -o build/start.o

# Compile kernel C code in freestanding mode
echo "[*] Compiling kernel.c..."
gcc -m32 -fno-stack-protector -fno-builtin -ffreestanding -nostdlib -c $KERNEL_C -o build/kernel.o

# Link the kernel into a single ELF file
echo "[*] Linking kernel..."
ld -n -m elf_i386 -o $KERNEL_ELF -T $LINKER_SCRIPT build/start.o build/kernel.o

# Create ISO folder for GRUB
mkdir -p build/iso/boot/grub
cp $KERNEL_ELF build/iso/boot/kernel.elf
cat > build/iso/boot/grub/grub.cfg <<EOF
set timeout=0
menuentry "eitanos" {
    multiboot /boot/kernel.elf
    boot
}
EOF

# Make ISO with GRUB
echo "[*] Creating ISO..."
grub-mkrescue -o build/eitanos.iso build/iso

# Launch in QEMU
echo "[*] Launching QEMU..."
#qemu-system-x86_64 -cdrom build/eitanos.iso -m 512 -serial stdio -boot d
qemu-system-i386 build/eitanos.iso