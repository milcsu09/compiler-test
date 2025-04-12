#include <stdio.h>
#include <stdarg.h>

extern double Main ();

double
putd (double a)
{
  printf ("%g\n", a);
  return a;
}

double
putds (double count, ...)
{
  va_list args;
  va_start (args, count);

  for (int i = 0; i < (int)count; i++) {
    double val = va_arg (args, double);
    putd (val);
  }

  va_end (args);

  return 0.0;
}

int
main (void)
{
  return (int) Main ();
}

