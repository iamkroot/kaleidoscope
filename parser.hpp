#ifndef PARSER_HPP
#define PARSER_HPP

#include <map>
#include "lexer.hpp"
#include "ast.hpp"

namespace parser {
    using namespace AST;
    static int curTok;

    static int getNextToken() {
        return curTok = gettok();
    }

    static const std::map<char, int> binopPrec = {{'<', 10},
                                                  {'+', 20},
                                                  {'-', 30},
                                                  {'*', 40}};

    static int getTokPrec() {
        if (!isascii(curTok))
            return -1;
        auto prec = binopPrec.find(static_cast<char>(curTok));
        if (prec != binopPrec.end())
            return prec->second;
        return -1;
    }

    std::unique_ptr<ExprAST> logError(const char* str) {
        fprintf(stderr, "Error: %s\n", str);
        return nullptr;
    }

    std::unique_ptr<PrototypeAST> logErrorP(const char* str) {
        fprintf(stderr, "Error: %s\n", str);
        return nullptr;
    }

    static std::unique_ptr<ExprAST> parseExpr();

    static std::unique_ptr<ExprAST> parseNumExpr() {
        auto res = std::make_unique<NumberExprAST>(numVal);
        getNextToken();
        return std::move(res);
    }

    static std::unique_ptr<ExprAST> parseParenExpr() {
        getNextToken();
        auto val = parseExpr();
        if (val == nullptr)
            return nullptr;
        if (curTok != ')')
            return logError("Expected ')'");
        getNextToken();
        return val;
    }

    static std::unique_ptr<ExprAST> parseIdentExpr() {
        auto idName = identStr;
        getNextToken();
        if (curTok != '(')
            return std::make_unique<VariableExprAST>(idName);

        getNextToken();  // eat '('
        std::vector<std::unique_ptr<ExprAST>> args;
        if (curTok != ')') {
            while (true) {
                if (auto arg = parseExpr())
                    args.push_back(std::move(arg));
                else
                    return nullptr;
                if (curTok == ')')
                    break;
                if (curTok != ',')
                    return logError("Expected ')' or ',' after arg");
                getNextToken();
            }
        }
        getNextToken();  // eat '('
        return std::make_unique<CallExprAST>(idName, std::move(args));
    }

    static std::unique_ptr<ExprAST> parseIfExpr() {
        getNextToken();
        auto cond = parseExpr();
        if (!cond)
            return nullptr;
        if (curTok != Token::THEN)
            return logError("Expected then");
        getNextToken();

        auto then = parseExpr();
        if (!then)
            return nullptr;
        if (curTok != Token::ELSE)
            return logError("Expected else");
        getNextToken();

        auto else_ = parseExpr();
        if (!else_)
            return nullptr;
        return std::make_unique<IfExprAST>(std::move(cond), std::move(then), std::move(else_));
    }

    static std::unique_ptr<ExprAST> parsePrimary() {
        switch (curTok) {
            case Token::IDENT:
                return parseIdentExpr();
            case Token::NUM:
                return parseNumExpr();
            case '(':
                return parseParenExpr();
            case Token::IF:
                return parseIfExpr();
            default:
                return logError("unknown token");
        }
    }

    static std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> lhs) {
        while (true) {
            int tokPrec = getTokPrec();
            if (tokPrec < exprPrec)
                return lhs;
            int binOp = curTok;
            getNextToken();
            auto rhs = parsePrimary();
            if (!rhs)
                return nullptr;
            int nextPrec = getTokPrec();
            if (tokPrec < nextPrec) {
                rhs = parseBinOpRHS(tokPrec + 1, std::move(rhs));
                if (!rhs)
                    return nullptr;
            }
            lhs = std::make_unique<BinaryExprAST>(binOp, std::move(lhs), std::move(rhs));
        }
    }

    static std::unique_ptr<ExprAST> parseExpr() {
        auto lhs = parsePrimary();
        if (!lhs)
            return nullptr;
        return parseBinOpRHS(0, std::move(lhs));
    }

    static std::unique_ptr<PrototypeAST> parseProto() {
        if (curTok != Token::IDENT)
            return logErrorP("Expected function name");
        auto fnName = identStr;
        getNextToken();
        if (curTok != '(')
            return logErrorP("Expected '(' in signature");
        std::vector<std::string> argNames;
        while (getNextToken() == Token::IDENT) {
            argNames.push_back(identStr);
        }
        if (curTok != ')')
            return logErrorP("Expected ')'");
        getNextToken();
        return std::make_unique<PrototypeAST>(fnName, argNames);
    }

    static std::unique_ptr<FunctionAST> parseDefn() {
        getNextToken();
        auto proto = parseProto();
        if (!proto)
            return nullptr;
        if (auto expr = parseExpr()) {
            return std::make_unique<FunctionAST>(std::move(proto), std::move(expr));
        }
        return nullptr;
    }

    static std::unique_ptr<FunctionAST> parseTopLevelExpr() {
        if (auto expr = parseExpr()) {
            auto proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
            return std::make_unique<FunctionAST>(std::move(proto), std::move(expr));
        }
        return nullptr;
    }

    static std::unique_ptr<PrototypeAST> parseExtern() {
        getNextToken();
        return parseProto();
    }
}

#endif //PARSER_HPP
