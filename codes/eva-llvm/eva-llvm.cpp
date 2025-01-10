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
  // closures

  (class Cell null
    (begin

      (var value 0)

      (def constructor (self value) -> Cell
        (begin
          (set (prop self value) value)
          self))
      
    ))

    (class SetFunctor null
      (begin

        (var (cell Cell) 0)

        (def constructor (self (cell Cell)) -> SetFunctor
          (begin
            (set (prop self cell) cell)
            self))

        (def __call__ (self value)
          (begin
            (set (prop (prop self cell) value) value)
            value))
      )
    )

    (var n (new Cell 10))

    (var setN (new SetFunctor n))
    (var getN (new GetFunctor n))

    (printf "getN.__call__ result = %d\n" (getN))
    (printf "setN.__call__ result = %d\n" (setN 20))
    (printf "getN.__call__ result = %d\n" (getN))

  )";

  // compiler instance
  EvaLLVM vm;

  // generate LLVM IR
  vm.exec(program);

  return 0;
}