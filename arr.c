#include "arr.h"
#include "err.h"

arr_metadata *internal_arr_get_metadata(u8 *memory)
{
  return ((arr_metadata *)(memory - sizeof(arr_metadata)));
}

u8 *internal_arr_new(size_t len, size_t elem_size)
{
  u8 *memory = malloc(sizeof(arr_metadata) + len * elem_size);
  memcpy(memory, &(arr_metadata){
    .len = len,
    .count = 0,
    .elem_size = elem_size
  }, sizeof(arr_metadata));

  return memory + sizeof(arr_metadata);
}

u8 *internal_arr_base_ptr(u8 *memory)
{
  return memory - sizeof(arr_metadata);
}

void arr_pop(void *thing, void *out)
{
  void **memory = thing;
  arr_metadata *data = internal_arr_get_metadata(*memory);
  if (data->count == 0) err("popped when count is 0");
  data->count--;
  memcpy(out, *memory + data->elem_size * data->count, data->elem_size);
}

void arr_add(void *thing, void const *item)
{
  void **memory = thing;
  arr_metadata *data = internal_arr_get_metadata(*memory);

  if (data->len == data->count)
  {
    data->len *= 2;
    size_t new_size_in_bytes =
      sizeof(arr_metadata) + data->len * data->elem_size;
    u8 *new_base_ptr = realloc(internal_arr_base_ptr(*memory),
                               new_size_in_bytes);
    *memory = new_base_ptr + sizeof(arr_metadata);

    data = internal_arr_get_metadata(*memory);
  }

  memcpy(*memory + data->count * data->elem_size, item, data->elem_size);
  data->count++;
}

bool internal_arr_has(u8 *memory, u8 *element, size_t elem_size)
{
  arr_metadata *data = internal_arr_get_metadata(memory);

  if (data->elem_size != elem_size)
  {
    err("incompatible type");
  }

  for (int i = 0; i < data->count; i++)
  {
    u8 *a = memory + data->elem_size * i;
    if (!memcmp(a, element, data->elem_size))
    {
      return true;
    }
  }

  return false;
}

void internal_arr_del(u8 *memory)
{
  u8 *base = internal_arr_base_ptr(memory);
  free(base);
}

void *internal_arr_at(u8 *memory, size_t n)
{
  arr_metadata *meta = internal_arr_get_metadata(memory);
  return memory + meta->elem_size * n;
}

void arr_clear(void *memory)
{
  arr_metadata *meta = internal_arr_get_metadata(memory);
  meta->count = 0;
}

void arr_add_bulk(void *thing, void *src)
{
  void **dst = thing;
  arr_metadata *dst_meta = internal_arr_get_metadata(*dst);
  arr_metadata *src_meta = internal_arr_get_metadata(src);

  if (src_meta->elem_size != dst_meta->elem_size)
  {
    err("Mismatched element sizes in arr_add_bulk!");
  }

  if (src_meta->count == 0) return;

  size_t new_count = dst_meta->count + src_meta->count;
  if (new_count > dst_meta->len)
  {
    size_t next = (size_t)pow(2, ceil(log2((double)new_count)));

    void *new_memory = realloc(internal_arr_base_ptr(*dst),
                               sizeof(arr_metadata) +
                               next * dst_meta->elem_size);
    ((arr_metadata *)new_memory)->len = next;
    *dst = new_memory + sizeof(arr_metadata);

    dst_meta = internal_arr_get_metadata(*dst);
  }

  memcpy(*dst + dst_meta->count * dst_meta->elem_size, src,
         src_meta->count * src_meta->elem_size);
  dst_meta->count = new_count;
}

void
arr_add_arr(void *thing, void *src_arr, size_t src_len, size_t src_elem_size)
{
  void **dst = thing;
  arr_metadata *dst_meta = internal_arr_get_metadata(*dst);

  if (src_len == 0) return;

  size_t new_count = dst_meta->count + src_len;
  if (new_count > dst_meta->len)
  {
    size_t next = (size_t)pow(2, ceil(log2((double)new_count)));

    void *new_memory = realloc(internal_arr_base_ptr(*dst),
                               sizeof(arr_metadata) +
                               next * dst_meta->elem_size);
    ((arr_metadata *)new_memory)->len = next;
    *dst = new_memory + sizeof(arr_metadata);

    dst_meta = internal_arr_get_metadata(*dst);
  }

  memcpy(*dst + dst_meta->count * dst_meta->elem_size, src_arr,
         src_len * src_elem_size);
  dst_meta->count = new_count;
}

void *arr_end(void *memory)
{
  arr_metadata *meta = internal_arr_get_metadata(memory);
  return memory + meta->elem_size * meta->count;
}

bool arr_is_empty(void *memory)
{
  return internal_arr_get_metadata(memory)->count == 0;
}

char *arr_get_sz(char *memory)
{
  char *str = malloc(arr_len(memory) + 1);
  memcpy(str, memory, arr_len(memory));
  str[arr_len(memory)] = '\0';
  return str;
}

void arr_copy(void *_dst, void *src)
{
  void **dst = _dst;
  arr_metadata dst_meta = *internal_arr_get_metadata(*dst);
  arr_metadata src_meta = *internal_arr_get_metadata(src);
  if (dst_meta.elem_size != src_meta.elem_size)
  {
    err("arr_copy: incompatible arrays!");
  }

  if (src_meta.count == 0)
  {
    return;
  }

  if (dst_meta.len >= src_meta.len)
  {
    memcpy(internal_arr_base_ptr(*dst),
           internal_arr_base_ptr(src),
           sizeof(arr_metadata) + src_meta.len * src_meta.elem_size);
  }
  else
  {
    arr_del(*dst);
    (*dst) =
      memcpy(malloc(sizeof(arr_metadata) + src_meta.len * src_meta.elem_size),
             internal_arr_base_ptr(src),
             sizeof(arr_metadata) + src_meta.len * src_meta.elem_size) +
      sizeof(arr_metadata);
  }
}

void *arr_from_arr(size_t elem_size, size_t len, void *data)
{
  u8 *memory = malloc(sizeof(arr_metadata) + len * elem_size);
  memcpy(memory, &(arr_metadata){
    .len = len,
    .count = len,
    .elem_size = elem_size
  }, sizeof(arr_metadata));

  memcpy(memory + sizeof(arr_metadata), data, elem_size * len);

  return memory + sizeof(arr_metadata);
}
