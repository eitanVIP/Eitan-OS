void syscall(unsigned int num, unsigned int arg1, unsigned int arg2, unsigned int arg3) {
    asm volatile(
        "int $0x80"
        : // No output operands
        : "a"(num),  // Put 'num' into eax
          "b"(arg1), // Put 'arg1' into ebx
          "c"(arg2), // Put 'arg2' into ecx
          "d"(arg3)  // Put 'arg3' into edx
        : "memory"   // Tell the compiler memory might change
    );
}

void main(void) {
    char* str = "NIMAN";
    syscall(30, (unsigned int)str, 0, 0);
}