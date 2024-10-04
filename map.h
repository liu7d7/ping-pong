#pragma once

#include "arr.h"
#include "err.h"

typedef struct map
{
  void *keys;
  void *vals;
  size_t *idx;
  size_t cap, count, key_size, val_size;
  float load_factor;

  bool (*eq)(void *lhs, void *rhs);

  uint32_t (*hash)(void *key);
} map;

void *map_add(map *l, void *key, void *val);

void *map_at(map *l, void *key);

void *map_at_or(map *l, void *key, void *or);

bool map_has(map *l, void *key);

map
map_new(size_t initial_size, size_t key_size, size_t val_size,
        float load_factor,
        bool(*eq)(void *, void *), uint32_t(*hash)(void *));
