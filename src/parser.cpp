#include "parser.h"

#include "error.h"

#include <memory>
#include <stdexcept>
#include <utility>

Parser::Parser(std::string file, std::vector<Token> tokens)
    : file_(std::move(file)), tokens_(std::move(tokens)) {}

Program Parser::parse() {
    scanSignatures();
    checkAllDeclarations();

    pos_ = 0;
    Program program;
    while (match(TokenKind::Newline)) {}
    while (!check(TokenKind::EndOfFile)) {
        program.functions.push_back(parseFunction());
        while (match(TokenKind::Newline)) {}
    }
    return program;
}

const Token& Parser::peek(std::size_t offset) const {
    const std::size_t index = pos_ + offset;
    return index < tokens_.size() ? tokens_[index] : tokens_.back();
}

bool Parser::check(TokenKind kind) const {
    return peek().kind == kind;
}

bool Parser::match(TokenKind kind) {
    if (!check(kind)) {
        return false;
    }
    advance();
    return true;
}

const Token& Parser::advance() {
    const Token& current = peek();
    if (!check(TokenKind::EndOfFile)) {
        ++pos_;
    }
    return current;
}

const Token& Parser::expect(TokenKind kind, const std::string& message) {
    if (!check(kind)) {
        fail(peek(), message);
    }
    return advance();
}

Token Parser::expectIdentifier(const std::string& role, bool warn) {
    if (!check(TokenKind::Identifier)) {
        fail(peek(), "预期标识符作为" + role);
    }
    Token token = advance();
    if (warn) {
        warnIfSpecialIdentifier(token);
    }
    return token;
}

void Parser::expectNewline() {
    expect(TokenKind::Newline, "语句必须在此结束，每条语句独占一行");
}

void Parser::scanSignatures() {
    pos_ = 0;
    while (match(TokenKind::Newline)) {}

    while (!check(TokenKind::EndOfFile)) {
        expect(TokenKind::Function, "顶层只允许 function 函数定义");

        Signature signature{expectIdentifier("函数名", true), {}};
        if (signatureByName_.count(signature.name.text) != 0) {
            fail(signature.name, "函数名重复: " + signature.name.text);
        }

        if (!check(TokenKind::Newline)) {
            while (true) {
                Token parameter = expectIdentifier("参数名", true);
                for (const Token& existing : signature.parameters) {
                    if (existing.text == parameter.text) {
                        fail(parameter, "参数名重复: " + parameter.text);
                    }
                }
                signature.parameters.push_back(std::move(parameter));
                if (!match(TokenKind::Comma)) {
                    break;
                }
                if (check(TokenKind::Newline)) {
                    fail(peek(), "参数列表不允许尾逗号");
                }
            }
        }

        if (signature.name.text == "main" && !signature.parameters.empty()) {
            fail(signature.name, "main 函数不允许有参数");
        }

        const std::size_t signatureIndex = signatures_.size();
        signatureByName_.emplace(signature.name.text, signatureIndex);
        signatures_.push_back(std::move(signature));
        expectNewline();

        FunctionRegion region{signatureIndex, pos_, pos_};
        if (check(TokenKind::Indent)) {
            skipRawBlock();
        }
        region.bodyEnd = pos_;
        regions_.push_back(region);

        while (match(TokenKind::Newline)) {}
    }

    if (signatureByName_.count("main") == 0) {
        fail(peek(), "程序必须恰好包含一个无参数 main 函数");
    }
}

void Parser::skipRawBlock() {
    expect(TokenKind::Indent, "内部错误：预期 INDENT");
    int depth = 0;
    while (!check(TokenKind::EndOfFile)) {
        if (check(TokenKind::Indent)) {
            ++depth;
        } else if (check(TokenKind::Dedent)) {
            if (depth == 0) {
                advance();
                return;
            }
            --depth;
        }
        advance();
    }
    fail(peek(), "函数块缺少 DEDENT");
}

void Parser::checkAllDeclarations() {
    for (const FunctionRegion& region : regions_) {
        std::vector<Token> variables;
        std::vector<Token> aliases;
        AliasMap aliasTargets;

        for (std::size_t i = region.bodyBegin; i < region.bodyEnd; ++i) {
            if (tokens_[i].kind == TokenKind::Alias &&
                i + 1 < region.bodyEnd &&
                tokens_[i + 1].kind == TokenKind::Identifier) {
                const Token& name = tokens_[i + 1];
                aliases.push_back(name);
                if (i + 2 < region.bodyEnd && isBuiltinCommand(tokens_[i + 2].kind)) {
                    aliasTargets[name.text] = tokens_[i + 2].kind;
                }
            }
        }

        for (std::size_t i = region.bodyBegin; i < region.bodyEnd; ++i) {
            bool isInitialize = tokens_[i].kind == TokenKind::Initialize;
            if (tokens_[i].kind == TokenKind::Identifier) {
                const auto found = aliasTargets.find(tokens_[i].text);
                isInitialize = found != aliasTargets.end() &&
                               found->second == TokenKind::Initialize;
            }
            if (!isInitialize) {
                continue;
            }

            for (std::size_t j = i + 1;
                 j < region.bodyEnd && tokens_[j].kind != TokenKind::Newline;
                 ++j) {
                if (isTypeToken(tokens_[j].kind) &&
                    j + 1 < region.bodyEnd &&
                    tokens_[j + 1].kind == TokenKind::Identifier) {
                    variables.push_back(tokens_[j + 1]);
                }
            }
        }

        checkDeclarationNames(signatures_[region.signatureIndex], variables, aliases);
    }
}

void Parser::checkDeclarationNames(
    const Signature& signature,
    const std::vector<Token>& variables,
    const std::vector<Token>& aliases) const {
    std::unordered_map<std::string, std::string> seen;

    auto add = [this, &seen](const Token& name, const std::string& category) {
        if (signatureByName_.count(name.text) != 0) {
            fail(name, "名称 '" + name.text + "' 与函数名冲突");
        }
        const auto found = seen.find(name.text);
        if (found != seen.end()) {
            fail(name,
                 "名称 '" + name.text + "' 与" + found->second +
                     "冲突，参数、变量和别名新名必须互不重复");
        }
        seen.emplace(name.text, category);
    };

    for (const Token& parameter : signature.parameters) {
        add(parameter, "参数");
    }
    for (const Token& variable : variables) {
        add(variable, "initialize 变量");
    }
    for (const Token& alias : aliases) {
        add(alias, "alias 新名");
    }
}

void Parser::warnIfSpecialIdentifier(const Token& token) const {
    if (token.text == "int" || token.text == "auto") {
        printWarning(file_, token.line, token.col,
                     "标识符 '" + token.text + "' 与类型简写同名，建议避免使用");
    }
}

std::unique_ptr<FunctionDef> Parser::parseFunction() {
    const Token& keyword = expect(TokenKind::Function, "顶层只允许 function 函数定义");
    Token name = expectIdentifier("函数名", false);
    const auto found = signatureByName_.find(name.text);
    if (found == signatureByName_.end()) {
        fail(name, "内部错误：缺少函数签名");
    }
    const Signature& signature = signatures_[found->second];

    if (!check(TokenKind::Newline)) {
        while (true) {
            (void)expectIdentifier("参数名", false);
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
    }
    expectNewline();

    auto function = std::make_unique<FunctionDef>();
    function->name = name.text;
    for (const Token& parameter : signature.parameters) {
        function->parameters.push_back(parameter.text);
    }
    function->line = keyword.line;
    function->col = keyword.col;

    AliasMap aliases;
    function->body = parseBlock(BlockContext::FunctionTop, aliases);
    return function;
}

Block Parser::parseBlock(BlockContext context, AliasMap& aliases) {
    Block body;
    if (!match(TokenKind::Indent)) {
        return body;
    }

    while (!check(TokenKind::Dedent)) {
        if (check(TokenKind::EndOfFile)) {
            fail(peek(), "块结束时缺少 DEDENT");
        }
        if (match(TokenKind::Newline)) {
            continue;
        }
        body.push_back(parseStatement(context, aliases));
    }
    advance();
    return body;
}

std::unique_ptr<Stmt> Parser::parseStatement(
    BlockContext context,
    AliasMap& aliases) {
    Token command = peek();
    if (command.kind == TokenKind::Identifier) {
        const auto found = aliases.find(command.text);
        if (found == aliases.end()) {
            fail(command, "未知命令: " + command.text);
        }
        command.kind = found->second;
    }

    switch (command.kind) {
    case TokenKind::Initialize: return parseInitialize(command);
    case TokenKind::Reassign: return parseReassign(command);
    case TokenKind::Free: return parseFree(command);
    case TokenKind::Delete: return parseDelete(command);
    case TokenKind::Automatic: return parseAutomatic(command);
    case TokenKind::Alias: return parseAlias(command, context, aliases);
    case TokenKind::Output: return parseOutput(command);
    case TokenKind::Input: return parseInput(command);
    case TokenKind::Repeat: return parseRepeat(command, aliases);
    case TokenKind::If: return parseIf(command, aliases);
    case TokenKind::Exit: return parseExit(command);
    case TokenKind::Return: return parseReturn(command);
    case TokenKind::Invoke: return parseInvokeStatement(command);
    case TokenKind::Otherwise:
        fail(command, "otherwise 必须紧跟同列 if 的代码块，不能单独出现");
    case TokenKind::Function:
        fail(command, "函数不可嵌套，顶层只允许函数定义");
    default:
        fail(command, "未知命令或无效语句");
    }
}

std::unique_ptr<Stmt> Parser::parseInitialize(const Token& command) {
    advance();
    auto statement = std::make_unique<InitializeStmt>(command.line, command.col);

    while (true) {
        const Token typeToken = peek();
        const TypeKind type = parseType();
        Token name = expectIdentifier("initialize 变量名", true);
        auto value = parseExpression();
        statement->declarations.push_back(
            InitializeDecl{type, name.text, std::move(value), name.line, name.col});

        if (!match(TokenKind::Comma)) {
            break;
        }
        if (check(TokenKind::Newline)) {
            fail(typeToken, "initialize 声明列表不允许尾逗号");
        }
    }
    expectNewline();
    return statement;
}

std::unique_ptr<Stmt> Parser::parseReassign(const Token& command) {
    advance();
    auto statement = std::make_unique<ReassignStmt>(command.line, command.col);
    statement->name = expectIdentifier("reassign 变量名", true).text;
    statement->value = parseExpression();
    expectNewline();
    return statement;
}

std::unique_ptr<Stmt> Parser::parseFree(const Token& command) {
    advance();
    auto statement = std::make_unique<FreeStmt>(command.line, command.col);
    statement->name = expectIdentifier("free 变量名", true).text;
    expectNewline();
    return statement;
}

std::unique_ptr<Stmt> Parser::parseDelete(const Token& command) {
    advance();
    auto statement = std::make_unique<DeleteStmt>(command.line, command.col);
    statement->name = expectIdentifier("delete 变量名", true).text;
    expectNewline();
    return statement;
}

std::unique_ptr<Stmt> Parser::parseAutomatic(const Token& command) {
    advance();
    auto statement = std::make_unique<AutomaticStmt>(command.line, command.col);
    statement->name = expectIdentifier("automatic 变量名", true).text;
    expectNewline();
    return statement;
}

std::unique_ptr<Stmt> Parser::parseAlias(
    const Token& command,
    BlockContext context,
    AliasMap& aliases) {
    if (context != BlockContext::FunctionTop) {
        fail(command, "alias 只允许出现在函数体顶层，不允许位于 if/repeat 内");
    }

    advance();
    auto statement = std::make_unique<AliasStmt>(command.line, command.col);
    Token name = expectIdentifier("alias 新名", true);
    const Token target = peek();

    if (target.kind == TokenKind::Identifier) {
        if (aliases.count(target.text) != 0) {
            fail(target, "不允许链式别名: " + target.text);
        }
        fail(target, "alias 目标必须是内建命令，不能是普通标识符");
    }
    if (!isBuiltinCommand(target.kind)) {
        fail(target, "alias 目标必须是内建命令");
    }
    advance();
    expectNewline();

    statement->name = name.text;
    statement->target = target.kind;
    aliases[name.text] = target.kind;
    return statement;
}

std::unique_ptr<Stmt> Parser::parseOutput(const Token& command) {
    advance();
    auto statement = std::make_unique<OutputStmt>(command.line, command.col);
    if (check(TokenKind::Newline)) {
        fail(peek(), "output 至少需要一个输出表达式");
    }

    while (true) {
        statement->values.push_back(parseExpression());
        if (!match(TokenKind::Comma)) {
            break;
        }
        if (check(TokenKind::Newline)) {
            fail(peek(), "output 参数列表不允许尾逗号");
        }
    }
    expectNewline();
    return statement;
}

std::unique_ptr<Stmt> Parser::parseInput(const Token& command) {
    advance();
    auto statement = std::make_unique<InputStmt>(command.line, command.col);
    statement->name = expectIdentifier("input 变量名", true).text;
    expectNewline();
    return statement;
}

std::unique_ptr<Stmt> Parser::parseRepeat(const Token& command, AliasMap& aliases) {
    advance();
    expectNewline();
    auto statement = std::make_unique<RepeatStmt>(command.line, command.col);
    statement->body = parseBlock(BlockContext::Nested, aliases);
    return statement;
}

std::unique_ptr<Stmt> Parser::parseIf(const Token& command, AliasMap& aliases) {
    advance();
    auto statement = std::make_unique<IfStmt>(command.line, command.col);
    statement->condition = parseExpression();
    expectNewline();
    statement->body = parseBlock(BlockContext::Nested, aliases);

    if (check(TokenKind::Otherwise)) {
        const Token otherwise = advance();
        if (otherwise.col != command.col) {
            fail(otherwise, "otherwise 必须与对应 if 同列");
        }
        expectNewline();
        statement->otherwise = parseBlock(BlockContext::Nested, aliases);
    }
    return statement;
}

std::unique_ptr<Stmt> Parser::parseExit(const Token& command) {
    advance();
    auto statement = std::make_unique<ExitStmt>(command.line, command.col);
    statement->value = parseExpression();
    expectNewline();
    return statement;
}

std::unique_ptr<Stmt> Parser::parseReturn(const Token& command) {
    advance();
    auto statement = std::make_unique<ReturnStmt>(command.line, command.col);
    if (!check(TokenKind::Newline)) {
        statement->value = parseExpression();
    }
    expectNewline();
    return statement;
}

std::unique_ptr<Stmt> Parser::parseInvokeStatement(const Token& command) {
    advance();
    InvokeParts parts = parseInvokeParts();
    if (check(TokenKind::Comma)) {
        arityError(parts.name, signatures_[signatureByName_.at(parts.name.text)].parameters.size());
    }
    expectNewline();

    auto statement = std::make_unique<InvokeStmt>(command.line, command.col);
    statement->name = parts.name.text;
    statement->arguments = std::move(parts.arguments);
    return statement;
}

std::unique_ptr<Expr> Parser::parseExpression() {
    return parseAndOr();
}

std::unique_ptr<Expr> Parser::parseAndOr() {
    auto left = parseNot();
    while (check(TokenKind::And) || check(TokenKind::Or)) {
        const Token op = advance();
        auto right = parseNot();
        left = std::make_unique<Binary>(
            op.line, op.col, op.kind, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseNot() {
    if (check(TokenKind::Not)) {
        const Token op = advance();
        return std::make_unique<Unary>(op.line, op.col, op.kind, parseNot());
    }
    return parseComparison();
}

std::unique_ptr<Expr> Parser::parseComparison() {
    auto left = parseAdditive();
    if (!isComparison(peek().kind)) {
        return left;
    }

    const Token op = advance();
    auto right = parseAdditive();
    if (isComparison(peek().kind)) {
        fail(peek(), "禁止链式比较，请拆分为独立比较表达式");
    }
    return std::make_unique<Binary>(
        op.line, op.col, op.kind, std::move(left), std::move(right));
}

std::unique_ptr<Expr> Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        const Token op = advance();
        auto right = parseMultiplicative();
        left = std::make_unique<Binary>(
            op.line, op.col, op.kind, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseMultiplicative() {
    auto left = parseUnary();
    while (check(TokenKind::Star) || check(TokenKind::Slash)) {
        const Token op = advance();
        auto right = parseUnary();
        left = std::make_unique<Binary>(
            op.line, op.col, op.kind, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (check(TokenKind::Minus)) {
        const Token op = advance();
        return std::make_unique<Unary>(op.line, op.col, op.kind, parseUnary());
    }
    return parsePrimary();
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    const Token token = peek();
    switch (token.kind) {
    case TokenKind::IntegerLiteral:
        advance();
        try {
            return std::make_unique<IntLit>(token.line, token.col, std::stoll(token.text));
        } catch (const std::exception&) {
            fail(token, "整数字面量超出 int64 范围: " + token.text);
        }
    case TokenKind::FloatLiteral:
        advance();
        try {
            return std::make_unique<FloatLit>(token.line, token.col, std::stod(token.text));
        } catch (const std::exception&) {
            fail(token, "浮点数字面量无法表示: " + token.text);
        }
    case TokenKind::StringLiteral:
        advance();
        return std::make_unique<StrLit>(token.line, token.col, token.text);
    case TokenKind::True:
    case TokenKind::False:
        advance();
        return std::make_unique<BoolLit>(token.line, token.col, token.kind == TokenKind::True);
    case TokenKind::Identifier:
        advance();
        return std::make_unique<VarRef>(token.line, token.col, token.text);
    case TokenKind::Invoke: {
        advance();
        InvokeParts parts = parseInvokeParts();
        return std::make_unique<InvokeExpr>(
            token.line,
            token.col,
            parts.name.text,
            std::move(parts.arguments));
    }
    case TokenKind::LeftParen: {
        advance();
        auto expression = parseExpression();
        if (check(TokenKind::Comma)) {
            fail(peek(), "括号内不允许逗号，括号只用于表达式分组");
        }
        expect(TokenKind::RightParen, "缺少右括号 )");
        return expression;
    }
    default:
        fail(token, "预期表达式");
    }
}

Parser::InvokeParts Parser::parseInvokeParts() {
    Token name = expectIdentifier("invoke 目标函数名", false);
    const auto found = signatureByName_.find(name.text);
    if (found == signatureByName_.end()) {
        fail(name, "invoke 调用了未定义函数: " + name.text);
    }
    const std::size_t arity = signatures_[found->second].parameters.size();

    InvokeParts parts{name, {}};
    for (std::size_t i = 0; i < arity; ++i) {
        if (i > 0 && !match(TokenKind::Comma)) {
            arityError(name, arity);
        }
        if (check(TokenKind::Newline) || check(TokenKind::Comma) ||
            check(TokenKind::RightParen) || check(TokenKind::EndOfFile)) {
            arityError(name, arity);
        }
        // 实参为逗号分隔的基本项；运算符（如 +/-）属于外层表达式，分组请用括号。
        parts.arguments.push_back(parseUnary());
    }
    return parts;
}

TypeKind Parser::parseType() {
    const Token token = advance();
    switch (token.kind) {
    case TokenKind::IntegerType: return TypeKind::Integer;
    case TokenKind::FloatType: return TypeKind::Float;
    case TokenKind::BooleanType: return TypeKind::Boolean;
    case TokenKind::StringType: return TypeKind::String;
    case TokenKind::Automatic: return TypeKind::Automatic;
    default: fail(token, "initialize 必须显式给出 integer/float/boolean/string/automatic 类型");
    }
}

bool Parser::isTypeToken(TokenKind kind) const {
    return kind == TokenKind::IntegerType || kind == TokenKind::FloatType ||
           kind == TokenKind::BooleanType || kind == TokenKind::StringType ||
           kind == TokenKind::Automatic;
}

bool Parser::isComparison(TokenKind kind) const {
    return kind == TokenKind::Equal || kind == TokenKind::NotEqual ||
           kind == TokenKind::Less || kind == TokenKind::LessEqual ||
           kind == TokenKind::Greater || kind == TokenKind::GreaterEqual;
}

bool Parser::isBuiltinCommand(TokenKind kind) const {
    switch (kind) {
    case TokenKind::Initialize:
    case TokenKind::Reassign:
    case TokenKind::Free:
    case TokenKind::Delete:
    case TokenKind::Automatic:
    case TokenKind::Alias:
    case TokenKind::Output:
    case TokenKind::Input:
    case TokenKind::Repeat:
    case TokenKind::If:
    case TokenKind::Otherwise:
    case TokenKind::Exit:
    case TokenKind::Return:
    case TokenKind::Invoke:
        return true;
    default:
        return false;
    }
}

void Parser::fail(const Token& token, const std::string& message) const {
    throw LangError(file_, token.line, token.col, message);
}

void Parser::arityError(const Token& name, std::size_t expected) const {
    fail(name,
         "函数 '" + name.text + "' 参数个数不匹配，需要 " +
             std::to_string(expected) + " 个参数");
}
