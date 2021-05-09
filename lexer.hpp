#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <sstream>

enum Token {
    EOF_ = -1,
    DEF = -2,
    EXTERN = -3,
    IDENT = -4,
    NUM = -5,
    IF = -6,
    THEN = -7,
    ELSE = -8,
};

static std::string identStr;
static double numVal;

static int gettok() {
    static int prevChar = ' ';
    while (isspace(prevChar))
        prevChar = getchar();
    if (isalpha(prevChar)) {
        std::stringstream ss;
        ss << static_cast<char>(prevChar);

        while (isalnum(prevChar = getchar())) {
            ss << static_cast<char>(prevChar);
        }
        ss >> identStr;
        if (identStr == "def")
            return Token::DEF;
        else if (identStr == "extern")
            return Token::EXTERN;
        else if (identStr == "if")
            return Token::IF;
        else if (identStr == "then")
            return Token::THEN;
        else if (identStr == "else")
            return Token::ELSE;
        return Token::IDENT;
    }
    if (isdigit(prevChar) || prevChar == '.') {
        std::stringstream ss;
        do {
            ss << static_cast<char>(prevChar);
            prevChar = getchar();
        } while (isdigit(prevChar) || prevChar == '.');
        ss >> numVal;
        return Token::NUM;
    }
    if (prevChar == '#') {
        do {
            prevChar = getchar();
        } while (prevChar != EOF && prevChar != '\n' && prevChar != '\r');
        if (prevChar != EOF)
            return gettok();
    }
    if (prevChar == EOF)
        return Token::EOF_;
    int curChar = prevChar;
    prevChar = getchar();
    return curChar;
}

#endif //LEXER_HPP
