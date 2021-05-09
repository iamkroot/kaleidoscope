#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include "ast.hpp"
#include "KaleidoscopeJIT.h"

using namespace llvm;
using namespace AST;


extern std::unique_ptr<LLVMContext> ctx;
extern std::unique_ptr<IRBuilder<>> builder;
extern std::unique_ptr<Module> module;
extern std::unique_ptr<legacy::FunctionPassManager> fpm;
extern std::unique_ptr<llvm::orc::KaleidoscopeJIT> jit;

static std::map<std::string, Value*> namedValues;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> functionProtos;
extern std::map<char, int> binopPrec;

Value* logErrorV(const char* str) {
    fprintf(stderr, "Error: %s\n", str);
    return nullptr;
}

Function* getFunction(const std::string &name) {
    if (auto* f = module->getFunction(name))
        return f;
    auto fi = functionProtos.find(name);
    if (fi != functionProtos.end())
        return fi->second->codegen();
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

Value* UnaryExprAST::codegen() {
    Value* operandV = operand->codegen();
    if (!operandV)
        return nullptr;
    Function* f = getFunction(std::string("unary") + opCode);
    if (!f)
        return logErrorV("Unknown unary op");
    return builder->CreateCall(f, operandV, "unop");
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
            break;
    }
    Function* f = getFunction(std::string("binary") + op);
    assert(f && "binary op not found");
    Value* ops[2] = {l, r};
    return builder->CreateCall(f, ops, "binop");
}

Value* CallExprAST::codegen() {
    Function* calleeFunc = getFunction(callee);
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

Value* IfExprAST::codegen() {
    Value* condV = cond->codegen();
    if (!condV)
        return nullptr;
    condV = builder->CreateFCmpONE(condV, ConstantFP::get(*ctx, APFloat(0.0)), "ifcond");

    Function* func = builder->GetInsertBlock()->getParent();
    BasicBlock* thenBB = BasicBlock::Create(*ctx, "then", func);
    BasicBlock* elseBB = BasicBlock::Create(*ctx, "else");
    BasicBlock* mergeBB = BasicBlock::Create(*ctx, "ifcont");
    builder->CreateCondBr(condV, thenBB, elseBB);
    builder->SetInsertPoint(thenBB);
    Value* thenV = then->codegen();
    if (!thenV)
        return nullptr;
    builder->CreateBr(mergeBB);
    thenBB = builder->GetInsertBlock();

    func->getBasicBlockList().push_back(elseBB);
    builder->SetInsertPoint(elseBB);
    Value* elseV = else_->codegen();
    if (!elseV)
        return nullptr;
    builder->CreateBr(mergeBB);
    elseBB = builder->GetInsertBlock();

    func->getBasicBlockList().push_back(mergeBB);
    builder->SetInsertPoint(mergeBB);
    PHINode* pn = builder->CreatePHI(Type::getDoubleTy(*ctx), 2, "iftmp");
    pn->addIncoming(thenV, thenBB);
    pn->addIncoming(elseV, elseBB);
    return pn;
}

Value* ForExprAST::codegen() {
    Value* startV = start->codegen();
    if (!startV)
        return nullptr;
    Function* func = builder->GetInsertBlock()->getParent();
    BasicBlock* preheaderBB = builder->GetInsertBlock();
    BasicBlock* loopBB = BasicBlock::Create(*ctx, "loop", func);
    builder->CreateBr(loopBB);

    builder->SetInsertPoint(loopBB);
    PHINode* var = builder->CreatePHI(Type::getDoubleTy(*ctx), 2, varName);
    var->addIncoming(startV, preheaderBB);

    Value* shadowedVal = namedValues[varName];
    namedValues[varName] = var;

    if (!body->codegen())
        return nullptr;

    Value* stepV;
    if (step) {
        stepV = step->codegen();
        if (!stepV)
            return nullptr;
    } else {
        stepV = ConstantFP::get(*ctx, APFloat(1.0));
    }
    Value* nextVar = builder->CreateFAdd(var, stepV, "nextvar");
    Value* endV = end->codegen();
    if (!endV)
        return nullptr;
    endV = builder->CreateFCmpONE(endV, ConstantFP::get(*ctx, APFloat(0.0)), "loopcond");

    BasicBlock* loopEndBB = builder->GetInsertBlock();
    BasicBlock* afterBB = BasicBlock::Create(*ctx, "afterloop", func);
    builder->CreateCondBr(endV, loopBB, afterBB);
    builder->SetInsertPoint(afterBB);
    var->addIncoming(nextVar, loopEndBB);
    if (shadowedVal)
        namedValues[varName] = shadowedVal;
    else
        namedValues.erase(varName);
    return Constant::getNullValue(Type::getDoubleTy(*ctx));
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
    auto &p = *proto;
    functionProtos[p.getName()] = std::move(proto);
    Function* func = getFunction(p.getName());

    if (!func)
        return nullptr;
    if (p.isBinaryOp()) {
        binopPrec[p.getOperatorName()] = p.getBinaryPrecedence();
    }
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
