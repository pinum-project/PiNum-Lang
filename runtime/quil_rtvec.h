/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

#ifndef QUIL_RTVEC_H
#define QUIL_RTVEC_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// bound checking for array access arr[idx]
static inline void __quil_panic_bounds(size_t len, long long idx) {
        fprintf(stderr, "quil: index out of bounds: %lld (size %zu)\n", idx, len);
        exit(1);
}
#define __quil_check_bounds(len, idx) \
        ((unsigned long long)(idx) < (unsigned long long)(len) ? (idx) : (__quil_panic_bounds(len, (long long)(idx)), 0))

/* ---------- GENERIC VEC TEMPLATE ---------- */
// QUIL_VEC_DEFINE(T, ETYPE, PTRTYPE, VAPROMO, FMT)
//   T        - name token used for the concrete type: vec_<T>, __quil_vec_<T>_*
//   ETYPE    - C type of a stored element (int, float, char, char *...)
//   PTRTYPE  - C type of the data pointer field (int *, char **...)
//   VAPROMO  - type va_arg promotes to for this element type
//              (char/bool promote to int, float promotes to double)
//   FMT      - printf format string for a single element
#define QUIL_VEC_DEFINE(T, ETYPE, PTRTYPE, VAPROMO, FMT)                                      \
        typedef struct {                                                                       \
                PTRTYPE data;                                                                  \
                size_t size;                                                                   \
                size_t capacity;                                                               \
        } vec_##T;                                                                             \
                                                                                               \
        static inline vec_##T __quil_vec_##T##_init(int count, ...) {                         \
                vec_##T v = {NULL, 0, 0};                                                      \
                if (count > 0) {                                                               \
                        v.data = (PTRTYPE)malloc(sizeof(ETYPE) * (size_t)count);               \
                        if (v.data) {                                                          \
                                v.size = v.capacity = (size_t)count;                           \
                                va_list ap;                                                    \
                                va_start(ap, count);                                           \
                                for (int i = 0; i < count; i++) {                              \
                                        v.data[i] = va_arg(ap, VAPROMO);                       \
                                }                                                              \
                                va_end(ap);                                                    \
                        }                                                                      \
                }                                                                              \
                return v;                                                                      \
        }                                                                                      \
                                                                                               \
        static inline void __quil_vec_##T##_append(vec_##T *v, ETYPE item) {                  \
                if (v->size >= v->capacity) {                                                  \
                        size_t new_cap = v->capacity == 0 ? 4 : v->capacity * 2;               \
                        PTRTYPE new_data = (PTRTYPE)realloc(v->data, sizeof(ETYPE) * new_cap); \
                        if (!new_data) {                                                       \
                                return;                                                        \
                        }                                                                      \
                        v->data = new_data;                                                    \
                        v->capacity = new_cap;                                                 \
                }                                                                              \
                v->data[v->size++] = item;                                                     \
        }                                                                                      \
                                                                                               \
        static inline void __quil_vec_##T##_print(vec_##T v) {                                \
                printf("[");                                                                   \
                for (size_t i = 0; i < v.size; i++) {                                          \
                        if (i) printf(", ");                                                   \
                        printf(FMT, v.data[i]);                                                \
                }                                                                              \
                printf("]");                                                                   \
        }

/* ---------- CONCRETE VECS ---------- */
QUIL_VEC_DEFINE(int8, int8_t, int8_t *, int, "%d")
QUIL_VEC_DEFINE(int16, int16_t, int16_t *, int, "%d")
QUIL_VEC_DEFINE(int32, int32_t, int32_t *, int, "%d")
QUIL_VEC_DEFINE(int64, int64_t, int64_t *, long long, "%lld")
QUIL_VEC_DEFINE(uint8, uint8_t, uint8_t *, int, "%u")
QUIL_VEC_DEFINE(uint16, uint16_t, uint16_t *, int, "%u")
QUIL_VEC_DEFINE(uint32, uint32_t, uint32_t *, unsigned int, "%u")
QUIL_VEC_DEFINE(uint64, uint64_t, uint64_t *, unsigned long long, "%llu")
QUIL_VEC_DEFINE(float32, float, float *, double, "%f")
QUIL_VEC_DEFINE(float64, double, double *, double, "%f")
QUIL_VEC_DEFINE(char, char, char *, int, "%c")
QUIL_VEC_DEFINE(bool, int, int *, int, "%d")
QUIL_VEC_DEFINE(string, char *, char **, char *, "%s")

#endif // !QUIL_RTVEC_H
