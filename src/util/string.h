//
// Created by eitan on 5/29/26.
//

#ifndef STRING_H
#define STRING_H

#include "stdint.h"

int strlen(const char* str);
char* num_to_str(double num);
char* num_to_str_no_malloc(uint64_t num, char *buffer, size_t buffer_size);
char* str_concat(const char* s1, const char* s2);
char* strdup(const char* str);
char* str_concats(const char** strings, int count);
unsigned char strcmp(const char* s1, const char* s2);
int strncmp(const char *s1, const char *s2, size_t n);
char* strchr(const char *s, int c);

#endif //STRING_H
