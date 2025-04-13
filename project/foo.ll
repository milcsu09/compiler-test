; ModuleID = 'Main'
source_filename = "Main"

declare void @putd(double)

declare void @separator()

define i1 @"!"(i1 %a) {
entry:
  %a1 = alloca i1, align 1
  store i1 %a, i1* %a1, align 1
  %a2 = load i1, i1* %a1, align 1
  br i1 %a2, label %if.true, label %if.false

if.true:                                          ; preds = %entry
  br label %if.merge

if.false:                                         ; preds = %entry
  br label %if.merge

if.merge:                                         ; preds = %if.false, %if.true
  %iftmp = phi i1 [ false, %if.true ], [ true, %if.false ]
  ret i1 %iftmp
}

define i1 @"=="(double %a, double %b) {
entry:
  %a1 = alloca double, align 8
  store double %a, double* %a1, align 8
  %b2 = alloca double, align 8
  store double %b, double* %b2, align 8
  %a3 = load double, double* %a1, align 8
  %b4 = load double, double* %b2, align 8
  %0 = fsub double %a3, %b4
  %f64_to_bool = fcmp one double %0, 0.000000e+00
  %1 = call i1 @"!"(i1 %f64_to_bool)
  ret i1 %1
}

define i1 @or(i1 %a, i1 %b) {
entry:
  %a1 = alloca i1, align 1
  store i1 %a, i1* %a1, align 1
  %b2 = alloca i1, align 1
  store i1 %b, i1* %b2, align 1
  %a3 = load i1, i1* %a1, align 1
  br i1 %a3, label %if.true, label %if.false

if.true:                                          ; preds = %entry
  %a4 = load i1, i1* %a1, align 1
  br label %if.merge

if.false:                                         ; preds = %entry
  %b5 = load i1, i1* %b2, align 1
  br label %if.merge

if.merge:                                         ; preds = %if.false, %if.true
  %iftmp = phi i1 [ %a4, %if.true ], [ %b5, %if.false ]
  ret i1 %iftmp
}

define i1 @and(i1 %a, i1 %b) {
entry:
  %a1 = alloca i1, align 1
  store i1 %a, i1* %a1, align 1
  %b2 = alloca i1, align 1
  store i1 %b, i1* %b2, align 1
  %a3 = load i1, i1* %a1, align 1
  br i1 %a3, label %if.true, label %if.false

if.true:                                          ; preds = %entry
  %b4 = load i1, i1* %b2, align 1
  br label %if.merge

if.false:                                         ; preds = %entry
  %a5 = load i1, i1* %a1, align 1
  br label %if.merge

if.merge:                                         ; preds = %if.false, %if.true
  %iftmp = phi i1 [ %b4, %if.true ], [ %a5, %if.false ]
  ret i1 %iftmp
}

define void @putb(i1 %a) {
entry:
  %a1 = alloca i1, align 1
  store i1 %a, i1* %a1, align 1
  %a2 = load i1, i1* %a1, align 1
  %bool_zext = zext i1 %a2 to i32
  %bool_to_f64 = sitofp i32 %bool_zext to double
  call void @putd(double %bool_to_f64)
  ret void
}

define double @Main() {
entry:
  %0 = call i1 @or(i1 false, i1 false)
  call void @putb(i1 %0)
  %1 = call i1 @or(i1 true, i1 false)
  call void @putb(i1 %1)
  %2 = call i1 @or(i1 false, i1 true)
  call void @putb(i1 %2)
  %3 = call i1 @or(i1 true, i1 true)
  call void @putb(i1 %3)
  call void @separator()
  %4 = call i1 @and(i1 false, i1 false)
  call void @putb(i1 %4)
  %5 = call i1 @and(i1 true, i1 false)
  call void @putb(i1 %5)
  %6 = call i1 @and(i1 false, i1 true)
  call void @putb(i1 %6)
  %7 = call i1 @and(i1 true, i1 true)
  call void @putb(i1 %7)
  call void @separator()
  %c1 = alloca i1, align 1
  store i1 true, i1* %c1, align 1
  %c2 = alloca i1, align 1
  %8 = call i1 @"=="(double 2.000000e+00, double 3.000000e+00)
  store i1 %8, i1* %c2, align 1
  store i1 true, i1* %c1, align 1
  %c11 = load i1, i1* %c1, align 1
  call void @putb(i1 %c11)
  %c22 = load i1, i1* %c2, align 1
  call void @putb(i1 %c22)
  %c13 = load i1, i1* %c1, align 1
  %c24 = load i1, i1* %c2, align 1
  %9 = call i1 @or(i1 %c13, i1 %c24)
  call void @putb(i1 %9)
  %c15 = load i1, i1* %c1, align 1
  %c26 = load i1, i1* %c2, align 1
  %10 = call i1 @and(i1 %c15, i1 %c26)
  call void @putb(i1 %10)
  ret double 0.000000e+00
}
