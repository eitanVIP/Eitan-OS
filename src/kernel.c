//
// Created by eitan on 9/19/25.
//

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

volatile unsigned short* VGA = (unsigned short*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;

void put_char(char c, int x, int y) {
    VGA[y * VGA_WIDTH + x] = 0x0F00 | c;
}

void print(const char* msg, int size) {
    for (int i = 0; i < size; i++) {
        if (msg[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else {
            put_char(msg[i], cursor_x, cursor_y);
            cursor_x++;
            if (cursor_x >= VGA_WIDTH) {
                cursor_x = 0;
                cursor_y++;
            }
        }
    }

    cursor_x = 0;
    cursor_y++;
}

void clear_screen() {
    for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        VGA[i] = 0;
    }
}

void kernel_main(void) {
    clear_screen();
    const char* msg = "Hello";
    for (int j = 0; j < 10; j++)
        print(msg, 5);

    while (1) {
        asm volatile("hlt");
    }
}
