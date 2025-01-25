/**
 * Eva to LLVM compiler
 */
#ifndef EvaLLVM_h
#define EvaLLVM_h

#include <string>
#include <iostream>
#include <regex>
#include <map>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "./parser/EvaParser.h"
#include "./Environment.h"
#include "./Logger.h"

using syntax::EvaParser;

using Env = std::shared_ptr<Environment>;

struct ClassInfo {
  llvm::StructType* cls; // the current class
  llvm::StructType* parent; // the parent class incase the base class inherits another class
  std::map<std::string, llvm::Type*> fieldsMap; // the fields described in the current class
  std::map<std::string, llvm::Function*> methodsMap; // the methods described in the current class
};

// index of the vTable in the class fields.
static const size_t VTABLE_INDEX = 0;

/*
  each class will have a set of reserved fields
  at the beginning of its layout. currently only
  vTable is used to resolve the methods.
*/
static const size_t RESERVED_FIELDS_COUNT = 1;

// Binary operation macro
// builder->Op exposes arithmetic, memory
// and logical operations through the IR Builder
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
      setupTargetTriple();
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
      saveModuleToFile("./bin/out.ll");
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

      // setting version global variable
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

        case ExpType::SYMBOL: {
          // checking if expression is a boolean or not
          if (exp.string == "true" || exp.string == "false") {
            return builder->getInt1(exp.string == "true" ? true : false);
          } 
          else {
            // variables
            auto varName = exp.string;
            auto value = env->lookup(varName);

            // local variables
            // we check if "value" is of type llvm::AllocaInst i.e. is it allocated on the stack
            // if yes then we get pointer to llvm::AllocaInst type else a null pointer
            if (auto localVar = llvm::dyn_cast<llvm::AllocaInst>(value)) {
              // if casting is successful, we try to create load instruction
              // to keep the variable on the stack
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
        }
        // numbers
        case ExpType::NUMBER:
          // we create instruction to get an integer of i32 type
          // where the value is exp.number
          return builder->getInt32(exp.number);

        // strings
        case ExpType::STRING: {
          auto re = std::regex("\\\\n");
          auto str = std::regex_replace(exp.string, re, "\n");

          // CreateGlobalStringPtr: is used to create a string constant
          // in the global scope of the program
          return builder->CreateGlobalStringPtr(str);
        }

        // lists
        case ExpType::LIST:
          auto tag = exp.list[0];

          // if we have + - * /
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
              
              // we dont want to re initialize values during the class declaration
              // as normal variables or overwrites due to var keyword.
              // this is a special case for class fields, which are already defined
              // during the class alocation.
              if (cls != nullptr) {
                return builder->getInt32(0);
              }
              
              // getting name from the declaration
              auto varNameDec = exp.list[1];
              auto varName = extractVarName(varNameDec);

              // special case for new keyword as it allocates a new variable.
              if (isNew(exp.list[2])) {
                auto instance = createInstance(exp.list[2], env, varName);
                return env->define(varName, instance);
              }

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

              // properties
              if (isProp(exp.list[1])) {
                auto instance = gen(exp.list[1].list[1], env); // we get instance of the class
                auto fieldName = exp.list[1].list[2].string; // we get field within the class whose value is to be modified
                auto ptrName = std::string("p") + fieldName; // we give a name to the field inside the class

                auto cls = (llvm::StructType*)(instance->getType()->getContainedType(0)); // we get a pointer to the instance

                // we get the offset factor from the start of the struct location
                auto fieldIdx = getFieldIndex(cls, fieldName);
                
                // we offset from the starting location of the struct 
                // pointer to point to where the field is stored on the heap
                auto address = builder->CreateStructGEP(cls, instance, fieldIdx, ptrName);

                // we store the actual value using value
                // and address contains a pointer to where the value must be stored
                builder->CreateStore(value, address);

                return value;
              }
              
              // variables
              else {
                // get the name of the variable to be updated
                auto varName = exp.list[1].string;

                // lookup where in the permissible env scope chain can
                // we find the variable to be updated with
                auto varBinding = env->lookup(varName);

                // set the value
                builder->CreateStore(value, varBinding);

                return value;
              }
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

              // gather the args
              for (auto i = 1; i < exp.list.size(); i++) {
                args.push_back(gen(exp.list[i], env));
              }

              // invoke the printf function with the array of collected args
              return builder->CreateCall(printfFn, args);
            }

            // class declaration
            // Example:
            // (class A <super> <body>)
            else if (op == "class") {
              auto name = exp.list[1].string;

              // getting the parent class name.
              // if base class inherits parent class
              auto parent = exp.list[2].string == "null" ? nullptr : getClassByName(exp.list[2].string);

              // compiling the class.
              cls = llvm::StructType::create(*ctx, name);

              if (parent != nullptr) {
                inheritClass(cls, parent);
              } else {
                // allocate info for new class.
                classMap_[name] = {
                  /* class */ cls,
                  /* parent */ parent,
                  /* fields */ {},
                  /* methods */ {}};
              }

              // add fields and methods in the class into class info
              buildClassInfo(cls, exp, env);

              // compile the body
              gen(exp.list[3], env);

              // reset the class after compiling, so normal fns
              // dont pick the class name prefix.
              cls = nullptr;

              return builder->getInt32(0);
            }

            // object instantiation
            // (new <class> <args)
            else if (op == "new") {
              return createInstance(exp, env, "");
            }

            // property of a class access
            // (prop <instance> <name>)
            else if (op == "prop") {
              // instance
              auto instance = gen(exp.list[1], env);
              auto fieldName = exp.list[2].string;
              auto ptrName = std::string("p") + fieldName;

              // instance->getType(): gives us Point*
              // instance->getType()->getContainedType(): 
              // get us the dereferenced pointer i.e. Point.
              auto cls = (llvm::StructType*)instance->getType()->getContainedType(0);

              auto fieldIdx = getFieldIndex(cls, fieldName);

              auto address = builder->CreateStructGEP(cls, instance, fieldIdx, ptrName);

              return builder->CreateLoad(cls->getElementType(fieldIdx), address, fieldName);
            }

            // method access
            // (method <instance> <name>) or (method (super <class>) <name>)
            else if (op == "method") {
              auto methodName = exp.list[2].string;

              llvm::StructType* cls;
              llvm::Value* vTable;
              llvm::StructType* vTableTy;

              // (method (super <class>) <name>)
              if (isSuper(exp.list[1])) {
                auto className = exp.list[1].list[1].string; // get class name
                cls = classMap_[className].parent; // get the parent class of current class from classMap
                auto parentName = std::string{cls->getName().data()}; // get parent classes name
                vTable = module->getNamedGlobal(parentName + "_vTable"); // get the vTable associated with the parent class
                vTableTy = llvm::StructType::getTypeByName(*ctx, parentName + "_vTable"); // used to get layout of fn pointers of the parent class
              }

              else {
                // instance
                auto instance = gen(exp.list[1], env);

                // get struct pointer to the class
                cls = (llvm::StructType*)(instance->getType()->getContainedType(0));

                // load vTable
                auto vTableAddr = builder->CreateStructGEP(cls, instance, VTABLE_INDEX);

                vTable = builder->CreateLoad(cls->getElementType(VTABLE_INDEX), vTableAddr, "vt");

                vTableTy = (llvm::StructType*)(vTable->getType()->getContainedType(0));
              }

              // get offset from vTable start to our desired method name
              auto methodIdx = getMethodIndex(cls, methodName);

              // get the type of the method from our vTable
              auto methodTy = (llvm::FunctionType*)vTableTy->getElementType(methodIdx);

              // get the address of th method using GEP instruction
              auto methodAddr = builder->CreateStructGEP(vTableTy, vTable, methodIdx);
              
              return builder->CreateLoad(methodTy, methodAddr);
            }

            // function calls
            else {
              auto callable = gen(exp.list[0], env);

              // raw function or a functor (callable class)
              auto callableTy = callable->getType()->getContainedType(0);

              std::vector<llvm::Value*> args{};
              auto argIdx = 0;

              if (callableTy->isStructTy()) {
                auto cls = (llvm::StructType*)callableTy;

                std::string className{cls->getName().data()};

                // push the functor as the fust `self` arg.
                args.push_back(callable);
                argIdx++;

                // TODO: support inheritance - load method from the vTable
                callable = module->getFunction(className + "___call__");
              }

              auto fn = (llvm::Function*)callable;

              for (auto i = 1; i < exp.list.size(); i++, argIdx++) {
                auto argValue = gen(exp.list[i], env);

                auto paramTy = fn->getArg(argIdx)->getType();
                auto bitCastArgVal = builder->CreateBitCast(argValue, paramTy);

                args.push_back(bitCastArgVal);
              }

              return builder->CreateCall(fn, args);
            }
          }

          // method calls.
          // ((method p getX) 2)
          else {
            auto loadedMethod = (llvm::LoadInst*)gen(exp.list[0], env);

            auto fnTy = (llvm::FunctionType*)(loadedMethod->getPointerOperand()->getType()->getContainedType(0)->getContainedType(0));

            std::vector<llvm::Value*> args{};

            for (auto i = 1; i < exp.list.size(); i++) {
              auto argValue = gen(exp.list[i], env);

              // we need to cast to param type to support sub-classes.
              // we should be able to pass Point3D instance for the type.
              // of the parent class Point:
              auto paramTy = fnTy->getParamType(i - 1);

              if (argValue->getType() != paramTy) {
                auto bitCastArgVal = builder->CreateBitCast(argValue, paramTy);
                args.push_back(bitCastArgVal);
              } else {
                args.push_back(argValue);
              }
            }

            return builder->CreateCall(fnTy, loadedMethod, args);
          }
      }

      // unreachable
      return builder->getInt32(0);
    }

    /*
      Returns field index.
    */
    size_t getFieldIndex(llvm::StructType* cls, const std::string& fieldName) {
      auto fields = &classMap_[cls->getName().data()].fieldsMap;
      auto it = fields->find(fieldName);
      return std::distance(fields->begin(), it) + RESERVED_FIELDS_COUNT;
    }

    /*
      Returns method index.
    */
    size_t getMethodIndex(llvm::StructType* cls, const std::string& methodName) {
      auto methods = &classMap_[cls->getName().data()].methodsMap;
      auto it = methods->find(methodName);
      return std::distance(methods->begin(), it);
    }

    /*
      Creates an instance of a class.
    */
    llvm::Value* createInstance(const Exp& exp, Env env, const std::string& name) {
      auto className = exp.list[1].string;
      auto cls = getClassByName(className);

      if (cls == nullptr) {
        DIE << "[EvaLLVM]: Unknown class " << cls;
      }

      // currently instance allocation is on stack.
      // TODO: heap allocation
      // auto instance = name.empty() ? 
      // builder->CreateAlloca(cls) : builder->CreateAlloca(cls, 0, name);

      // we are going to use malloc here as an external function
      // to reuse memory allocation a garbage collection
      auto instance = mallocInstance(cls, name);

      // call constructor after instance has been created
      auto ctor = module->getFunction(className + "_constructor");

      std::vector<llvm::Value*> args{instance};

      for (auto i = 2; i < exp.list.size(); i++) {
        args.push_back(gen(exp.list[i], env));
      }

      builder->CreateCall(ctor, args);

      return instance;
    }

    /*
      Allocates an object of given class on the heap.
    */
    llvm::Value* mallocInstance(llvm::StructType* cls, const std::string& name) {
      auto typeSize = builder->getInt64(getTypeSize(cls));
      
      // will give us a void*
      auto mallocPtr = builder->CreateCall(module->getFunction("GC_malloc"), typeSize, name);

      // need to convert pointer to our class pointer type
      // void* -> Point*
      auto instance = builder->CreatePointerCast(mallocPtr, cls->getPointerTo());

      // install the vTable to lookup methods.
      std::string className{cls->getName().data()};
      auto vTableName = className + "_vTable";
      auto vTableAddr = builder->CreateStructGEP(cls, instance, VTABLE_INDEX);
      auto vTable = module->getNamedGlobal(vTableName);
      builder->CreateStore(vTable, vTableAddr);

      return instance;
    }

    /*
      Returns size of a type in bytes.
    */
    size_t getTypeSize(llvm::Type* type_) {
      return module->getDataLayout().getTypeAllocSize(type_);
    }

    /*
      Inherits parent class fields.
    */
    void inheritClass(llvm::StructType* cls, llvm::StructType* parent) {
      auto parentClassInfo = &classMap_[parent->getName().data()];

      // inherit the field and method names.
      classMap_[cls->getName().data()] = {
        /* class */ cls,
        /* parent */ parent,
        /* fields */ parentClassInfo->fieldsMap,
        /* methods */ parentClassInfo->methodsMap};

    }

    /*
      Extract fields and methods from a class expression.
    */
    void buildClassInfo(llvm::StructType* cls, const Exp& clsExp, Env env) {
      auto className = clsExp.list[1].string;
      auto classInfo = &classMap_[className];

      // body block
      auto body = clsExp.list[3];
      
      // iterate over the statements in the body block
      for (auto i = 1; i < body.list.size(); i++) {
        auto exp = body.list[i];

        // check if it is a variable assignment statement
        if (isVar(exp)) {

          auto varNameDecl = exp.list[1];

          auto fieldName = extractVarName(varNameDecl); // get name of the variable
          auto fieldTy = extractVarType(varNameDecl); // get variable type

          classInfo->fieldsMap[fieldName] = fieldTy; // add the field into the class's field map
        } 
        
        // check if it is a method declaration
        else if (isDef(exp)) {
          auto methodName = exp.list[1].string; // get the method name
          auto fnName = className + "_" + methodName; // to maintain uniqueness across classes use cls name + fn name

          // add the method definition into the classes method map
          classInfo->methodsMap[methodName] = createFunctionProto(fnName, extractFunctionType(exp), env);
        }
      }
      
      // create fields.
      buildClassBody(cls);
    }

    /*
      Builds class body using class info.
    */
    void buildClassBody(llvm::StructType* cls) {
      std::string className{cls->getName().data()};

      auto classInfo = &classMap_[className];

      // allocate vTable to set its type in the body.
      // the table itself is populated later in buildVTable.
      auto vTableName = className + "_vTable";
      auto vTableTy = llvm::StructType::create(*ctx, vTableName);

      auto clsFields = std::vector<llvm::Type*>{
        // first element is always the vTable.
        vTableTy->getPointerTo(),
      };

      // field types
      for (const auto& fieldInfo : classInfo->fieldsMap) {
        clsFields.push_back(fieldInfo.second);
      }

      cls->setBody(clsFields, /* packed */ false);

      // methods:
      buildVTable(cls);
    }

    /*
      create a vTable per class.
      vTable stores method references 
      to support inheritance and method overloading.
    */
    void buildVTable(llvm::StructType* cls) {
      std::string className{cls->getName().data()}; // get the class name
      auto vTableName = className + "_vTable"; // set the vTable name for the class

      // the vTable should already exists.
      auto vTableTy = llvm::StructType::getTypeByName(*ctx, vTableName);

      // create vTable methods and method types
      std::vector<llvm::Constant*> vTableMethods;
      std::vector<llvm::Type*> vTableMethodTys;

      // iterate over the methods in a class and collect methods and method types
      for (auto& methodInfo : classMap_[className].methodsMap) {
        auto method = methodInfo.second;
        vTableMethods.push_back(method);
        vTableMethodTys.push_back(method->getType());
      }

      vTableTy->setBody(vTableMethodTys);

      auto vTableValue = llvm::ConstantStruct::get(vTableTy, vTableMethods);
      createGlobalVar(vTableName, vTableValue);
    }


    /*
      Tagged list
    */
    bool isTaggedList(const Exp& exp, const std::string& tag) {
      return exp.type == ExpType::LIST && exp.list[0].type == ExpType::SYMBOL && 
              exp.list[0].string == tag;
    }

    /*
      to check if list is of variable assignment type.
      (var ...)
    */
    bool isVar(const Exp& exp) { return isTaggedList(exp, "var"); }

    /*
      to check if list is of variable assignment type.
      (def ...)
    */
    bool isDef(const Exp& exp) { return isTaggedList(exp, "def"); }

    /*
      to check if list is of object instantiation type.
      (new ...)
    */
    bool isNew(const Exp& exp) { return isTaggedList(exp, "new"); }

    /*
      to check if the list is of object
      (prop ...)
    */
    bool isProp(const Exp& exp) { return isTaggedList(exp, "prop"); }

    /*
      (super ...)
    */
    bool isSuper(const Exp& exp) { return isTaggedList(exp, "super"); }

    /*
      Get a type struct using name.
    */
    llvm::StructType* getClassByName(const std::string& name) {
      return llvm::StructType::getTypeByName(*ctx, name);
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

      // class
      return classMap_[type_].cls->getPointerTo();
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

      // collect the args for a fn
      for (auto& param : params.list) {
        auto paramName = extractVarName(param);
        auto paramTy = extractVarType(param);

        // if self add a pointer to the class itself
        paramTypes.push_back(
            paramName == "self" ? (llvm::Type*)cls->getPointerTo() : paramTy);
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

      // class methods
      if (cls != nullptr) {
        fnName = std::string(cls->getName().data()) + "_" + fnName;
      }

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

      // void* malloc(size_t size), void* GC_malloc(size_t size)
      // size_t is i64
      module->getOrInsertFunction("GC_malloc", llvm::FunctionType::get(bytePtrTy, builder->getInt64Ty(), 
                                                                                /* vararg */ false));
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
      Setup the target triple.
    */
    void setupTargetTriple() {
      module->setTargetTriple("arm64-apple-macosx14.0.0");
    }

    /*
      Eva Parser
    */
    std::unique_ptr<EvaParser> parser;

    /*
      currently compiling class
    */
    llvm::StructType* cls = nullptr;

    /*
      Class information
    */
    std::map<std::string, ClassInfo> classMap_;

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