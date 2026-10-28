#ifndef LANG_TOKEN_H
#define LANG_TOKEN_H

#include <string>

enum class TokenKind {
    Identifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,

    True,
    False,

    Function,
    Initialize,
    Reassign,
    Free,
    Delete,
    Automatic,
    Alias,
    Output,
    Input,
    Repeat,
    If,
    Otherwise,
    Exit,
    Return,
    Invoke,

    IntegerType,
    FloatType,
    BooleanType,
    StringType,

    And,
    Or,
    Not,

    Plus,
    Minus,
    Star,
    Slash,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    LeftParen,
    RightParen,
    Comma,

    Newline,
    Indent,
    Dedent,
    EndOfFile
};

struct Token {
    TokenKind kind;
    std::string text;
    int line;
    int col;
};

const char* tokenKindName(TokenKind kind);

#endif
