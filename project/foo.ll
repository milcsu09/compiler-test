; ModuleID = 'Main'
source_filename = "Main"

declare double @putd(double)

declare double @putds(double, ...)

declare double @putsep(double, double)

declare double @mandelbrot_plot(double, double, double, double, double, double)

declare double @sqrt(double)

define double @"||"(double %a, double %b) {
entry:
  %cond = fcmp ueq double %a, 0.000000e+00
  %cond1 = fcmp ueq double %b, 0.000000e+00
  %. = select i1 %cond1, double 0.000000e+00, double 1.000000e+00
  %0 = select i1 %cond, double %., double 1.000000e+00
  ret double %0
}

define double @"&&"(double %a, double %b) {
entry:
  %cond = fcmp ueq double %a, 0.000000e+00
  %cond1 = fcmp ueq double %b, 0.000000e+00
  %. = select i1 %cond1, double 0.000000e+00, double 1.000000e+00
  %0 = select i1 %cond, double 0.000000e+00, double %.
  ret double %0
}

define double @"=="(double %a, double %b) {
entry:
  %0 = fsub double %a, %b
  %cond = fcmp ueq double %0, 0.000000e+00
  %. = select i1 %cond, double 1.000000e+00, double 0.000000e+00
  ret double %.
}

define double @"!="(double %a, double %b) {
entry:
  %0 = fsub double %a, %b
  %cond = fcmp ueq double %0, 0.000000e+00
  %. = select i1 %cond, double 0.000000e+00, double 1.000000e+00
  ret double %.
}

define double @"!"(double %a) {
entry:
  %cond = fcmp ueq double %a, 0.000000e+00
  %. = select i1 %cond, double 1.000000e+00, double 0.000000e+00
  ret double %.
}

define double @-(double %a) {
entry:
  %0 = fsub double 0.000000e+00, %a
  ret double %0
}

define double @mandelbrot_converge_help(double %real, double %imag, double %iters, double %creal, double %cimag) {
entry:
  %0 = fcmp ogt double %iters, 1.002400e+04
  %1 = uitofp i1 %0 to double
  %2 = fmul double %real, %real
  %3 = fmul double %imag, %imag
  %4 = fadd double %2, %3
  %5 = fcmp ogt double %4, 4.000000e+00
  %6 = uitofp i1 %5 to double
  %7 = call double @"||"(double %1, double %6)
  %cond = fcmp ueq double %7, 0.000000e+00
  br i1 %cond, label %8, label %16

8:                                                ; preds = %entry
  %9 = fsub double %2, %3
  %10 = fadd double %9, %creal
  %11 = fmul double %real, 2.000000e+00
  %12 = fmul double %11, %imag
  %13 = fadd double %12, %cimag
  %14 = fadd double %iters, 1.000000e+00
  %15 = call double @mandelbrot_converge_help(double %10, double %13, double %14, double %creal, double %cimag)
  br label %16

16:                                               ; preds = %entry, %8
  %17 = phi double [ %15, %8 ], [ %iters, %entry ]
  ret double %17
}

define double @mandelbrot_converge(double %real, double %imag) {
entry:
  %0 = call double @mandelbrot_converge_help(double %real, double %imag, double 0.000000e+00, double %real, double %imag)
  ret double %0
}

define double @Main() {
entry:
  %0 = call double @-(double 2.750000e+00)
  %1 = call double @-(double 1.500000e+00)
  %2 = call double @mandelbrot_plot(double %0, double 1.250000e+00, double 3.750000e-02, double %1, double 1.500000e+00, double 5.000000e-02)
  ret double 0.000000e+00
}
