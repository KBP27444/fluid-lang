// 沙箱内的内存验证工具：从 stdin 读取 "源码长度 输入长度\n + 源码 + 输入"，
// 直接走 Lexer -> Parser -> Interpreter，不访问文件系统。
// 仅用于在无法读取文件的受限环境中验证用例逻辑；正式测试请用 run_tests.py。

#include "error.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include "source.h"

#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    std::string header;
    if (!std::getline(std::cin, header)) {
        return 3;
    }
    std::istringstream headerStream(header);
    std::size_t sourceLength = 0;
    std::size_t inputLength = 0;
    if (!(headerStream >> sourceLength >> inputLength)) {
        return 3;
    }

    std::string source(sourceLength, '\0');
    if (sourceLength > 0 &&
        !std::cin.read(&source[0], static_cast<std::streamsize>(sourceLength))) {
        return 3;
    }
    std::string inputText(inputLength, '\0');
    if (inputLength > 0 &&
        !std::cin.read(&inputText[0], static_cast<std::streamsize>(inputLength))) {
        return 3;
    }

    std::istringstream input(inputText);
    try {
        Source src{"<case>", source};
        Lexer lexer(src);
        Parser parser(src.file, lexer.tokenize());
        Program program = parser.parse();

        Interpreter interpreter(src.file, program, std::cout, input);
        return interpreter.run();
    } catch (const LangError& error) {
        std::cerr << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
