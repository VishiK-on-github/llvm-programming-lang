/*
  Eva LLVM executable
*/

#include <string>
#include "./src/EvaLLVM.h"

int main(int argc, char const *argv[])
{
  // program to execute
  std::string program = R"(

  (var x 10)

  (while (> x 0)
    (begin
      (set x (- x 1))
      (printf "%d " x)
    )
  )

  )";

  // compiler instance
  EvaLLVM vm;

  // generate LLVM IR
  vm.exec(program);

  return 0;
}