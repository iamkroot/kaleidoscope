#include <iostream>
#include "parser.hpp"
#include "llvm/IR/IRBuilder.h"

using namespace parser;
extern std::unique_ptr<LLVMContext> ctx;
extern std::unique_ptr<Module> module;
extern std::unique_ptr<IRBuilder<>> builder;

static void initModule() {
    ctx = std::make_unique<LLVMContext>();
    module = std::make_unique<Module>("my jit", *ctx);
    builder = std::make_unique<IRBuilder<>>(*ctx);
}

static void handleDefn() {
    if (auto fn = parseDefn()) {
        if (auto* fnIR = fn->codegen()) {
            fprintf(stdout, "Read fn defn:\n");
            fnIR->print(outs());
            fprintf(stdout, "\n");
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
        }
    } else {
        getNextToken();
    }
}

static void handleTopLevelExpr() {
    if (auto fn = parseTopLevelExpr()) {
        if (auto* fnIR = fn->codegen()) {
            fprintf(stdout, "Read top-level expr:\n");
            fnIR->print(outs());
            fprintf(stdout, "\n");
            fnIR->eraseFromParent();
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

int main() {
    using namespace parser;
    fprintf(stdout, "ready> ");
    getNextToken();

    initModule();

    mainLoop();
    module->print(errs(), nullptr);
    return 0;
}
