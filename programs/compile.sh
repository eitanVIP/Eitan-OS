rm -rf programs/build/$1
mkdir -p programs/build/$1

nasm -f elf32 programs/boot.S -o programs/build/boot.o
[[ -f programs/$1/asm.S ]] && nasm -f elf32 programs/$1/asm.S -o programs/build/$1/obj.o
[[ -f programs/$1/c.c ]] && gcc -m32 -ffreestanding -nostdlib -c programs/$1/c.c -o programs/build/$1/obj.o
ld -m elf_i386 -T programs/$1/linker.ld programs/build/$1/obj.o programs/build/boot.o -o programs/build/$1/elf.elf

xxd -i programs/build/$1/elf.elf > src/compiled_programs/$1.c
printf '#include "%s.h"\n\n%s\n\nunsigned char* %s_program_get() {\n    return programs_build_%s_elf_elf;\n}' $1 "$(cat src/compiled_programs/$1.c)" $1 $1 > src/compiled_programs/$1.c
printf "#pragma once\n\nunsigned char* %s_program_get();" $1 > src/compiled_programs/$1.h