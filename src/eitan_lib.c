//
// Created by eitan on 9/20/25.
//

#include "eitan_lib.h"

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

// Under Construction
//const char* num_to_str(double num) {
//    if (num == 0)
//        return "0";
//
//    int fraction_digit_count = 0;
//    double no_frac_num = abs(num);
//    while ((long long)no_frac_num != no_frac_num) {
//        fraction_digit_count++;
//        no_frac_num *= 10;
//    }
//
//    long long int_num = (long long)no_frac_num;
//    const char* result;
//    while (int_num > 0) {
//        int digit = int_num;
//        while (digit / 10 > 0) {
//            digit /= 10;
//        }
//
//    }
//}