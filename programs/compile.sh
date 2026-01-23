rm -rf programs/build/
mkdir -p programs/build

nasm -f elf32 programs/test.S -o programs/build/test.o
ld -m elf_i386 -T programs/linker.ld programs/build/test.o -o programs/build/test.elf

#xxd -i programs/build/test.elf | xclip -selection clipboard

xxd -i programs/build/test.elf > src/compiled_programs/test.c
printf '#include "test.h"\n\n%s\n\nunsigned char* test_program_get() {\n    return programs_build_test_elf;\n}//', "$(cat src/compiled_programs/test.c)" > src/compiled_programs/test.c