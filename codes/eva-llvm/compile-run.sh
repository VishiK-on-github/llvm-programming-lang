# /bin/zsh
# compile main file.
clang++ -o eva-llvm.o `llvm-config --cxxflags --ldflags --system-libs --libs core` eva-llvm.cpp

# run main executable
./eva-llvm.o

# execute generated IR.
lli ./out.ll

# print result
echo $?

printf "\n"