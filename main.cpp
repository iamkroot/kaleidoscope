#include <iostream>
#include "lexer.hpp"

int main() {
    int tok;
    do {
        tok = gettok();
        std::cout << "Token " << tok;
        switch (tok) {
            case Token::IDENT:
                std::cout << " " << identStr;
                break;
            case Token::NUM:
                std::cout << " " << numVal;
                break;
            default:
                break;
        }
        std::cout << std::endl;
    } while (tok != Token::EOF_);
    return 0;
}
