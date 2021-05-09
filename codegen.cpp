#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include "ast.hpp"

using namespace llvm;
using namespace AST;


std::unique_ptr<LLVMContext> ctx;
std::unique_ptr<IRBuilder<>> builder;
std::unique_ptr<Module> module;
std::unique_ptr<legacy::FunctionPassManager> fpm;

static std::map<std::string, Value*> namedValues;

Value* logErrorV(const char* str) {
    fprintf(stderr, "Error: %s\n", str);
    return nullptr;
}

Value* NumberExprAST::codegen() {
    return ConstantFP::get(*ctx, APFloat(val));
}

Value* VariableExprAST::codegen() {
    Value* v = namedValues[name];
    if (!v) {
        logErrorV("Unknown variable name");
    }
    return v;
}

Value* BinaryExprAST::codegen() {
    Value* l = lhs->codegen();
    Value* r = rhs->codegen();
    if (!l || !r)
        return nullptr;
    switch (op) {
        case '+':
            return builder->CreateFAdd(l, r, "addtmp");
        case '-':
            return builder->CreateFSub(l, r, "subtmp");
        case '*':
            return builder->CreateFMul(l, r, "multmp");
        case '<':
            l = builder->CreateFCmpULT(l, r, "cmptmp");
            return builder->CreateUIToFP(l, Type::getDoubleTy(*ctx), "bool");
        default:
            return logErrorV("Invalid binop");
    }
}

Value* CallExprAST::codegen() {
    Function* calleeFunc = module->getFunction(callee);
    if (!calleeFunc)
        return logErrorV("Unknown function referenced");
    if (calleeFunc->arg_size() != args.size())
        return logErrorV("Incorrect num of args");
    std::vector<Value*> argsV;
    for (auto &arg:args) {
        argsV.push_back(arg->codegen());
        if (!argsV.back())
            return nullptr;
    }
    return builder->CreateCall(calleeFunc, argsV, "calltmp");
}

Function* PrototypeAST::codegen() {
    std::vector<Type*> doubles(args.size(), Type::getDoubleTy(*ctx));
    FunctionType* ft = FunctionType::get(Type::getDoubleTy(*ctx), doubles, false);
    Function* f = Function::Create(ft, Function::ExternalLinkage, name, module.get());

    unsigned idx = 0;
    for (auto &arg:f->args()) {
        arg.setName(args[idx++]);
    }
    return f;
}

Function* FunctionAST::codegen() {
    Function* func = module->getFunction(proto->getName());

    if (!func)
        func = proto->codegen();
    if (!func)
        return nullptr;
    if (!func->empty())
        return (Function*) (logErrorV("Function cannot be redefined"));
    BasicBlock* bb = BasicBlock::Create(*ctx, "entry", func);
    builder->SetInsertPoint(bb);
    namedValues.clear();
    for (auto &arg: func->args())
        namedValues[arg.getName().str()] = &arg;
    if (Value* retval = body->codegen()) {
        builder->CreateRet(retval);
        verifyFunction(*func);
        fpm->run(*func);
        return func;
    }
    func->eraseFromParent();
    return nullptr;
}
