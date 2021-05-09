#include <iostream>
#include "llvm/IR/IRBuilder.h"
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include "parser.hpp"
#include "KaleidoscopeJIT.h"

using namespace parser;

std::unique_ptr<LLVMContext> ctx;
std::unique_ptr<Module> module;
std::unique_ptr<IRBuilder<>> builder;
std::unique_ptr<legacy::FunctionPassManager> fpm;
std::unique_ptr<llvm::orc::KaleidoscopeJIT> jit;
std::map<std::string, std::unique_ptr<PrototypeAST>> functionProtos;
std::map<char, int> binopPrec = {{'<', 10},
                                 {'+', 20},
                                 {'-', 30},
                                 {'*', 40}};;

static void initModuleAndPassMgr() {
    ctx = std::make_unique<LLVMContext>();
    module = std::make_unique<Module>("my jit", *ctx);
    module->setDataLayout(jit->getTargetMachine().createDataLayout());

    fpm = std::make_unique<legacy::FunctionPassManager>(module.get());
    fpm->add(createInstructionCombiningPass());
    fpm->add(createReassociatePass());
    fpm->add(createGVNPass());
    fpm->add(createCFGSimplificationPass());
    fpm->doInitialization();
    builder = std::make_unique<IRBuilder<>>(*ctx);
}

static void handleDefn() {
    if (auto fn = parseDefn()) {
        if (auto* fnIR = fn->codegen()) {
            fprintf(stdout, "Read fn defn:\n");
            fnIR->print(outs());
            fprintf(stdout, "\n");
            jit->addModule(std::move(module));
            initModuleAndPassMgr();
        }
    } else {
        getNextToken();
    }
}

static void handleExtern() {
    if (auto proto = parseExtern()) {
        if (auto* fnIR = proto->codegen()) {
            fprintf(stdout, "Read extern:\n");
            fnIR->print(outs());
            fprintf(stdout, "\n");
            functionProtos[proto->getName()] = std::move(proto);
        }
    } else {
        getNextToken();
    }
}

static void handleTopLevelExpr() {
    if (auto fn = parseTopLevelExpr()) {
        if (fn->codegen()) {
            auto h = jit->addModule(std::move(module));
            initModuleAndPassMgr();
            auto exprSym = jit->findSymbol("__anon_expr");
            assert(exprSym && "Function not found");

            auto fp = (double (*)()) (intptr_t) cantFail(exprSym.getAddress());
            fprintf(stdout, "Evaluated to %f\n", fp());
            jit->removeModule(h);
        }
    } else {
        getNextToken();
    }
}

static void mainLoop() {
    while (true) {
        fprintf(stdout, "ready> ");
        switch (curTok) {
            case Token::EOF_:
                return;
            case ';':
                getNextToken();
                break;
            case Token::DEF:
                handleDefn();
                break;
            case Token::EXTERN:
                handleExtern();
                break;
            default:
                handleTopLevelExpr();
                break;
        }
    }
}

/// putchard - putchar that takes a double and returns 0.
extern "C" double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

int main() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    fprintf(stdout, "ready> ");
    getNextToken();

    jit = std::make_unique<llvm::orc::KaleidoscopeJIT>();

    initModuleAndPassMgr();

    mainLoop();
    module->print(errs(), nullptr);
    return 0;
}
