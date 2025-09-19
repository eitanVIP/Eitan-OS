//
// Created by eitan on 9/19/25.
//

void kernel_main(void) {
    volatile unsigned short* VGA = (unsigned short*)0xB8000;
    const char* msg = "Hello from 64-bit kernel!";
    for (int i = 0; msg[i]; i++) {
        VGA[i] = (0x0F00 | msg[i]); // white text on black background
    }
    while (1) {
        asm volatile("hlt"); // halt CPU
    }
}
