/**
 * Eva to LLVM compiler
 */
#ifndef EvaLLVM_h
#define EvaLLVM_h

#include <string>
#include <iostream>
#include <regex>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "./parser/EvaParser.h"
#include "./Environment.h"

using syntax::EvaParser;

using Env = std::shared_ptr<Environment>;

class EvaLLVM {
  public:
    EvaLLVM() : parser(std::make_unique<EvaParser>()) { 
      moduleInit(); 
      setupExternFunction();
      setupGlobalEnvironment();
    }

    /*
    Executes a program
    */
    void exec(const std::string& program) {
      // 1. parsing the program
      auto ast = parser->parse("(begin " + program + ")");

      // 2. compile to LLVM IR
      compile(ast);

      // printing generated code.
      module->print(llvm::outs(), nullptr);

      std::cout << "\n";

      // 3. save IR to file.
      saveModuleToFile("./out.ll");
    }

  private:
    /*
      compiles an expression
    */
    void compile(const Exp& ast) {
      // 1. make main function
      fn = createFunction("main",
                          llvm::FunctionType::get(/* return type */ builder->getInt32Ty(),
                                                  /* vararg */ false), GlobalEnv);

      // global variables
      createGlobalVar("VERSION", builder->getInt32(1));

      // 2. compile the main body
      auto result = gen(ast, GlobalEnv);

      builder->CreateRet(builder->getInt32(0));
    }

    /*
      Main compile loop.
    */
    llvm::Value* gen(const Exp& exp, Env env) { 
      
      switch (exp.type) {
        // boolean
        case ExpType::SYMBOL:
          if (exp.string == "true" || exp.string == "false") {
            return builder->getInt1(exp.string == "true" ? true : false);
          } else {
            // variables
            auto varName = exp.string;
            auto value = env->lookup(varName);

            // TODO: local variables

            // global variables
            if (auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
              return builder->CreateLoad(globalVar->getInitializer()->getType(),
                                        globalVar, varName.c_str());
            }
          }

        // numbers
        case ExpType::NUMBER:
          return builder->getInt32(exp.number);

        // strings
        case ExpType::STRING: {
          auto re = std::regex("\\\\n");
          auto str = std::regex_replace(exp.string, re, "\n");

          return builder->CreateGlobalStringPtr(str);
        }

        // lists
        case ExpType::LIST:
          auto tag = exp.list[0];

          if (tag.type == ExpType::SYMBOL) {
            auto op = tag.string;

            if (op == "var") {
              auto varName = exp.list[1].string;

              auto init = gen(exp.list[2], env);

              return createGlobalVar(varName, (llvm::Constant*)init)->getInitializer();
            }

            // blocks
            else if (op == "begin") {
              llvm::Value* blockRes;

              for (auto i = 1; i < exp.list.size(); i++) {
                blockRes = gen(exp.list[i], env);
              }
              return blockRes;
            }

            // external functions
            else if (op == "printf") {
              auto printfFn = module->getFunction("printf");

              // args:
              std::vector<llvm::Value*> args{};

              for (auto i = 1; i < exp.list.size(); i++) {
                args.push_back(gen(exp.list[i], env));
              }

              return builder->CreateCall(printfFn, args);
            } 
          }
      }

      // unreachable
      return builder->getInt32(0);
    }

    /*
      Creates a global variable
    */
    llvm::GlobalVariable* createGlobalVar(const std::string& name, 
                                          llvm::Constant* init) {
      module->getOrInsertGlobal(name, init->getType());
      auto variable = module->getNamedGlobal(name);
      variable->setAlignment(llvm::MaybeAlign(4));
      variable->setConstant(false);
      variable->setInitializer(init);
      return variable;
    }

    /*
      define external functions (from some other libraries)
    */
    void setupExternFunction() {
      /*
        printf takes char* as input argument
        char* is alias for a byte which can
        be represented as i8*
      */
      auto bytePtrTy = builder->getInt8Ty()->getPointerTo();

      module->getOrInsertFunction("printf",
      llvm::FunctionType::get(
        /* return type */ builder->getInt32Ty(),
        /* format arg */ bytePtrTy,
        /* var arg */ true
      ));
    }

    /*
      creates a function prototype (defines function, not the body)
    */
    llvm::Function* createFunctionProto(const std::string& fnName, llvm::FunctionType* fnType, Env env) {
      auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fnName, *module);

      verifyFunction(*fn);

      // define in environment
      env->define(fnName, fn);

      return fn;
    }

    /*
      creates a function
    */
    llvm::Function* createFunction(const std::string& fnName, llvm::FunctionType* fnType, Env env) {
      // function prototype maybe defined
      auto fn = module->getFunction(fnName);

      // if not prototype, allocate the function
      if (fn == nullptr) {
        fn = createFunctionProto(fnName, fnType, env);
      }

      createFunctionalBlock(fn);
      return fn;
    }

    /*
      creates function block
    */
    void createFunctionalBlock(llvm::Function* fn) {
      auto entry = createBB("entry", fn);
      // TODO: why ?
      builder->SetInsertPoint(entry);
    }

    /*
      creates a basic block.
    */
    llvm::BasicBlock* createBB(std::string name, llvm::Function* fn = nullptr) {
      return llvm::BasicBlock::Create(*ctx, name, fn);
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
      Setup global environment
    */
    void setupGlobalEnvironment() {
      std::map<std::string, llvm::Value*> globalObject{
        {"VERSION", builder->getInt32(1)},
      };

      std::map<std::string, llvm::Value*> globalRec{};

      for (auto& entry: globalObject) {
        globalRec[entry.first] = 
          createGlobalVar(entry.first, (llvm::Constant*)entry.second);
      }

      GlobalEnv = std::make_shared<Environment>(globalRec, nullptr);
    }

    /*
      Eva Parser
    */
    std::unique_ptr<EvaParser> parser;

    /*
      Global Enviroment (symbol table)
    */
    std::shared_ptr<Environment> GlobalEnv;

    /*
      currently compiling functions.
    */
    llvm::Function* fn;
    
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