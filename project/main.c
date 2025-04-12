#include <stdio.h>
#include <stdarg.h>
#include <math.h>

extern double Main ();

double
putd (double a)
{
  printf ("%g\n", a);
  return 0.0;
}

double
putsep (double c_, double count_)
{
  int count = count_;
  char c = c_;

  for (int i = 0; i < count; ++i)
    putc (c, stdout);

  putc ('\n', stdout);

  return 0.0;
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


extern double mandelbrot_converge (double, double);

double mandelbrot_plot (double xmin, double xmax, double xstep, double ymin, double ymax, double ystep)
{
  const char gradient[] = " .:-=+*%@#";

  for (double y = ymin; y < ymax; y += ystep)
    {
      for (double x = xmin; x < xmax; x += xstep)
        {
          double d = mandelbrot_converge (x, y);
          const int MAX_ITER = 10024;
          if (d >= MAX_ITER)
            putc ('#', stdout);
          else
          {

                double log_d = log(d + 1); // log(d + 1) to avoid log(0)
                double log_max = log(MAX_ITER + 1); // log(MAX_ITER + 1)
                int char_index = (int)((log_d / log_max) * (sizeof(gradient) - 1));  // Map to gradient

                putc(gradient[char_index], stdout);
          }
        }

      putc (10, stdout);
    }

  return 0.0;
}

int
main (void)
{
  return (int) Main ();
}

