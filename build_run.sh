#!/bin/bash
set -e

# Paths
BUILD_DIR="build"
SRC_DIR="src"
LINKER_SCRIPT="linker.ld"
KERNEL_ELF="$BUILD_DIR/kernel.elf"

# Clean and recreate build dir
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

# Run CMake to compile all .c/.S in src/ into .o files
echo "[*] Running CMake build..."
cmake -S . -B $BUILD_DIR
cmake --build $BUILD_DIR

# Link everything from build/*.o
echo "[*] Linking kernel..."
ld -n -m elf_i386 -T $LINKER_SCRIPT -o $KERNEL_ELF $BUILD_DIR/*.o

# Create ISO folder for GRUB
mkdir -p $BUILD_DIR/iso/boot/grub
cp $KERNEL_ELF $BUILD_DIR/iso/boot/kernel.elf
cat > $BUILD_DIR/iso/boot/grub/grub.cfg <<EOF
set timeout=0
menuentry "eitanos" {
    multiboot /boot/kernel.elf
    boot
}
EOF

# Make ISO with GRUB
echo "[*] Creating ISO..."
grub-mkrescue -o $BUILD_DIR/eitanos.iso $BUILD_DIR/iso

# Launch in QEMU
echo "[*] Launching QEMU..."
qemu-system-i386 $BUILD_DIR/eitanos.iso -m 512 -serial stdio
#qemu-system-i386 $BUILD_DIR/eitanos.iso -m 512 -serial stdio -S -gdb tcp::1234
