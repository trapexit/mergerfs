/*
  ISC License

  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#pragma once

#include "lfmp.h"

#include "node.h"

static lfmp_t g_NODE_FMP;

__attribute__((constructor))
void
__construct_g_NODE_FMP()
{
  lfmp_init(&g_NODE_FMP,sizeof(struct node),256);
}

__attribute__((destructor))
void
__destruct__g_NODE_FMP()
{
  lfmp_destroy(&g_NODE_FMP);
}

struct node*
node_alloc()
{
  return lfmp_calloc(&g_NODE_FMP);
}

void
node_free(struct node *node_)
{
  lfmp_free(&g_NODE_FMP,node_);
}

int
node_gc()
{
  return lfmp_gc(&g_NODE_FMP);
}

lfmp_t*
node_lfmp()
{
  return &g_NODE_FMP;
}
