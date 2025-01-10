; ModuleID = 'EvaLLVM'
source_filename = "EvaLLVM"
target triple = "arm64-apple-macosx14.0.0"

@VERSION = global i32 1, align 4
@0 = private unnamed_addr constant [9 x i8] c"sum: %d\0A\00", align 1

declare i32 @printf(i8*, ...)

declare i8* @GC_malloc(i64)

define i32 @main() {
entry:
  %0 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @0, i32 0, i32 0), i32 20)
  ret i32 0
}
