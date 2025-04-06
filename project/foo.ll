; ModuleID = 'Main'
source_filename = "Main"

define double @avarage(double %a, double %b) {
entry:
  %0 = fadd double %a, %b
  %1 = fdiv double %0, 2.000000e+00
  ret double %1
}

declare double @sqrt(double)

define double @c_side(double %a, double %b) {
entry:
  %0 = fmul double %a, %a
  %1 = fmul double %b, %b
  %2 = fadd double %0, %1
  %3 = call double @sqrt(double %2)
  ret double %3
}
