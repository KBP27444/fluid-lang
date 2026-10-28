#include "error.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include "source.h"

#include <exception>
#include <iostream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc != 3 || std::string(argv[1]) != "run") {
        std::cerr << "用法: fluid.exe run <file.fld>\n";
        return 2;
    }

    Source source;
    try {
        source = loadSource(argv[2]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }

    try {
        Lexer lexer(source);
        Parser parser(source.file, lexer.tokenize());
        Program program = parser.parse();

        Interpreter interpreter(source.file, program);
        return interpreter.run();
    } catch (const LangError& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
