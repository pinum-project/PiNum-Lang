/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

#ifndef QUIL_RTSTRCHR_H
#define QUIL_RTSTRCHR_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// 'c' repeated `count` times → a heap string ("   " for (' ', 3))
static inline char *__quil_repeat_char(char c, int count) {
        if (!c || count <= 0) {
                char *empty = (char *)malloc(1);
                if (empty) {
                        empty[0] = '\0';
                }
                return empty;
        }
        char *out = (char *)malloc((size_t)count + 1);
        if (!out) {
                return NULL;
        }
        for (int i = 0; i < count; i++) {
                out[i] = c;
        }
        out[count] = '\0';
        return out;
}

// `str` repeated `count` times → a heap string
static inline char *__quil_repeat_string(const char *str, int count) {
        if (!str || count <= 0) {
                char *empty = (char *)malloc(1);
                if (empty) {
                        empty[0] = '\0';
                }
                return empty;
        }
        size_t len = strlen(str);
        char *out = (char *)malloc((len * (size_t)count) + 1);
        if (!out) {
                return NULL;
        }
        char *ptr = out;
        for (int i = 0; i < count; i++) {
                memcpy(ptr, str, len);
                ptr += len;
        }
        *ptr = '\0';
        return out;
}

// concatenates two strings → a heap string
static inline char *__quil_add_string(const char *s1, const char *s2) {
        if (!s1) s1 = "";
        if (!s2) s2 = "";

        size_t len1 = strlen(s1);
        size_t len2 = strlen(s2);
        char *out = (char *)malloc(len1 + len2 + 1); // +1 for null terminator
        if (!out) {
                return NULL;
        }
        memcpy(out, s1, len1);
        memcpy(out + len1, s2, len2);
        out[len1 + len2] = '\0';
        return out;
}

#endif // !QUIL_RTSTRCHR_H
