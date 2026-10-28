#ifndef LANG_PARSER_H
#define LANG_PARSER_H

#include "ast.h"
#include "token.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class Parser {
public:
    Parser(std::string file, std::vector<Token> tokens);

    Program parse();

private:
    struct Signature {
        Token name;
        std::vector<Token> parameters;
    };

    struct FunctionRegion {
        std::size_t signatureIndex;
        std::size_t bodyBegin;
        std::size_t bodyEnd;
    };

    using AliasMap = std::unordered_map<std::string, TokenKind>;

    enum class BlockContext {
        FunctionTop,
        Nested
    };

    struct InvokeParts {
        Token name;
        std::vector<std::unique_ptr<Expr>> arguments;
    };

    std::string file_;
    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
    std::vector<Signature> signatures_;
    std::unordered_map<std::string, std::size_t> signatureByName_;
    std::vector<FunctionRegion> regions_;

    const Token& peek(std::size_t offset = 0) const;
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);
    const Token& advance();
    const Token& expect(TokenKind kind, const std::string& message);
    Token expectIdentifier(const std::string& role, bool warn);
    void expectNewline();

    void scanSignatures();
    void skipRawBlock();
    void checkAllDeclarations();
    void checkDeclarationNames(
        const Signature& signature,
        const std::vector<Token>& variables,
        const std::vector<Token>& aliases) const;
    void warnIfSpecialIdentifier(const Token& token) const;

    std::unique_ptr<FunctionDef> parseFunction();
    Block parseBlock(BlockContext context, AliasMap& aliases);
    std::unique_ptr<Stmt> parseStatement(BlockContext context, AliasMap& aliases);

    std::unique_ptr<Stmt> parseInitialize(const Token& command);
    std::unique_ptr<Stmt> parseReassign(const Token& command);
    std::unique_ptr<Stmt> parseFree(const Token& command);
    std::unique_ptr<Stmt> parseDelete(const Token& command);
    std::unique_ptr<Stmt> parseAutomatic(const Token& command);
    std::unique_ptr<Stmt> parseAlias(const Token& command, BlockContext context, AliasMap& aliases);
    std::unique_ptr<Stmt> parseOutput(const Token& command);
    std::unique_ptr<Stmt> parseInput(const Token& command);
    std::unique_ptr<Stmt> parseRepeat(const Token& command, AliasMap& aliases);
    std::unique_ptr<Stmt> parseIf(const Token& command, AliasMap& aliases);
    std::unique_ptr<Stmt> parseExit(const Token& command);
    std::unique_ptr<Stmt> parseReturn(const Token& command);
    std::unique_ptr<Stmt> parseInvokeStatement(const Token& command);

    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseAndOr();
    std::unique_ptr<Expr> parseNot();
    std::unique_ptr<Expr> parseComparison();
    std::unique_ptr<Expr> parseAdditive();
    std::unique_ptr<Expr> parseMultiplicative();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePrimary();
    InvokeParts parseInvokeParts();

    TypeKind parseType();
    bool isTypeToken(TokenKind kind) const;
    bool isComparison(TokenKind kind) const;
    bool isBuiltinCommand(TokenKind kind) const;

    [[noreturn]] void fail(const Token& token, const std::string& message) const;
    [[noreturn]] void arityError(const Token& name, std::size_t expected) const;
};

#endif
