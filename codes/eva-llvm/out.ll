; ModuleID = 'EvaLLVM'
source_filename = "EvaLLVM"

%Point = type {
  i32, ; x [0]
  i32 ; y [1]
}

define void @Point_constructor(%Point* %self, i32 %x, i32 %y) {
  
  ; (set (prop self x) x)
  %1 = getelementptr %Point, %Point* %self, i32 0, i32 0 ; x address
  store i32 %x, i32* %1

  ; (set (prop self y) y)
  %2 = getelementptr %Point, %Point* %self, i32 0, i32 1 ; y address
  store i32 %y, i32* %2

  ret void
}

define i32 @main() {
  
  %p = alloca %Point ; stack allocated TODO: instances need to be heap allocated

  call void @Point_constructor(%Point* %p, i32 10, i32 20)

  %1 = getelementptr %Point, %Point* %p, i32 0, i32 0 ; x address
  %2 = load i32, i32* %1

  ret i32 %2
}