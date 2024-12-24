Parts of LLVM infrastructure are as follows:

1. Module: it is a container for out functions, global variables, constants etc
2. Context: container for modules and metadata
3. IR Builder: APIs to generate IR instructions

Command to compile with LLVM:

clang++ -o output-filename `llvm-config --cxxflags --ldflags --system-libs --libs core` cpp-filename