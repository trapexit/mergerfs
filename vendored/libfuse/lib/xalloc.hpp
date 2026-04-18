#pragma once

#include <stdio.h>
#include <stdlib.h>

#define xstrdup(S) xstrdup_impl((S),__FILE__,__LINE__)
#define xmalloc(S) xmalloc_impl((S),__FILE__,__LINE__)

static
inline
char*
xstrdup_impl(const char     *s_,
             const char     *file_,
             const unsigned  line_)
{
  char *p;

  if(s_ == NULL)
    {
      fprintf(stderr,
              "mergerfs: xstrdup called with NULL - file: %s; line: %u\n",
              file_,
              line_);
      exit(EXIT_FAILURE);
    }

  p = strdup(s_);
  if(p == NULL)
    {
      fprintf(stderr,
              "mergerfs: strdup failed - file: %s; line: %u\n",
              file_,
              line_);
      exit(EXIT_FAILURE);
    }

  return p;
}

static
inline
void*
xmalloc_impl(const size_t    size_,
             const char     *file_,
             const unsigned  line_)
{
  void *p;

  p = malloc(size_);
  if(p == NULL)
    {
      fprintf(stderr,
              "mergerfs: malloc failed - file: %s; line: %u\n",
              file_,
              line_);
      exit(EXIT_FAILURE);
    }

  return p;
}
