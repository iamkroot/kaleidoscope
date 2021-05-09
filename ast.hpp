#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <utility>
#include <memory>
#include <vector>
#include <llvm/IR/Value.h>

namespace AST {
    using namespace llvm;

    class ExprAST {
    public:
        virtual ~ExprAST() = default;

        virtual Value* codegen() = 0;
    };

    class NumberExprAST : public ExprAST {
        double val;
    public:
        explicit NumberExprAST(double val) : val(val) {}

        Value* codegen() override;
    };

    class VariableExprAST : public ExprAST {
        std::string name;

    public:
        VariableExprAST(std::string name) : name(std::move(name)) {}

        Value* codegen() override;

    };

    class BinaryExprAST : public ExprAST {
        char op;
        std::unique_ptr<ExprAST> lhs, rhs;
    public:
        BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs) : op(op),
                                                                                             lhs(std::move(lhs)),
                                                                                             rhs(std::move(rhs)) {}

        Value* codegen() override;

    };

    class CallExprAST : public ExprAST {
        std::string callee;
        std::vector<std::unique_ptr<ExprAST>> args;
    public:
        CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args) : callee(callee),
                                                                                             args(std::move(args)) {}

        Value* codegen() override;

    };


    class PrototypeAST {
        std::string name;
        std::vector<std::string> args;
    public:
        PrototypeAST(const std::string &name, std::vector<std::string> args) : name(name), args(std::move(args)) {}

        Function* codegen();

        [[nodiscard]] const std::string &getName() const {
            return name;
        }
    };

    class FunctionAST {
        std::unique_ptr<PrototypeAST> proto;
        std::unique_ptr<ExprAST> body;

    public:
        FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprAST> body) : proto(std::move(proto)),
                                                                                          body(std::move(body)) {}

        Function* codegen();
    };
}

#endif //AST_HPP
