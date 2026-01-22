rm -rf programs/build/
mkdir -p programs/build

nasm -f elf32 programs/test.S -o programs/build/test.o
ld -m elf_i386 -T programs/linker.ld programs/build/test.o -o programs/build/test.elf

xxd -i programs/build/test.elf