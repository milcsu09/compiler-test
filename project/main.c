#include <stdio.h>
#include <stdarg.h>

extern void Main ();

void
putd (double a)
{
  printf ("%g\n", a);
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
  Main ();
  return 0;
}

