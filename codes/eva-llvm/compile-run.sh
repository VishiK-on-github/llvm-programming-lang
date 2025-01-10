# /bin/zsh
# compile main file.
clang++ -o eva-llvm.o `llvm-config --cxxflags --ldflags --system-libs --libs core` eva-llvm.cpp

# run main executable
./eva-llvm.o

# execute generated IR.
# lli ./out.ll

# optimize the output:
opt ./out.ll -O3 -S -o ./out-opt.ll

# compile ./out.ll with GC:
# to install GC_malloc: bre install libgc
clang++ -O3 -I/opt/homebrew/Cellar/gc/ ./out.ll /opt/homebrew/Cellar/bdw-gc/8.2.8/lib/libgc.a -o ./out.o

# run compiled program
./out.o

# print result
echo $?

printf "\n"