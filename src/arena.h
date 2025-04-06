#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

struct region;

struct arena
{
  struct region *start;
  struct region *end;
};

void *arena_alloc (struct arena *arena, size_t nbytes);
void arena_destroy (struct arena *arena);

#endif // ARENA_H

