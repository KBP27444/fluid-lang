#ifndef LANG_LEXER_H
#define LANG_LEXER_H

#include "source.h"
#include "token.h"

#include <cstddef>
#include <vector>

class Lexer {
public:
    explicit Lexer(const Source& source);

    std::vector<Token> tokenize();

private:
    std::vector<Token> tokens_;
    const std::string& file_;
    const std::string& text_;
    std::size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    bool inComment_ = false;
    int commentLine_ = 1;
    int commentCol_ = 1;
    bool lineHasTokens_ = false;
    bool atPhysicalLineStart_ = true;
    bool indentationChecked_ = false;
    int pendingIndent_ = 0;

    std::vector<int> indents_{0};
    int indentUnit_ = 0;

    bool isAtEnd() const;
    char peek(std::size_t offset = 0) const;
    bool startsWith(const char* value) const;
    char advance();
    void advanceBytes(std::size_t count);

    void beginComment();
    void consumeComment();
    void handlePhysicalLineStart();
    void applyIndentation();
    void addNewlineIfNeeded();

    void scanToken();
    void scanString();
    void scanNumberOrIdentifier();
    void scanBareEscape();

    void add(TokenKind kind, const std::string& text, int line, int col);
    [[noreturn]] void fail(const std::string& message) const;
    [[noreturn]] void failAt(
        int line,
        int col,
        const std::string& message) const;
};

#endif
