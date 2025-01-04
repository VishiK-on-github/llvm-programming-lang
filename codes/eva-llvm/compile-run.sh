# /bin/zsh
# compile main file.
clang++ -o eva-llvm.o `llvm-config --cxxflags --ldflags --system-libs --libs core` eva-llvm.cpp

# run main executable
./eva-llvm.o

# execute generated IR.
# lli ./out.ll

# compile ./out.ll with GC:
# to install GC_malloc: bre install libgc
clang++ -O3 -I/opt/homebrew/Cellar/gc/ ./out.ll /opt/homebrew/Cellar/bdw-gc/8.2.8/lib/libgc.a -o ./out.o

# run compiled program
./out

# print result
echo $?

printf "\n"