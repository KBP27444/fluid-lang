#include "lexer.h"

#include "error.h"

#include <cctype>
#include <cstring>
#include <unordered_map>
#include <utility>

namespace {

bool isAsciiLetter(char c) {
    const auto value = static_cast<unsigned char>(c);
    return std::isalpha(value) != 0;
}

bool isAsciiDigit(char c) {
    const auto value = static_cast<unsigned char>(c);
    return std::isdigit(value) != 0;
}

bool isIdentifierByte(char c) {
    return isAsciiLetter(c) || isAsciiDigit(c);
}

const std::unordered_map<std::string, TokenKind>& keywords() {
    static const std::unordered_map<std::string, TokenKind> table = {
        {"true", TokenKind::True},
        {"false", TokenKind::False},
        {"function", TokenKind::Function},
        {"initialize", TokenKind::Initialize},
        {"reassign", TokenKind::Reassign},
        {"free", TokenKind::Free},
        {"delete", TokenKind::Delete},
        {"automatic", TokenKind::Automatic},
        {"alias", TokenKind::Alias},
        {"output", TokenKind::Output},
        {"input", TokenKind::Input},
        {"repeat", TokenKind::Repeat},
        {"if", TokenKind::If},
        {"otherwise", TokenKind::Otherwise},
        {"exit", TokenKind::Exit},
        {"return", TokenKind::Return},
        {"invoke", TokenKind::Invoke},
        {"integer", TokenKind::IntegerType},
        {"float", TokenKind::FloatType},
        {"boolean", TokenKind::BooleanType},
        {"string", TokenKind::StringType},
        {"and", TokenKind::And},
        {"or", TokenKind::Or},
        {"not", TokenKind::Not},
    };
    return table;
}

} // namespace

const char* tokenKindName(TokenKind kind) {
    switch (kind) {
    case TokenKind::Identifier: return "Identifier";
    case TokenKind::IntegerLiteral: return "IntegerLiteral";
    case TokenKind::FloatLiteral: return "FloatLiteral";
    case TokenKind::StringLiteral: return "StringLiteral";
    case TokenKind::True: return "True";
    case TokenKind::False: return "False";
    case TokenKind::Function: return "Function";
    case TokenKind::Initialize: return "Initialize";
    case TokenKind::Reassign: return "Reassign";
    case TokenKind::Free: return "Free";
    case TokenKind::Delete: return "Delete";
    case TokenKind::Automatic: return "Automatic";
    case TokenKind::Alias: return "Alias";
    case TokenKind::Output: return "Output";
    case TokenKind::Input: return "Input";
    case TokenKind::Repeat: return "Repeat";
    case TokenKind::If: return "If";
    case TokenKind::Otherwise: return "Otherwise";
    case TokenKind::Exit: return "Exit";
    case TokenKind::Return: return "Return";
    case TokenKind::Invoke: return "Invoke";
    case TokenKind::IntegerType: return "IntegerType";
    case TokenKind::FloatType: return "FloatType";
    case TokenKind::BooleanType: return "BooleanType";
    case TokenKind::StringType: return "StringType";
    case TokenKind::And: return "And";
    case TokenKind::Or: return "Or";
    case TokenKind::Not: return "Not";
    case TokenKind::Plus: return "Plus";
    case TokenKind::Minus: return "Minus";
    case TokenKind::Star: return "Star";
    case TokenKind::Slash: return "Slash";
    case TokenKind::Equal: return "Equal";
    case TokenKind::NotEqual: return "NotEqual";
    case TokenKind::Less: return "Less";
    case TokenKind::LessEqual: return "LessEqual";
    case TokenKind::Greater: return "Greater";
    case TokenKind::GreaterEqual: return "GreaterEqual";
    case TokenKind::LeftParen: return "LeftParen";
    case TokenKind::RightParen: return "RightParen";
    case TokenKind::Comma: return "Comma";
    case TokenKind::Newline: return "Newline";
    case TokenKind::Indent: return "Indent";
    case TokenKind::Dedent: return "Dedent";
    case TokenKind::EndOfFile: return "EndOfFile";
    }
    return "Unknown";
}

Lexer::Lexer(const Source& source)
    : file_(source.file), text_(source.text) {}

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        if (inComment_) {
            consumeComment();
            continue;
        }

        if (atPhysicalLineStart_ && !indentationChecked_) {
            handlePhysicalLineStart();
        }

        if (isAtEnd()) {
            break;
        }
        if (inComment_) {
            continue;
        }

        if (startsWith("\\note")) {
            beginComment();
            continue;
        }

        const char c = peek();
        if (c == '\n') {
            addNewlineIfNeeded();
            advance();
            lineHasTokens_ = false;
        } else if (c == ' ') {
            advance();
        } else if (c == '\t') {
            fail("tab 不允许作为空白字符");
        } else {
            scanToken();
            lineHasTokens_ = true;
        }
    }

    if (inComment_) {
        failAt(commentLine_, commentCol_, "未闭合的 \\note 注释");
    }

    addNewlineIfNeeded();
    while (indents_.size() > 1) {
        add(TokenKind::Dedent, "", line_, col_);
        indents_.pop_back();
    }
    add(TokenKind::EndOfFile, "", line_, col_);
    return std::move(tokens_);
}

bool Lexer::isAtEnd() const {
    return pos_ >= text_.size();
}

char Lexer::peek(std::size_t offset) const {
    if (pos_ + offset >= text_.size()) {
        return '\0';
    }
    return text_[pos_ + offset];
}

bool Lexer::startsWith(const char* value) const {
    const std::size_t length = std::strlen(value);
    return pos_ + length <= text_.size() && text_.compare(pos_, length, value) == 0;
}

char Lexer::advance() {
    const char c = text_[pos_++];
    if (c == '\n') {
        ++line_;
        col_ = 1;
        atPhysicalLineStart_ = true;
        indentationChecked_ = false;
    } else {
        ++col_;
        atPhysicalLineStart_ = false;
    }
    return c;
}

void Lexer::advanceBytes(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        advance();
    }
}

void Lexer::beginComment() {
    commentLine_ = line_;
    commentCol_ = col_;
    inComment_ = true;
    advanceBytes(5);
}

void Lexer::consumeComment() {
    while (!isAtEnd()) {
        if (startsWith("\\note")) {
            const bool closedAtLineStart = atPhysicalLineStart_;
            advanceBytes(5);
            inComment_ = false;
            if (closedAtLineStart) {
                atPhysicalLineStart_ = true;
                indentationChecked_ = false;
            }
            return;
        }
        advance();
    }
}

void Lexer::handlePhysicalLineStart() {
    pendingIndent_ = 0;
    while (peek() == ' ') {
        ++pendingIndent_;
        advance();
    }

    if (peek() == '\t') {
        fail("tab 不允许用于缩进");
    }

    indentationChecked_ = true;
    if (isAtEnd() || peek() == '\n' || startsWith("\\note")) {
        return;
    }

    if (!lineHasTokens_) {
        applyIndentation();
    }
}

void Lexer::applyIndentation() {
    const int top = indents_.back();
    const int n = pendingIndent_;

    if (n == top) {
        return;
    }

    if (n > top) {
        if (indentUnit_ == 0) {
            indentUnit_ = n - top;
        }
        if (n != top + indentUnit_) {
            fail("缩进错误：一次只能增加一个缩进单位，且缩进单位必须统一");
        }
        indents_.push_back(n);
        add(TokenKind::Indent, "", line_, col_);
        return;
    }

    while (indents_.size() > 1 && indents_.back() > n) {
        indents_.pop_back();
        add(TokenKind::Dedent, "", line_, col_);
    }
    if (indents_.back() != n) {
        fail("缩进错误：回退缩进必须命中已有缩进层");
    }
}

void Lexer::addNewlineIfNeeded() {
    if (lineHasTokens_) {
        add(TokenKind::Newline, "", line_, col_);
        lineHasTokens_ = false;
    }
}

void Lexer::scanToken() {
    const int tokenLine = line_;
    const int tokenCol = col_;
    const char c = peek();

    if (isIdentifierByte(c)) {
        scanNumberOrIdentifier();
        return;
    }
    if (c == '"') {
        scanString();
        return;
    }
    if (c == '\\') {
        scanBareEscape();
        return;
    }

    switch (c) {
    case '+': advance(); add(TokenKind::Plus, "+", tokenLine, tokenCol); return;
    case '-': advance(); add(TokenKind::Minus, "-", tokenLine, tokenCol); return;
    case '*': advance(); add(TokenKind::Star, "*", tokenLine, tokenCol); return;
    case '/': advance(); add(TokenKind::Slash, "/", tokenLine, tokenCol); return;
    case '(': advance(); add(TokenKind::LeftParen, "(", tokenLine, tokenCol); return;
    case ')': advance(); add(TokenKind::RightParen, ")", tokenLine, tokenCol); return;
    case ',': advance(); add(TokenKind::Comma, ",", tokenLine, tokenCol); return;
    case '=':
        if (peek(1) == '=') {
            fail("本语言用 = 比较，不支持 ==");
        }
        advance();
        add(TokenKind::Equal, "=", tokenLine, tokenCol);
        return;
    case '!':
        if (peek(1) == '=') {
            advanceBytes(2);
            add(TokenKind::NotEqual, "!=", tokenLine, tokenCol);
            return;
        }
        fail("无法识别的字符 '!'，不等比较请使用 !=");
    case '<':
        if (peek(1) == '=') {
            advanceBytes(2);
            add(TokenKind::LessEqual, "<=", tokenLine, tokenCol);
        } else {
            advance();
            add(TokenKind::Less, "<", tokenLine, tokenCol);
        }
        return;
    case '>':
        if (peek(1) == '=') {
            advanceBytes(2);
            add(TokenKind::GreaterEqual, ">=", tokenLine, tokenCol);
        } else {
            advance();
            add(TokenKind::Greater, ">", tokenLine, tokenCol);
        }
        return;
    case '.':
        if (isAsciiDigit(peek(1))) {
            fail("浮点数字面量小数点两侧都必须有数字，不支持 .5");
        }
        fail("无法识别的字符 '.'");
    default:
        fail(std::string("无法识别的字符 '") + c + "'");
    }
}

void Lexer::scanString() {
    const int tokenLine = line_;
    const int tokenCol = col_;
    advance();

    std::string value;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\n') {
            failAt(tokenLine, tokenCol, "字符串字面量不能跨行");
        }
        if (peek() != '\\') {
            value.push_back(advance());
            continue;
        }

        const int escapeLine = line_;
        const int escapeCol = col_;
        advance();
        if (isAtEnd()) {
            failAt(tokenLine, tokenCol, "未闭合的字符串字面量");
        }
        const char escaped = advance();
        switch (escaped) {
        case 'n': value.push_back('\n'); break;
        case '\\': value.push_back('\\'); break;
        case '"': value.push_back('"'); break;
        default:
            failAt(escapeLine, escapeCol, "字符串只支持 \\n、\\\\、\\\" 转义");
        }
    }

    if (isAtEnd()) {
        failAt(tokenLine, tokenCol, "未闭合的字符串字面量");
    }
    advance();
    add(TokenKind::StringLiteral, value, tokenLine, tokenCol);
}

void Lexer::scanNumberOrIdentifier() {
    const int tokenLine = line_;
    const int tokenCol = col_;
    const std::size_t start = pos_;
    bool hasLetter = false;

    while (isIdentifierByte(peek())) {
        hasLetter = hasLetter || isAsciiLetter(peek());
        advance();
    }
    const std::string word = text_.substr(start, pos_ - start);

    if (hasLetter) {
        const auto found = keywords().find(word);
        add(found == keywords().end() ? TokenKind::Identifier : found->second,
            word,
            tokenLine,
            tokenCol);
        return;
    }

    if (peek() != '.') {
        add(TokenKind::IntegerLiteral, word, tokenLine, tokenCol);
        return;
    }

    if (!isAsciiDigit(peek(1))) {
        fail("浮点数字面量小数点两侧都必须有数字，不支持 1.");
    }
    advance();
    while (isAsciiDigit(peek())) {
        advance();
    }
    add(TokenKind::FloatLiteral,
        text_.substr(start, pos_ - start),
        tokenLine,
        tokenCol);
}

void Lexer::scanBareEscape() {
    const int tokenLine = line_;
    const int tokenCol = col_;
    if (peek(1) == 'n') {
        advanceBytes(2);
        add(TokenKind::StringLiteral, "\n", tokenLine, tokenCol);
        return;
    }
    fail("字符串外仅支持裸 \\n 换行字符串或 \\note 注释");
}

void Lexer::add(
    TokenKind kind,
    const std::string& text,
    int line,
    int col) {
    tokens_.push_back(Token{kind, text, line, col});
}

void Lexer::fail(const std::string& message) const {
    failAt(line_, col_, message);
}

void Lexer::failAt(int line, int col, const std::string& message) const {
    throw LangError(file_, line, col, message);
}
