#ifndef ERROR_H
#define ERROR_H

#include <stddef.h>

struct location
{
  const char *context;
  size_t line;
  size_t column;
};

struct error
{
  char message[256];

  /* Possible new fields */
};

void location_advance (struct location *, char);
void location_debug_print (struct location);

struct error error_create (const char *fmt, ...);

#endif // ERROR_H

