/**
 * Eva to LLVM compiler
 */
#ifndef EvaLLVM_h
#define EvaLLVM_h

#include <string>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

class EvaLLVM {
  public:
    EvaLLVM() { moduleInit(); }

    /*
    Executes a program
    */
    void exec(const std::string& program) {
      // 1. parsing the program
      // auto ast = parser->parser(program);

      // 2. compile to LLVM IR
      // compile(ast);

      // printing generated code.
      module->print(llvm::outs(), nullptr);

      // 3. save IR to file.
      saveModuleToFile("./out.ll");
    }

  private:
    /*
      Module init
    */
    void moduleInit() {
      // open a new context and module.
      ctx = std::make_unique<llvm::LLVMContext>();
      module = std::make_unique<llvm::Module>("EvaLLVM", *ctx);

      // make new builder for the module.
      builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
    }

    /*
      save the IR to the file.
    */
    void saveModuleToFile(const std::string& fileName) {
      std::error_code errorCode;
      llvm::raw_fd_ostream outLL(fileName, errorCode);
      module->print(outLL, nullptr);
    }
    
    /*
      Global LLVM context.
      manages core "global" data of LLVM core infra
      including type and constant unique tables.
    */
    std::unique_ptr<llvm::LLVMContext> ctx;

    /*
      TODO: add module info
    */
    std::unique_ptr<llvm::Module> module;

    /*
      TODO: add IR builder info
    */
    std::unique_ptr<llvm::IRBuilder<>> builder;

};

#endif