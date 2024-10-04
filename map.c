#include "map.h"

size_t *new_entries(size_t size)
{
  size_t *it = malloc(sizeof(size_t) * size);
  memset(it, 0xff, sizeof(size_t) * size);

  return it;
}

bool idx_is_invalid(size_t entry)
{
  return entry == SIZE_MAX;
}

map
map_new(size_t initial_size, size_t key_size, size_t val_size,
        float load_factor,
        bool (*eq)(void *, void *), uint32_t (*hash)(void *))
{
  return (map){
    .keys = internal_arr_new(initial_size, key_size),
    .vals = internal_arr_new(initial_size, val_size),
    .idx = new_entries(initial_size),
    .cap = initial_size,
    .count = 0,
    .load_factor = load_factor,
    .eq = eq,
    .hash = hash,
    .key_size = key_size,
    .val_size = val_size
  };
}

void map_add_idx(map *l, size_t idx)
{
  void *key = l->keys + idx * l->key_size;
  uint32_t hash = l->hash(key);
  uint32_t entry_idx = hash % l->cap;
//  uint32_t i = 1;
  while (!idx_is_invalid(l->idx[entry_idx]) &&
         !l->eq(key, l->keys + l->idx[entry_idx] * l->key_size))
  {
//    entry_idx += i * i;
//    i++;
    entry_idx = (entry_idx + 1 == l->cap ? 0 : entry_idx + 1);
  }

  l->idx[entry_idx] = idx;
}

map map_resize(map *l, size_t new_size)
{
  map new = {
    .keys = l->keys,
    .vals = l->vals,
    .idx = new_entries(new_size),
    .cap = new_size,
    .count = l->count,
    .load_factor = l->load_factor,
    .eq = l->eq,
    .hash = l->hash,
    .key_size = l->key_size,
    .val_size = l->val_size
  };

  for (size_t i = 0, len = arr_len(l->keys); i < len; i++)
  {
    map_add_idx(&new, i);
  }

  free(l->idx);

  return new;
}

void *map_add(map *l, void *key, void *val)
{
  if (l->count + 1 > l->cap * l->load_factor)
  {
    *l = map_resize(l, l->cap * 2);
  }

  arr_add(&l->keys, key);
  arr_add(&l->vals, val);

  map_add_idx(l, arr_len(l->keys) - 1);
  l->count++;

  return l->vals + arr_len(l->vals) * l->val_size - l->val_size;
}

void *map_at(map *l, void *key)
{
  uint32_t hash = l->hash(key);
  uint32_t entry_idx = hash % l->cap;
//  uint32_t i = 1;
  while (!idx_is_invalid(l->idx[entry_idx]) &&
         !l->eq(key, l->keys + l->idx[entry_idx] * l->key_size))
  {
//    entry_idx += i * i;
//    i++;
    entry_idx = (entry_idx + 1 == l->cap ? 0 : entry_idx + 1);
  }

  if (idx_is_invalid(l->idx[entry_idx])) return NULL;

  return l->vals + l->idx[entry_idx] * l->val_size;
}

void *map_at_or(map *l, void *key, void *or)
{
  uint32_t hash = l->hash(key);
  uint32_t entry_idx = hash % l->cap;
//  uint32_t i = 1;
  while (!idx_is_invalid(l->idx[entry_idx]) &&
         !l->eq(key, l->keys + l->idx[entry_idx] * l->key_size))
  {
//    entry_idx += i * i;
//    i++;
    entry_idx = (entry_idx + 1 == l->cap ? 0 : entry_idx + 1);
  }

  if (idx_is_invalid(l->idx[entry_idx])) return or;

  return l->vals + l->idx[entry_idx] * l->val_size;
}

bool map_has(map *l, void *key)
{
  return map_at(l, key) != NULL;
}
