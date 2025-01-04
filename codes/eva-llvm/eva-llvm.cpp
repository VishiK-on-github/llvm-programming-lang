/*
  Eva LLVM executable
*/

#include <string>
#include "./src/EvaLLVM.h"

int main(int argc, char const *argv[])
{
  // program to execute
  std::string program = R"(

  (class Point null
    (begin

      (var x 0)
      (var y 0)

      (def constructor (self x y)
        (begin 
          0))
          // (set (prop self x) x)
          // (set (prop self y) y)))

      (def calc (self) 
        0)
        // (+ (prop self x) (prop self y)))
    )
  )

  (var p (new Point 10 20))

  )";

  // compiler instance
  EvaLLVM vm;

  // generate LLVM IR
  vm.exec(program);

  return 0;
}