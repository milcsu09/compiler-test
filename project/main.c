#include <stdio.h>
#include <stdarg.h>

extern double Main ();

double
putd (double a)
{
  printf ("%g\n", a);
  return 0.0;
}

void
separator()
{
  for (int i = 0; i < 25; ++i)
    putc ('-', stdout);
  putc ('\n', stdout);
}

int
main (void)
{
  return (int) Main ();
}

