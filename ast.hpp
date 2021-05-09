#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <utility>
#include <memory>
#include <vector>

namespace AST {
    class ExprAST {
    public:
        virtual ~ExprAST() = default;
    };

    class NumberExprAST : public ExprAST {
        double val;
    public:
        explicit NumberExprAST(double val) : val(val) {}
    };

    class VariableExprAST : public ExprAST {
        std::string name;

    public:
        VariableExprAST(std::string name) : name(std::move(name)) {}
    };

    class BinaryExprAST : public ExprAST {
        char op;
        std::unique_ptr<ExprAST> lhs, rhs;
    public:
        BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs) : op(op),
                                                                                             lhs(std::move(lhs)),
                                                                                             rhs(std::move(rhs)) {}
    };

    class CallExprAST : public ExprAST {
        std::string callee;
        std::vector<std::unique_ptr<ExprAST>> args;
    public:
        CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args) : callee(callee),
                                                                                             args(std::move(args)) {}
    };

    class PrototypeAST {
        std::string name;
        std::vector<std::string> args;
    public:
        PrototypeAST(const std::string &name, std::vector<std::string> args) : name(name), args(std::move(args)) {}

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
    };
}

#endif //AST_HPP
