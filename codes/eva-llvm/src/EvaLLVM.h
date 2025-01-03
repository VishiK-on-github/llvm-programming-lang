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

// Binary operation macro
#define GEN_BINARY_OP(Op, varName)            \
  do {                                        \
      auto op1 = gen(exp.list[1], env);       \
      auto op2 = gen(exp.list[2], env);       \
      return builder->Op(op1, op2, varName);  \
  } while(false)

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
            if (auto localVar = llvm::dyn_cast<llvm::AllocaInst>(value)) {
              return builder->CreateLoad(localVar->getAllocatedType(), localVar, varName.c_str());
            }

            // global variables
            else if (auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
              return builder->CreateLoad(globalVar->getInitializer()->getType(),
                                        globalVar, varName.c_str());
            }

            // functions
            else {
              return value;
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
            
            // math binary ops
            if (op == "+") {
              GEN_BINARY_OP(CreateAdd, "tmpadd");
            } else if (op == "-") {
              GEN_BINARY_OP(CreateSub, "tmpsub");
            } else if (op == "*") {
              GEN_BINARY_OP(CreateMul, "tmpmul");
            } else if (op == "/") {
              GEN_BINARY_OP(CreateSDiv, "tmpdiv");
            }

            // comparison ops: unsigned
            else if (op == ">") {
              GEN_BINARY_OP(CreateICmpUGT, "tmpcmp");
            } else if (op == "<") {
              GEN_BINARY_OP(CreateICmpULT, "tmpcmp");
            } else if (op == "==") {
              GEN_BINARY_OP(CreateICmpEQ, "tmpcmp");
            } else if (op == "!=") {
              GEN_BINARY_OP(CreateICmpNE, "tmpcmp");
            } else if (op == ">=") {
              GEN_BINARY_OP(CreateICmpUGE, "tmpcmp");
            } else if (op == "<=") {
              GEN_BINARY_OP(CreateICmpULE, "tmpcmp");
            }

            // branch instruction
            // if (<condition> <then> <else>)
            else if (op == "if") {
              auto condition = gen(exp.list[1], env);

              // blocks
              auto thenBlock = createBB("then", fn);

              // else, ifend blocks appended later
              // to handle nested if-expressions
              auto elseBlock = createBB("else");
              auto ifEndBlock = createBB("ifend");

              // condition branch
              builder->CreateCondBr(condition, thenBlock, elseBlock);

              // then branch
              builder->SetInsertPoint(thenBlock);
              auto thenRes = gen(exp.list[2], env);
              builder->CreateBr(ifEndBlock);

              // restoring the block to handle nested if-expression
              // it is needed for the phi instruction
              thenBlock = builder->GetInsertBlock();

              // else branch
              // append the block to the function now
              fn->getBasicBlockList().push_back(elseBlock);
              builder->SetInsertPoint(elseBlock);
              auto elseRes = gen(exp.list[3], env);
              builder->CreateBr(ifEndBlock);
              
              // restore the block for phi instruction
              elseBlock = builder->GetInsertBlock();

              // if-end block
              fn->getBasicBlockList().push_back(ifEndBlock);
              builder->SetInsertPoint(ifEndBlock);

              // result of if expression
              auto phi = builder->CreatePHI(thenRes->getType(), 2, "tmpif");

              phi->addIncoming(thenRes, thenBlock);
              phi->addIncoming(elseRes, elseBlock);

              return phi;
            }

            // while loop
            else if (op == "while") {
              // condition
              auto condBlock = createBB("cond", fn);
              builder->CreateBr(condBlock);

              // body
              auto bodyBlock = createBB("body");
              auto loopEndBlock = createBB("loopend");

              // compile the condition
              builder->SetInsertPoint(condBlock);
              auto cond = gen(exp.list[1], env);

              // condition branch
              builder->CreateCondBr(cond, bodyBlock, loopEndBlock);

              // body
              fn->getBasicBlockList().push_back(bodyBlock);
              builder->SetInsertPoint(bodyBlock);
              gen(exp.list[2], env);
              builder->CreateBr(condBlock);

              fn->getBasicBlockList().push_back(loopEndBlock);
              builder->SetInsertPoint(loopEndBlock);

              return builder->getInt32(0);
            }

            // function declaration
            // (def <name> <param> <body>)
            if (op == "def") {
              return compileFunction(exp, /* name */ exp.list[1].string, env);
            }
            
            // variable declaration: (var a (+ b 1))
            // typed version: (var (x number) 10)
            // Note: locals are allocated on the stack
            if (op == "var") {
              auto varNameDec = exp.list[1];
              auto varName = extractVarName(varNameDec);

              // init
              auto init = gen(exp.list[2], env);

              // variable type
              auto varType = extractVarType(varNameDec);

              // variable
              auto varBinding = allocVar(varName, varType, env);

              // setting variable value
              return builder->CreateStore(init, varBinding);
            }

            // set: is used to update the value of a variable
            else if (op == "set") {
              // value
              auto value = gen(exp.list[2], env);

              // get the name of the variable to be updated
              auto varName = exp.list[1].string;

              // lookup where in the permissible env scope chain can
              // we find the variable to be updated with
              auto varBinding = env->lookup(varName);

              // set the value
              builder->CreateStore(value, varBinding);

              return value;
            }

            // blocks
            // starts with the begin keyword (begin <block>)
            else if (op == "begin") {
              auto blockEnv = std::make_shared<Environment>(
                std::map<std::string, llvm::Value*>{}, env);

              llvm::Value* blockRes;

              for (auto i = 1; i < exp.list.size(); i++) {
                blockRes = gen(exp.list[i], blockEnv);
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

            else if (op == "class") {
              // TODO: implement classes
            }

            // function calls
            else {
              auto callable = gen(exp.list[0], env);

              std::vector<llvm::Value*> args{};

              for (auto i = 1; i < exp.list.size(); i++) {
                args.push_back(gen(exp.list[i], env));
              }

              auto fn = (llvm::Function*)callable;

              return builder->CreateCall(fn, args);
            }
          }
      }

      // unreachable
      return builder->getInt32(0);
    }

    /*
      Extract the variable/param name from an 
      assignment expression.
      (var x 10) -> x
      (var (x number) 10) -> x
    */
    std::string extractVarName(const Exp& exp) {
      return exp.type == ExpType::LIST ? exp.list[0].string : exp.string;
    }

    /*
      Extracts the variable/param type. i32 is default.
      x -> i32
      (x number) -> number
    */
    llvm::Type* extractVarType(const Exp& exp) {
      return exp.type == ExpType::LIST ? getTypeFromString(exp.list[1].string) : builder->getInt32Ty();
    }

    /*
      Infer the LLVM type from the string representation
    */
    llvm::Type* getTypeFromString(const std::string& type_) {
      // number -> i32
      if (type_ == "number") {
        return builder->getInt32Ty();
      }

      // string ->i8*
      if (type_ == "string") {
        return builder->getInt8Ty()->getPointerTo();
      }

      // default:
      return builder->getInt32Ty();
    }

    /*
      If a function has a return type defined or not
    */
    bool hasReturnType(const Exp& fnExp) {
      return fnExp.list[3].type == ExpType::SYMBOL && fnExp.list[3].string == "->";
    }

    /*
      TODO: add description
    */
    llvm::FunctionType* extractFunctionType(const Exp& fnExp) {
      auto params = fnExp.list[2];

      // return type
      auto returnType = hasReturnType(fnExp) ? getTypeFromString(fnExp.list[4].string) : builder->getInt32Ty();

      // param types
      std::vector<llvm::Type*> paramTypes{};

      for (auto& param : params.list) {
        auto paramTy = extractVarType(param);
        paramTypes.push_back(paramTy);
      }

      return llvm::FunctionType::get(returnType, paramTypes, /* varargs */ false);
    }

    /*
      Compiles a function.
      Untyped example: (def square (x) (* x x)) - i32 by default
      Typed example: (def square ((a number)) -> number (* x x))
    */
    llvm::Value* compileFunction(const Exp& fnExp, std::string fnName, Env env) {
      auto params = fnExp.list[2];
      auto body = hasReturnType(fnExp) ? fnExp.list[5] : fnExp.list[3];

      // save current function.
      auto prevFn = fn;
      auto prevBlock = builder->GetInsertBlock();

      // override function to compile the body.
      auto newFn = createFunction(fnName, extractFunctionType(fnExp), env);
      fn = newFn;

      // set param names
      auto idx = 0;

      auto fnEnv = std::make_shared<Environment>(
        std::map<std::string, llvm::Value*>{}, env);

      for (auto& arg : fn->args()) {
        auto param = params.list[idx++];
        auto argName = extractVarName(param);

        arg.setName(argName);

        // allocate the local variable as per 
        // the argument to make mutable arguments
        auto argBinding = allocVar(argName, arg.getType(), fnEnv);
        builder->CreateStore(&arg, argBinding);
      }

      builder->CreateRet(gen(body, fnEnv));

      // restore previous function after compiling
      builder->SetInsertPoint(prevBlock);
      fn = prevFn;

      return newFn;
    }

    /*
      Allocating a local variable on the stack.
      results in alloca instruction.
    */
    llvm::Value* allocVar(const std::string& name, llvm::Type* type_, Env env) {
      varsBuilder->SetInsertPoint(&fn->getEntryBlock());

      auto varAlloc = varsBuilder->CreateAlloca(type_, 0, name.c_str());

      // add to the env
      env->define(name, varAlloc);

      return varAlloc;
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

      // builder for variables.
      varsBuilder = std::make_unique<llvm::IRBuilder<>>(*ctx);
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
      TODO: add more info
    */
    std::unique_ptr<llvm::IRBuilder<>> varsBuilder;

    /*
      TODO: add IR builder info
    */
    std::unique_ptr<llvm::IRBuilder<>> builder;

};

#endif