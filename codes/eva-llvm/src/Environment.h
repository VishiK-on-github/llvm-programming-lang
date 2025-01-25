/*
    Environment class equivalent to Symbol table in LLVM
*/
#ifndef Environment_h
#define Environment_h

#include <map>
#include <memory>
#include <string>

#include "llvm/IR/Value.h"
#include "./Logger.h"

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment(std::map<std::string, llvm::Value*> record,
                std::shared_ptr<Environment> parent): record_(record), parent_(parent) {}

    // creating variable with given name and value
    llvm::Value* define(const std::string& name, llvm::Value* value) {
        record_[name] = value;
        return value;
    }

    // returning a defined variable, if not throw error
    llvm::Value* lookup(const std::string& name) {
        return resolve(name)->record_[name];
    }

private:
    // do scope resolution till the parent envs to get defined vars
    std::shared_ptr<Environment> resolve(const std::string& name) {
        if (record_.count(name) != 0) {
            return shared_from_this();
        }

        if (parent_ == nullptr) {
            DIE << "Variable \"" << name << "\" is not defined." << std::endl;
        }

        return parent_->resolve(name);
    }

    // the actual storage
    std::map<std::string, llvm::Value*> record_;

    // link to parent environments
    std::shared_ptr<Environment> parent_; // link to parent
};

#endif