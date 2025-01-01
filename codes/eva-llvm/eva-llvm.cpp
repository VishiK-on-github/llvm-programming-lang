/*
  Eva LLVM executable
*/

#include <string>
#include "./src/EvaLLVM.h"

int main(int argc, char const *argv[])
{
  // program to execute
  std::string program = R"(

  (def square (x) (* x x))
  (printf "square: %d\n\n" (square 2))

  )";

  // compiler instance
  EvaLLVM vm;

  // generate LLVM IR
  vm.exec(program);

  return 0;
}