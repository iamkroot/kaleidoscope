#include <iostream>
#include "parser.hpp"

int main() {
    using namespace parser;
    fprintf(stdout, "ready> ");
    getNextToken();
    mainLoop();
    return 0;
}
