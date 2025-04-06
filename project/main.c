
#include <stdio.h>

extern double avarage (double, double);
extern double c_side (double, double);

int
main (void)
{
  printf ("%f\n", avarage (5, 10));
  printf ("%f\n", c_side (3, 4));
  return 0;
}

