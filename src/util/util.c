//
// Created by eitan on 9/20/25.
//

#include "util.h"

double abs(double num) {
    if (num < 0)
        return -num;
    return num;
}

double pow(double base, int power) {
    if (power == 0)
        return 1;
    else if (power == 1)
        return base;
    else if (power == -1)
        return 1 / base;
    else if (power > 1) {
        double result = base;
        for (int i = 0; i < abs(power) - 1; i++)
            result *= base;
        return result;
    }
    else {
        double result = 1 / base;
        for (int i = 0; i < abs(power) - 1; i++)
            result /= base;
        return result;
    }
}

double max(double num1, double num2) {
    return num1 > num2 ? num1 : num2;
}

double min(double num1, double num2) {
    return num1 < num2 ? num1 : num2;
}

double clamp(double num, double min, double max) {
    return num > max ? max : num < min ? min : num;
}

double floor(double num) {
    return num >= 0 ? (int)num : ((num - (int)num) != 0 ? (int)num - 1 : num);
}

double ceil(double num) {
    double flo = floor(num);
    double frac = num - flo;
    return frac != 0 ? flo + 1 : num;
}

uint64_t ceil_div(uint64_t a, uint64_t b) {
    return (a + b - 1) / b;
}

double round(double num) {
    double flo = floor(num);
    double frac = num - flo;
    return frac >= 0.5 ? flo + 1 : flo;
}

static int rand_state = 183;
int rand() {
    long r = ((rand_state * 1103515245) + 12345);
    rand_state = r % 0xffffffff;
    return rand_state;
}



void* memcpy(void* dest, const void* src, const size_t size) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* dest, uint8_t val, size_t size) {
    unsigned char *d = dest;
    while (size--) {
        *d++ = val;
    }
    return dest;
}