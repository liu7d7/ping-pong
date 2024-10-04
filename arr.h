#pragma once

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

typedef struct arr_metadata
{
  size_t len;
  size_t count;
  size_t elem_size;
} arr_metadata;

typedef uint8_t u8;

arr_metadata *internal_arr_get_metadata(u8 *memory);

u8 *internal_arr_new(size_t len, size_t elem_size);

u8 *internal_arr_base_ptr(u8 *memory);

#define arr_new_sized(type, initial_size) ((type*) internal_arr_new(initial_size, sizeof(type)))
#define arr_new(type) arr_new_sized(type, 4)

void arr_add(void *memory, void const *item);
void arr_pop(void *memory, void *item);

void *arr_from_arr(size_t elem_size, size_t len, void *data);

bool internal_arr_has(u8 *memory, u8 *element, size_t elem_size);

#define arr_has(this, element) internal_arr_has((byte*) this, (byte*) &element, sizeof(element))
#define arr_has_i(this, type, ...) internal_arr_has((byte*) this, (byte*) &(type) {__VA_ARGS__}, sizeof(type))

#define arr_last(this) &(this)[arr_len(this) - 1]

void
arr_add_arr(void *thing, void *src_arr, size_t src_len, size_t src_elem_size);

#define arr_len(this) internal_arr_get_metadata((u8*) this)->count
#define arr_setlen(this, n) (internal_arr_get_metadata((u8*) this)->count = (n))
#define arr_elem_size(this) internal_arr_get_metadata((u8*) this)->elem_size
#define arr_cap(this) internal_arr_get_metadata((u8*) this)->len

void internal_arr_del(u8 *memory);

#define arr_del(this) internal_arr_del((u8*) this)

void *internal_arr_at(u8 *memory, size_t n);

#define arr_at(this, n) internal_arr_at((u8*) this, n)

void arr_clear(void *memory);

void arr_add_bulk(void *dst, void *src);

void *arr_end(void *memory);

bool arr_is_empty(void *memory);

char *arr_get_sz(char *memory);

void arr_copy(void *dst, void *src);
