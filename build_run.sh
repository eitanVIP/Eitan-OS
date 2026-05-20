#!/bin/bash
set -e

# Paths
BUILD_DIR="build"
SRC_DIR="src"
LINKER_SCRIPT="linker.ld"
KERNEL_ELF="$BUILD_DIR/kernel.elf"

./programs/compile.sh shell
./fonts/compile.sh zap

# Clean and recreate build dir
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

# Run CMake to compile all .c/.S in src/ into .o files
echo "[*] Running CMake build..."
cmake -S . -B $BUILD_DIR
cmake --build $BUILD_DIR

# Link everything from build/*.o
echo "[*] Linking kernel..."
ld -m elf_x86_64 \
   -z max-page-size=0x1000 \
   -T $LINKER_SCRIPT \
   -o $KERNEL_ELF \
   $BUILD_DIR/kernel.o $(ls $BUILD_DIR/*.o | grep -v "kernel.o")

# Create ISO folder for limine
curl -L https://github.com/Limine-Bootloader/Limine/releases/latest/download/limine-binary.tar.gz | gunzip | tar -xf -
mv limine-binary $BUILD_DIR/limine-binary
make -C $BUILD_DIR/limine-binary

mkdir -p $BUILD_DIR/iso/EFI/boot
cp BOOTX64.EFI $BUILD_DIR/iso/EFI/boot/BOOTX64.EFI
cp BOOTIA32.EFI $BUILD_DIR/iso/EFI/boot/BOOTIA32.EFI
cp limine-uefi-cd.bin $BUILD_DIR/iso/limine-uefi-cd.bin
cp limine-bios-cd.bin $BUILD_DIR/iso/limine-bios-cd.bin
cp limine-bios.sys $BUILD_DIR/iso/limine-bios.sys
cp $KERNEL_ELF $BUILD_DIR/iso/kernel.elf

printf "timeout: 0\n\n/eitanos\nprotocol: limine\npath: boot():/kernel.elf" > $BUILD_DIR/iso/limine.conf

xorriso -as mkisofs -R -r -J -b limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        "$BUILD_DIR/iso" -o "$BUILD_DIR/eitanos.iso"
$BUILD_DIR/limine-binary/limine bios-install "$BUILD_DIR/eitanos.iso"

# Launch in QEMU
echo "[*] Launching QEMU..."
qemu-system-x86_64 -drive file=$BUILD_DIR/eitanos.iso,format=raw,index=0,media=cdrom -bios /usr/share/ovmf/OVMF.fd -drive file=disk.img,format=raw,index=1,media=disk -boot d -m 512 -serial stdio #-S -s