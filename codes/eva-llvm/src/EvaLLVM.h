/**
 * Eva to LLVM compiler
 */
#ifndef EvaLLVM_h
#define EvaLLVM_h

#include <string>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

class EvaLLVM
{
public:
  EvaLLVM() { moduleInit(); }

  // executes a program
  void exec(const std::string &program)
  {
    // parse the program

    // compile to LLVM IR

    // printing the generated code
    module->print(llvm::outs(), nullptr);

    // save module IR to file
    saveModuleToFile("./out.ll")
  }

private:
  // save the IR to a file
  void saveModuleToFile(const std::string &fileName)
  {
    std::error_code errorCode;
    llvm::raw_fd_ostream outLL(fileName, errorCode);

    module->print(outLL, nullptr);
  }
  // initialize the module
  void moduleInit()
  {
    // open new context and module
    ctx = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("EvaLLVM", *ctx);

    // create a new builder for the module
    builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
  }

  std::unique_ptr<llvm::LLVMContext> ctx;

  std::unique_ptr<llvm::LLVMModule> module;

  std::unique_ptr<llvm::IRBuilder> builder;
};

#endif