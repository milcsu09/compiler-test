#ifndef STRING_H
#define STRING_H

#include <stddef.h>

struct arena;

char *string_copy (const char *, struct arena *);
char *string_copy_n (const char *, size_t, struct arena *);
char *string_copy_until (const char *, const char *, struct arena *);
// void string_escape (char *);

#endif // STRING_H

