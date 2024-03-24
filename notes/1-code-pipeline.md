#### AOT Compiler

The steps in execution of code from programming language is as follows:

1. code
2. tokenizer: takes code breaks it into stream of tokens
3. parser: takes tokes converts them into AST
4. code generator: takes AST and generates multiple IRs. The are translated to
   different target architectures (x64 / x86) and sent to the CPU which figures
   out the runtime semantics

- Frontend: are steps which start from parsing code to generating ASTs
- Backend: are steps which transform AST to target architecture code

- LLVM IR: takes AST -> LLVM IR
- LLVM: takes LLVM IR -> wasm, x86, x64
