#include "type.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static const char *const TYPE_KIND_STRING[] = {
  "Void",
  "F64",
  "Bool",
  "Function",
};

const char *
type_kind_string (enum type_kind kind)
{
  return TYPE_KIND_STRING[kind];
}

struct type *
type_create (enum type_kind kind, struct arena *arena)
{
  struct type *type;

  if (arena)
    type = arena_alloc (arena, sizeof (struct type));
  else
    type = calloc (1, sizeof (struct type));

  type->kind = kind;

  return type;
}

struct type *
type_create_f (struct type_function function, struct arena *arena)
{
  struct type *type;

  type = type_create (TYPE_FUNCTION, arena);

  type->value.function = function;

  return type;
}

void
type_destroy (struct type *type)
{
  free (type);
}

void
type_debug_print (struct type *type)
{
  switch (type->kind)
    {
    case TYPE_FUNCTION:
      {
        printf ("(");

        struct type_function function = type->value.function;
        size_t n = function.argument_n;

        for (size_t i = 0; i < n; ++i)
          {
            type_debug_print (function.argument_t[i]);
            if (i < n - 1)
              printf (", ");
          }

        if (function.variadic)
          printf (", ...");

        printf (") -> ");
        type_debug_print (function.return_t);
      }
      break;
    default:
      printf ("%s", type_kind_string (type->kind));
      break;
    }
}

int
type_match (struct type *t1, struct type *t2)
{
  if (t1->kind != t2->kind)
    return 0;

  switch (t1->kind)
    {
    case TYPE_FUNCTION:
      {
        struct type_function f1 = t1->value.function;
        struct type_function f2 = t2->value.function;

        if (f1.argument_n != f2.argument_n)
          return 0;

        if (f1.variadic != f2.variadic)
          return 0;

        if (type_match (f1.return_t, f2.return_t) == 0)
          return 0;

        for (size_t i = 0; i < f1.argument_n; ++i)
          if (type_match (f1.argument_t[i], f2.argument_t[i]) == 0)
            return 0;

        return 1;
      }
      break;
    default:
      return t1->kind == t2->kind;
    }
}

int
type_can_cast (enum type_kind t1, enum type_kind t2)
{
  if (t1 == t2)
    return 1;

  switch (t1)
    {
    case TYPE_F64:
      if (t2 == TYPE_F64)
        return 1;
      if (t2 == TYPE_BOOL)
        return 1;
    case TYPE_BOOL:
      if (t2 == TYPE_F64)
        return 1;
      if (t2 == TYPE_BOOL)
        return 1;
    default:
      return 0;
    }
}

