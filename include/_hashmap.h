/**************************************************
 * QUIL - Quick Unified Iterative Language
 * Language and Compiler toolchain, Frontend
 *
 * Copyright (c) 2026-present quil-project authors.
 * Licensed under the terms of the LICENSE file.
 *
 * Issues: <https://github.com/quil-project/quil>
 *************************************************/

#ifndef _HASHMAP_H
#define _HASHMAP_H

#include <stdbool.h>
#include <stddef.h>

// Initial hashmap node
typedef struct HMNode HMNode;
struct HMNode {
        void *key;
        void *value;
        size_t hash;
        HMNode *next;
};

typedef size_t (*hm_hash_fn)(const void *key);
typedef bool (*hm_eq_fn)(const void *a, const void *b);
typedef void (*hm_free_fn)(void *ptr);

typedef struct {
        HMNode **buckets;
        size_t capacity;
        size_t size;
        hm_hash_fn hash;
        hm_eq_fn eq;
        hm_free_fn key_free;
        hm_free_fn value_free;
        float max_load_factor; // size devided by capacity. default 1.0
} HashMap;

typedef struct {
        HashMap *map;
        size_t bucket_idx;
        HMNode *node;
} HashMapIter;

// --- Construction and Destruction ---
HashMap *hashmap_create(hm_hash_fn hash, hm_eq_fn eq, hm_free_fn key_free, hm_free_fn value_free);
void hashmap_free(HashMap *map);
void hashmap_clear(HashMap *map);
// --- capacity ---
size_t hashmap_size(HashMap *map); // get hashmap size
bool hashmap_empty(HashMap *map);  // check if the hashmap empty or not
// --- element access / lookup ---
bool hashmap_insert(HashMap *map, void *key, void *value);     // fails and returns false if key exists
bool hashmap_put(HashMap *map, void *key, void *value);        // overwrites the value if a pair with the same key exists
void *hashmap_get(HashMap *map, const void *key, bool *found); // retrieve pointer to the value associated with a key
bool hashmap_contains(HashMap *map, const void *key);          // checks if a specific key exists in the hashmap
bool hashmap_erase(HashMap *map, const void *key);             // removes a key value pair associated with the key
// --- iteration ---
HashMapIter hashmap_begin(HashMap *map);         // creates and returns a new iterator
bool hashmap_iter_valid(HashMapIter *it);        // check if the iterator is pointing to a valid node
void hashmap_iter_next(HashMapIter *it);         // moves to next node
void *hashmap_iter_key(HashMapIter *it);         // retrieve the pointer to the key the iterator is currently pointing at
void *hashmap_iter_value(HashMapIter *it);       // retrieve the pointer to the value the iterator is currently pointing at
HashMapIter hashmap_erase_iter(HashMapIter *it); // erase the node the iterator points at, return the next iterator
// --- bucket interface ---
size_t hashmap_bucket_count(HashMap *map);                    // number of buckets
size_t hashmap_bucket_size(HashMap *map, size_t bucket_idx);  // size of given bucket
float hashmap_load_factor(HashMap *map);                      // load factor of current map
float hashmap_max_load_factor(HashMap *map);                  // max load factor of current map
void hashmap_set_max_load_factor(HashMap *map, float lodfac); // set max load factor for current map
void hashmap_rehash(HashMap *map, size_t new_bucket_count);   // force hashmap to resize bucket array to new_bucket_count
void hashmap_reserve(HashMap *map, size_t element_count);     // pre allocate bucket size to element_count to hold element_count items without exceeding max_load_factor
// --- built in hash/eq helpers ---
// hm_hash_fn
size_t hm_hash_str(const void *key);
size_t hm_hash_int(const void *key);
size_t hm_hash_ptr(const void *key);
// hm_eq_fn
bool hm_eq_str(const void *a, const void *b);
bool hm_eq_int(const void *a, const void *b);
bool hm_eq_ptr(const void *a, const void *b);

#endif // !_HASHMAP_H
