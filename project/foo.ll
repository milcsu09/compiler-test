; ModuleID = 'Main'
source_filename = "Main"

declare void @putd(double)

define void @Main() {
entry:
  %0 = fadd double 1.000000e+00, 1.000000e+00
  %1 = fadd double 1.000000e+00, %0
  call void @putd(double %1)
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %i.0 = phi double [ 0.000000e+00, %entry ], [ %3, %while.body ]
  call void @putd(double %i.0)
  %2 = fcmp olt double %i.0, 1.000000e+01
  br i1 %2, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %3 = fadd double %i.0, 1.000000e+00
  br label %while.cond

while.end:                                        ; preds = %while.cond
  ret void
}
