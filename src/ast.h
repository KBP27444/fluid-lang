#ifndef LANG_AST_H
#define LANG_AST_H

#include "token.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class TypeKind {
    Integer,
    Float,
    Boolean,
    String,
    Automatic
};

struct Expr {
    Expr(int lineValue, int colValue) : line(lineValue), col(colValue) {}
    virtual ~Expr() = default;

    int line;
    int col;
};

struct IntLit final : Expr {
    IntLit(int lineValue, int colValue, std::int64_t valueValue)
        : Expr(lineValue, colValue), value(valueValue) {}
    std::int64_t value;
};

struct FloatLit final : Expr {
    FloatLit(int lineValue, int colValue, double valueValue)
        : Expr(lineValue, colValue), value(valueValue) {}
    double value;
};

struct StrLit final : Expr {
    StrLit(int lineValue, int colValue, std::string valueValue)
        : Expr(lineValue, colValue), value(std::move(valueValue)) {}
    std::string value;
};

struct BoolLit final : Expr {
    BoolLit(int lineValue, int colValue, bool valueValue)
        : Expr(lineValue, colValue), value(valueValue) {}
    bool value;
};

struct VarRef final : Expr {
    VarRef(int lineValue, int colValue, std::string nameValue)
        : Expr(lineValue, colValue), name(std::move(nameValue)) {}
    std::string name;
};

struct Unary final : Expr {
    Unary(int lineValue, int colValue, TokenKind opValue, std::unique_ptr<Expr> operandValue)
        : Expr(lineValue, colValue), op(opValue), operand(std::move(operandValue)) {}
    TokenKind op;
    std::unique_ptr<Expr> operand;
};

struct Binary final : Expr {
    Binary(
        int lineValue,
        int colValue,
        TokenKind opValue,
        std::unique_ptr<Expr> leftValue,
        std::unique_ptr<Expr> rightValue)
        : Expr(lineValue, colValue),
          op(opValue),
          left(std::move(leftValue)),
          right(std::move(rightValue)) {}
    TokenKind op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct InvokeExpr final : Expr {
    InvokeExpr(
        int lineValue,
        int colValue,
        std::string nameValue,
        std::vector<std::unique_ptr<Expr>> argumentsValue)
        : Expr(lineValue, colValue),
          name(std::move(nameValue)),
          arguments(std::move(argumentsValue)) {}
    std::string name;
    std::vector<std::unique_ptr<Expr>> arguments;
};

struct Stmt {
    Stmt(int lineValue, int colValue) : line(lineValue), col(colValue) {}
    virtual ~Stmt() = default;

    int line;
    int col;
};

using Block = std::vector<std::unique_ptr<Stmt>>;

struct InitializeDecl {
    TypeKind type;
    std::string name;
    std::unique_ptr<Expr> value;
    int line;
    int col;
};

struct InitializeStmt final : Stmt {
    using Stmt::Stmt;
    std::vector<InitializeDecl> declarations;
};

struct ReassignStmt final : Stmt {
    using Stmt::Stmt;
    std::string name;
    std::unique_ptr<Expr> value;
};

struct FreeStmt final : Stmt {
    using Stmt::Stmt;
    std::string name;
};

struct DeleteStmt final : Stmt {
    using Stmt::Stmt;
    std::string name;
};

struct AutomaticStmt final : Stmt {
    using Stmt::Stmt;
    std::string name;
};

struct AliasStmt final : Stmt {
    using Stmt::Stmt;
    std::string name;
    TokenKind target;
};

struct OutputStmt final : Stmt {
    using Stmt::Stmt;
    std::vector<std::unique_ptr<Expr>> values;
};

struct InputStmt final : Stmt {
    using Stmt::Stmt;
    std::string name;
};

struct RepeatStmt final : Stmt {
    using Stmt::Stmt;
    Block body;
};

struct IfStmt final : Stmt {
    using Stmt::Stmt;
    std::unique_ptr<Expr> condition;
    Block body;
    Block otherwise;
};

struct ExitStmt final : Stmt {
    using Stmt::Stmt;
    std::unique_ptr<Expr> value;
};

struct ReturnStmt final : Stmt {
    using Stmt::Stmt;
    std::unique_ptr<Expr> value;
};

struct InvokeStmt final : Stmt {
    using Stmt::Stmt;
    std::string name;
    std::vector<std::unique_ptr<Expr>> arguments;
};

struct FunctionDef {
    std::string name;
    std::vector<std::string> parameters;
    Block body;
    int line;
    int col;
};

struct Program {
    std::vector<std::unique_ptr<FunctionDef>> functions;
};

#endif
