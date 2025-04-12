; ModuleID = 'Main'
source_filename = "Main"

declare double @putd(double)

declare double @putds(double, ...)

declare double @sqrt(double)

define double @Main() {
entry:
  %0 = call double (double, ...) @putds(double 3.000000e+00, double 1.000000e+00, double 2.000000e+00, double 3.000000e+00)
  ret double 0.000000e+00
}
