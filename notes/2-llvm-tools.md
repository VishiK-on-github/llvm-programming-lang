Commands:

To emit an llvm ir file from c++ code:

`clang++ -S emit-llvm <c++ filename>`

To generate .exe from .ll file:

`clang++ -o test <llvm ir name | file.ll>`

llvm-as is used to generate .bc file which is bitcode representation:

`llvm-as test.ll`

llvm-dis is used to convert bitcode to .ll format (IR):

`llvm-dis test.bc -o test-2.ll`

To generate native assembly code we can use clang:

`clang++ -S test.ll`

- llvm-as: converts human-readable IR to machine-level bitcode
- llvm-dis: converts machine-level bitcode to human-readable IR
