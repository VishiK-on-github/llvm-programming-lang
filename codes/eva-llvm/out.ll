; ModuleID = 'EvaLLVM'
source_filename = "EvaLLVM"
target triple = "arm64-apple-macosx14.0.0"

%Point = type { i32, i32 }

@VERSION = global i32 1, align 4

declare i32 @printf(i8*, ...)

declare i8* @GC_malloc(i64)

define i32 @main() {
entry:
  %p = call i8* @GC_malloc(i64 8)
  %0 = bitcast i8* %p to %Point*
  %1 = call i32 @Point_constructor(%Point* %0, i32 10, i32 20)
  ret i32 0
}

define i32 @Point_constructor(%Point* %self, i32 %x, i32 %y) {
entry:
  %self1 = alloca %Point*, align 8
  store %Point* %self, %Point** %self1, align 8
  %x2 = alloca i32, align 4
  store i32 %x, i32* %x2, align 4
  %y3 = alloca i32, align 4
  store i32 %y, i32* %y3, align 4
  ret i32 0
}

define i32 @Point_calc(%Point* %self) {
entry:
  %self1 = alloca %Point*, align 8
  store %Point* %self, %Point** %self1, align 8
  ret i32 0
}
