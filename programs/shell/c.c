void syscall(unsigned int num, unsigned int arg1, unsigned int arg2, unsigned int arg3) {
    asm volatile(
        "int $0x80"
        : // No output operands
        : "a"(num),
          "b"(arg1),
          "c"(arg2),
          "d"(arg3)
        : "memory"
    );
}

void main(void) {
    char* str = "NIMAN\n";
    syscall(30, (unsigned int)str, 0, 0);
}