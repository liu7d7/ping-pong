#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define err(...) err_impl(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define lg(...) lg_impl(__FILE__, __func__, __LINE__, __VA_ARGS__)

_Noreturn
static void 
err_impl(char const *file, char const *fn, int line, char const *fmt, ...) 
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s:%s:%d | ", file, fn, line);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(-1);
}

static void 
lg_impl(char const *file, char const *fn, int line, char const *fmt, ...) 
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s:%s:%d | ", file, fn, line);
  vfprintf(stderr, fmt, args);
  va_end(args);
}
