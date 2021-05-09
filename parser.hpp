#ifndef PARSER_HPP
#define PARSER_HPP

#include <map>
#include "lexer.hpp"
#include "ast.hpp"

extern std::map<char, int> binopPrec;

namespace parser {
    using namespace AST;
    static int curTok;

    static int getNextToken() {
        return curTok = gettok();
    }

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

    static std::unique_ptr<ExprAST> parseForExpr() {
        getNextToken();
        if (curTok != Token::IDENT)
            return logError("Expected identifier after 'for'");
        std::string idName = identStr;
        getNextToken();

        if (curTok != '=')
            return logError("Expected '=' after identifier");
        getNextToken();

        auto start = parseExpr();
        if (!start)
            return nullptr;
        if (curTok != ',')
            return logError("Expected ',' after start val");
        getNextToken();

        auto end = parseExpr();
        if (!end)
            return nullptr;

        std::unique_ptr<ExprAST> step;
        if (curTok == ',') {
            getNextToken();
            step = parseExpr();
            if (!step)
                return nullptr;
        }

        if (curTok != Token::IN)
            return logError("Expected 'in' after for");
        getNextToken();

        auto body = parseExpr();
        if (!body)
            return nullptr;
        return std::make_unique<ForExprAST>(idName, std::move(start), std::move(end), std::move(step), std::move(body));
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
            case Token::FOR:
                return parseForExpr();
            default:
                return logError("unknown token");
        }
    }

    static std::unique_ptr<ExprAST> parseUnaryExpr() {
        if (!isascii(curTok) || curTok == '(')
            return parsePrimary();
        int opcode = curTok;
        getNextToken();
        if (auto operand = parseUnaryExpr())
            return std::make_unique<UnaryExprAST>(opcode, std::move(operand));
        return nullptr;
    }

    static std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> lhs) {
        while (true) {
            int tokPrec = getTokPrec();
            if (tokPrec < exprPrec)
                return lhs;
            int binOp = curTok;
            getNextToken();
            auto rhs = parseUnaryExpr();
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
        auto lhs = parseUnaryExpr();
        if (!lhs)
            return nullptr;
        return parseBinOpRHS(0, std::move(lhs));
    }

    static std::unique_ptr<PrototypeAST> parseProto() {
        std::string fnName;
        enum Kind {
            IDENTIFIER = 0,
            UNARY = 1,
            BINARY = 2
        };
        Kind kind;
        unsigned binaryPrecedence = 30;
        switch (curTok) {
            case Token::IDENT:
                fnName = identStr;
                kind = Kind::IDENTIFIER;
                getNextToken();
                break;
            case Token::UNARY:
                getNextToken();
                if (!isascii(curTok))
                    return logErrorP("Expected unary op");
                fnName = "unary";
                fnName += static_cast<char>(curTok);
                kind = Kind::UNARY;
                getNextToken();
                break;
            case Token::BINARY:
                getNextToken();
                if (!isascii(curTok))
                    return logErrorP("Expected binary op");
                fnName = "binary";
                fnName += static_cast<char>(curTok);
                kind = Kind::BINARY;
                getNextToken();
                if (curTok == Token::NUM) {
                    if (numVal < 1 || numVal > 100)
                        return logErrorP("Precedence out of range");
                    binaryPrecedence = static_cast<unsigned>( numVal);
                    getNextToken();
                }
                break;
            default:
                return logErrorP("Expected function name");
        }
        if (curTok != '(')
            return logErrorP("Expected '(' in signature");
        std::vector<std::string> argNames;
        while (getNextToken() == Token::IDENT) {
            argNames.push_back(identStr);
        }
        if (curTok != ')')
            return logErrorP("Expected ')'");
        getNextToken();
        if (kind != Kind::IDENTIFIER && argNames.size() != kind) {
            return logErrorP("Invalid num of args");
        }
        return std::make_unique<PrototypeAST>(fnName, argNames, kind != Kind::IDENTIFIER, binaryPrecedence);
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
