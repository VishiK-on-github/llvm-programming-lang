/*
  Eva LLVM executable
*/

#include <string>
#include <iostream>
#include <fstream>

#include "./src/EvaLLVM.h"

void printHelp() {
  std::cout << "\nUsage: eva-llvm [option]\n\n"
            << "Options:\n"
            << "    -e, --expression  Expression to parse\n"
            << "    -f, --file        File to parse\n\n";
}

int main(int argc, char const *argv[])
{ 

  if (argc != 3) {
    printHelp();
    return 0;
  }

  // expression mode
  std::string mode = argv[1];

  // program to execute
  std::string program;

  // simple expression
  if (mode == "-e") {
    program = argv[2];
  }

  /*
    eva file
  */
  else if (mode == "-f") {
    std::ifstream programFile(argv[2]);
    std::stringstream buffer;
    buffer << programFile.rdbuf() << "\n";

    // program
    program = buffer.str();
  }

  // compiler instance
  EvaLLVM vm;

  // generate LLVM IR
  vm.exec(program);

  return 0;
}