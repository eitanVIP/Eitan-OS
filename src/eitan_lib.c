//
// Created by eitan on 9/20/25.
//

#include "eitan_lib.h"
#include "memory.h"

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

int strlen(const char* str) {
    int count = 0;
    while (str[count] != '\0') {
        if (count > 1000)
            return -1;
        count++;
    }
    return count;
}

char* num_to_str(double num) {
    if (num == 0) {
        char* result = (char*)malloc(2);
        result[0] = '0';
        result[1] = '\0';
        return result;
    }

    int is_negative = num < 0;
    if (is_negative)
        num = -num;

    int fraction_digit_count = 0;
    double no_frac_num = abs(num);
    while ((long)no_frac_num != no_frac_num && fraction_digit_count < 6) {
        fraction_digit_count++;
        no_frac_num *= 10;
    }

    long int_num = (long)no_frac_num;

    int digit_count = 0;
    long tmp = int_num;
    do {
        digit_count++;
        tmp /= 10;
    } while (tmp > 0);

    //        digits,       '.'                          '0.'                                    '-'       '\0'
    int len = digit_count + (fraction_digit_count > 0) + (fraction_digit_count == digit_count) + is_negative + 1;
    char* result = malloc(len);

    result[len - 1] = '\0';
    if (is_negative)
        result[0] = '-';

    int i = -(is_negative) - (fraction_digit_count > 0) - (fraction_digit_count == digit_count);
    while (int_num != 0) {
        result[digit_count - i - 1] = (char)(int_num % 10 + '0');
        if (i + is_negative + (fraction_digit_count > 0) + (fraction_digit_count == digit_count) + 1 == fraction_digit_count) {
            i++;
            result[digit_count - i - 1] = '.';

            if (int_num / 10 == 0)
                result[is_negative] = '0';

        }
        int_num /= 10;
        i++;
    }

    return result;
}

char* str_concat(const char* s1, const char* s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);

    // Allocate space for both strings + null terminator
    char* result = malloc(len1 + len2 + 1);
    if (!result) return NULL; // handle malloc failure

    // Copy first string
    for (int i = 0; i < len1; i++)
        result[i] = s1[i];

    // Copy second string
    for (int i = 0; i < len2; i++)
        result[len1 + i] = s2[i];

    // Null terminate
    result[len1 + len2] = '\0';

    return result;
}

static int rand_state = 183;
int rand() {
    long r = ((rand_state * 1103515245) + 12345);
    rand_state = r % 0xffffffff;
    return rand_state;
}