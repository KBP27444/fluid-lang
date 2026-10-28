#include "../src/error.h"
#include "../src/lexer.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

namespace {

std::vector<Token> lex(const std::string& text) {
    Source source{"lexer_smoke.fld", text};
    Lexer lexer(source);
    return lexer.tokenize();
}

void expectError(const std::string& text, const std::string& fragment) {
    try {
        (void)lex(text);
    } catch (const LangError& error) {
        assert(std::string(error.what()).find(fragment) != std::string::npos);
        return;
    }
    assert(false && "expected LangError");
}

} // namespace

int main() {
    const auto tokens = lex(R"LANG(function main
  output "a"\note hidden
hidden\note,"b"
  output "\note", "\\note", \n
  if not 1a = 2
    output "yes"
  otherwise
    output "no"
)LANG");

    assert(tokens.front().kind == TokenKind::Function);
    assert(tokens.back().kind == TokenKind::EndOfFile);
    assert(std::count_if(tokens.begin(), tokens.end(), [](const Token& token) {
        return token.kind == TokenKind::Indent;
    }) == 3);
    assert(std::count_if(tokens.begin(), tokens.end(), [](const Token& token) {
        return token.kind == TokenKind::Dedent;
    }) == 3);
    assert(std::count_if(tokens.begin(), tokens.end(), [](const Token& token) {
        return token.kind == TokenKind::Newline;
    }) == 7);
    assert(std::any_of(tokens.begin(), tokens.end(), [](const Token& token) {
        return token.kind == TokenKind::Identifier && token.text == "1a";
    }));
    assert(std::any_of(tokens.begin(), tokens.end(), [](const Token& token) {
        return token.kind == TokenKind::StringLiteral && token.text == "\note";
    }));
    assert(std::any_of(tokens.begin(), tokens.end(), [](const Token& token) {
        return token.kind == TokenKind::StringLiteral && token.text == "\\note";
    }));

    expectError("if 1 == 1\n", "==");
    expectError("function main\n\toutput 1\n", "tab");
    expectError("function main\n  if true\n      output 1\n", "缩进");
    expectError("function main\n  output 1\n output 2\n", "缩进");
    expectError("\\note never closed", "未闭合");
    expectError("output .5\n", ".5");
    expectError("output 1.\n", "1.");
    return 0;
}
