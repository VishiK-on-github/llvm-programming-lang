/*
 * Eva LLVM executable
 */

#include <string>
#include "EvaLLVM.h"

int main(int argc, char const *argv[])
{
  // program to execute
  std::string program = R"(
    42
  )";

  // compiler instance
  EvaLLVM vm;

  // generate LLVM IR
  vm.exec(program);

  return 0;
}