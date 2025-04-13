; ModuleID = 'Main'
source_filename = "Main"

declare void @putd(double)

declare void @separator()

define i1 @or(i1 %a, i1 %b) {
entry:
  br i1 %a, label %if.true, label %if.false

if.true:                                          ; preds = %entry
  br label %if.merge

if.false:                                         ; preds = %entry
  br label %if.merge

if.merge:                                         ; preds = %if.false, %if.true
  %iftmp = phi i1 [ %a, %if.true ], [ %b, %if.false ]
  ret i1 %iftmp
}

define i1 @and(i1 %a, i1 %b) {
entry:
  br i1 %a, label %if.true, label %if.false

if.true:                                          ; preds = %entry
  br label %if.merge

if.false:                                         ; preds = %entry
  br label %if.merge

if.merge:                                         ; preds = %if.false, %if.true
  %iftmp = phi i1 [ %b, %if.true ], [ %a, %if.false ]
  ret i1 %iftmp
}

define double @Main() {
entry:
  %0 = call i1 @or(i1 false, i1 false)
  %bool_zext = zext i1 %0 to i32
  %bool_to_f64 = sitofp i32 %bool_zext to double
  call void @putd(double %bool_to_f64)
  %1 = call i1 @or(i1 true, i1 false)
  %bool_zext1 = zext i1 %1 to i32
  %bool_to_f642 = sitofp i32 %bool_zext1 to double
  call void @putd(double %bool_to_f642)
  %2 = call i1 @or(i1 false, i1 true)
  %bool_zext3 = zext i1 %2 to i32
  %bool_to_f644 = sitofp i32 %bool_zext3 to double
  call void @putd(double %bool_to_f644)
  %3 = call i1 @or(i1 true, i1 true)
  %bool_zext5 = zext i1 %3 to i32
  %bool_to_f646 = sitofp i32 %bool_zext5 to double
  call void @putd(double %bool_to_f646)
  call void @separator()
  %4 = call i1 @and(i1 false, i1 false)
  %bool_zext7 = zext i1 %4 to i32
  %bool_to_f648 = sitofp i32 %bool_zext7 to double
  call void @putd(double %bool_to_f648)
  %5 = call i1 @and(i1 true, i1 false)
  %bool_zext9 = zext i1 %5 to i32
  %bool_to_f6410 = sitofp i32 %bool_zext9 to double
  call void @putd(double %bool_to_f6410)
  %6 = call i1 @and(i1 false, i1 true)
  %bool_zext11 = zext i1 %6 to i32
  %bool_to_f6412 = sitofp i32 %bool_zext11 to double
  call void @putd(double %bool_to_f6412)
  %7 = call i1 @and(i1 true, i1 true)
  %bool_zext13 = zext i1 %7 to i32
  %bool_to_f6414 = sitofp i32 %bool_zext13 to double
  call void @putd(double %bool_to_f6414)
  ret double 0.000000e+00
}
