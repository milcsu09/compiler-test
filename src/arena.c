#include "arena.h"
#include <stdint.h>
#include <stdlib.h>

struct region
{
  struct region *next;
  size_t size;
  size_t capacity;
  uintptr_t storage[];
};

static struct region *
region_create (size_t capacity)
{
  size_t nbytes = sizeof (struct region) + sizeof (uintptr_t) * capacity;
  struct region *region;

  region = calloc (nbytes, 1);
  region->next = NULL;
  region->size = 0;
  region->capacity = capacity;

  return region;
}

static void
region_destroy (struct region *region)
{
  free (region);
}

void *
arena_alloc (struct arena *arena, size_t nbytes)
{
  size_t size = (nbytes + sizeof (uintptr_t) - 1) / sizeof (uintptr_t);

  if (!arena->end)
    {
      size_t capacity = 1 * 1024;
      if (size > capacity)
        capacity = size;

      arena->end = region_create (capacity);
      arena->start = arena->end;
    }

  while (arena->end->size + size > arena->end->capacity && arena->end->next)
    arena->end = arena->end->next;

  if (arena->end->size + size > arena->end->capacity)
    {
      size_t capacity = 1 * 1024;
      if (size > capacity)
        capacity = size;

      arena->end->next = region_create (capacity);
      arena->end = arena->end->next;
    }

  void *result = &arena->end->storage[arena->end->size];
  arena->end->size += size;
  return result;
}

void
arena_destroy (struct arena *arena)
{
  struct region *current = arena->start;

  while (current)
    {
      struct region *t = current;
      current = current->next;
      region_destroy (t);
    }

  arena->start = NULL;
  arena->end = NULL;
}

