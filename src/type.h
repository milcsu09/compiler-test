#ifndef TYPE_H
#define TYPE_H

#include "arena.h"

// Here, I use "_kind" instead of "_type".
enum type_kind
{
  TYPE_VOID,
  TYPE_F64,
  TYPE_BOOL,
  TYPE_FUNCTION,
};

struct type_function
{
  struct type *return_t;
  struct type **argument_t;
  size_t argument_n;
  int variadic;
};

union type_value
{
  struct type_function function;
};

struct type
{
  enum type_kind kind;
  union type_value value;
};

const char *type_kind_string (enum type_kind);

struct type *type_create (enum type_kind, struct arena *);
struct type *type_create_f (struct type_function, struct arena *);

void type_destroy (struct type *);

void type_debug_print (struct type *);

int type_match (struct type *, struct type *);

int type_can_cast (enum type_kind, enum type_kind);

#endif // TYPE_H

