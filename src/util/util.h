//
// Created by eitan on 9/20/25.
//

#pragma once
#include "stdint.h"

// Math
double abs(double num);
double pow(double base, int power);
double max(double num1, double num2);
double min(double num1, double num2);
double floor(double num);
double ceil(double num);
double round(double num);
int rand();

// Memory
void* memcpy(void* dest, const void* src, const size_t size);
void* memset(void* dest, uint8_t val, size_t size);