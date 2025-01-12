# /bin/zsh
# compile main file.
clang++ -o ./bin/eva-llvm.o `llvm-config --cxxflags --ldflags --system-libs --libs core` eva-llvm.cpp

# run main executable
./bin/eva-llvm.o -f test.eva

# execute generated IR.
# lli ./out.ll

# optimize the output:
opt ./bin/out.ll -O3 -S -o ./bin/out-opt.ll

# compile ./out.ll with GC:
# to install GC_malloc: bre install libgc
clang++ -O3 -I/opt/homebrew/Cellar/gc/ ./bin/out-opt.ll /opt/homebrew/Cellar/bdw-gc/8.2.8/lib/libgc.a -o ./bin/out.o

# run compiled program
./bin/out.o

# print result
echo $?

printf "\n"