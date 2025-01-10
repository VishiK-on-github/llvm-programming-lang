/*
  Eva LLVM executable
*/

#include <string>
#include "./src/EvaLLVM.h"

int main(int argc, char const *argv[])
{
  // program to execute
  std::string program = R"(

  // functors - callable objects

  (class Transformer null
    (begin

      (var factor 5)

      (def constructor (self factor) -> Transformer
        (begin
          (set (prop self factor) factor)
          self))

      (def __call__ (self v)
        (* (prop self factor) v))
    )
  )

  (var transform (new Transformer 5))
  (printf "(transform 10) = %d\n" (transform 10))

  (def calculate (x (modify Transformer))
    (modify x))

  (printf "(calculate 10 transform) = %d\n" (calculate 10 transform))

  )";

  // compiler instance
  EvaLLVM vm;

  // generate LLVM IR
  vm.exec(program);

  return 0;
}