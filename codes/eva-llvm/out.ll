; ModuleID = 'EvaLLVM'
source_filename = "EvaLLVM"
target triple = "arm64-apple-macosx14.0.0"

%Point_vTable = type { i32 (%Point*)*, i32 (%Point*, i32, i32)* }
%Point = type { %Point_vTable*, i32, i32 }
%Point3D_vTable = type { i32 (%Point3D*)*, i32 (%Point3D*, i32, i32, i32)* }
%Point3D = type { %Point3D_vTable*, i32, i32, i32 }

@VERSION = global i32 1, align 4
@Point_vTable = global %Point_vTable { i32 (%Point*)* @Point_calc, i32 (%Point*, i32, i32)* @Point_constructor }, align 4
@Point3D_vTable = global %Point3D_vTable { i32 (%Point3D*)* @Point3D_calc, i32 (%Point3D*, i32, i32, i32)* @Point3D_constructor }, align 4
@0 = private unnamed_addr constant [11 x i8] c"p2.x = %d\0A\00", align 1
@1 = private unnamed_addr constant [11 x i8] c"p2.y = %d\0A\00", align 1
@2 = private unnamed_addr constant [11 x i8] c"p2.z = %d\0A\00", align 1

declare i32 @printf(i8*, ...)

declare i8* @GC_malloc(i64)

define i32 @main() {
entry:
  %p1 = call i8* @GC_malloc(i64 16)
  %0 = bitcast i8* %p1 to %Point*
  %1 = call i32 @Point_constructor(%Point* %0, i32 10, i32 20)
  %p2 = call i8* @GC_malloc(i64 24)
  %2 = bitcast i8* %p2 to %Point3D*
  %3 = call i32 @Point3D_constructor(%Point3D* %2, i32 100, i32 200, i32 300)
  %px = getelementptr inbounds %Point3D, %Point3D* %2, i32 0, i32 1
  %x = load i32, i32* %px, align 4
  %4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([11 x i8], [11 x i8]* @0, i32 0, i32 0), i32 %x)
  %py = getelementptr inbounds %Point3D, %Point3D* %2, i32 0, i32 2
  %y = load i32, i32* %py, align 4
  %5 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([11 x i8], [11 x i8]* @1, i32 0, i32 0), i32 %y)
  %pz = getelementptr inbounds %Point3D, %Point3D* %2, i32 0, i32 3
  %z = load i32, i32* %pz, align 4
  %6 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([11 x i8], [11 x i8]* @2, i32 0, i32 0), i32 %z)
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
  %x4 = load i32, i32* %x2, align 4
  %self5 = load %Point*, %Point** %self1, align 8
  %px = getelementptr inbounds %Point, %Point* %self5, i32 0, i32 1
  store i32 %x4, i32* %px, align 4
  %y6 = load i32, i32* %y3, align 4
  %self7 = load %Point*, %Point** %self1, align 8
  %py = getelementptr inbounds %Point, %Point* %self7, i32 0, i32 2
  store i32 %y6, i32* %py, align 4
  ret i32 %y6
}

define i32 @Point_calc(%Point* %self) {
entry:
  %self1 = alloca %Point*, align 8
  store %Point* %self, %Point** %self1, align 8
  %self2 = load %Point*, %Point** %self1, align 8
  %px = getelementptr inbounds %Point, %Point* %self2, i32 0, i32 1
  %x = load i32, i32* %px, align 4
  %self3 = load %Point*, %Point** %self1, align 8
  %py = getelementptr inbounds %Point, %Point* %self3, i32 0, i32 2
  %y = load i32, i32* %py, align 4
  %tmpadd = add i32 %x, %y
  ret i32 %tmpadd
}

define i32 @Point3D_constructor(%Point3D* %self, i32 %x, i32 %y, i32 %z) {
entry:
  %self1 = alloca %Point3D*, align 8
  store %Point3D* %self, %Point3D** %self1, align 8
  %x2 = alloca i32, align 4
  store i32 %x, i32* %x2, align 4
  %y3 = alloca i32, align 4
  store i32 %y, i32* %y3, align 4
  %z4 = alloca i32, align 4
  store i32 %z, i32* %z4, align 4
  %z5 = load i32, i32* %z4, align 4
  %self6 = load %Point3D*, %Point3D** %self1, align 8
  %pz = getelementptr inbounds %Point3D, %Point3D* %self6, i32 0, i32 3
  store i32 %z5, i32* %pz, align 4
  ret i32 %z5
}

define i32 @Point3D_calc(%Point3D* %self) {
entry:
  %self1 = alloca %Point3D*, align 8
  store %Point3D* %self, %Point3D** %self1, align 8
  ret i32 0
}
