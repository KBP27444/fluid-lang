#include "interpreter.h"

#include "error.h"
#include "token.h"
#include "value.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

// 进入/离开 if/repeat 块时维护嵌套层数（异常路径亦正确）
struct BlockDepthGuard {
    explicit BlockDepthGuard(int* depth) : depth_(depth) { ++(*depth_); }
    ~BlockDepthGuard() { --(*depth_); }
    int* depth_;
};

// 函数帧的 RAII 守卫
struct FrameGuard {
    explicit FrameGuard(Environment* env) : env_(env) {}
    ~FrameGuard() { env_->popFrame(); }
    Environment* env_;
};

} // namespace

Interpreter::Interpreter(
    std::string file,
    const Program& program,
    std::ostream& out,
    std::istream& in)
    : file_(std::move(file)), out_(out), in_(in) {
    for (const auto& function : program.functions) {
        env_.addFunction(function->name, function.get());
    }
}

int Interpreter::run() {
    try {
        const FunctionDef* mainFunction = env_.findFunction("main");
        if (mainFunction == nullptr) {
            // 解析期已保证存在，这里仅作防御
            throw LangError(file_, 1, 1, "程序缺少 main 函数");
        }
        (void)callFunction("main", {}, 0, 0);
        return 0;
    } catch (const ProgramExit&) {
        return 0;
    } catch (const ExitSignal&) {
        // 防御：不应到达顶层
        return 0;
    } catch (const LangError& error) {
        // 运行期错误在底层以空文件名抛出，这里统一补上源文件名，
        // 保证诊断格式始终为 文件:行:列: 错误: ...
        if (error.file.empty()) {
            throw LangError(file_, error.line, error.col, error.message);
        }
        throw;
    }
}

Value Interpreter::eval(const Expr* expr) {
    if (const auto* node = dynamic_cast<const IntLit*>(expr)) {
        return node->value;
    }
    if (const auto* node = dynamic_cast<const FloatLit*>(expr)) {
        return node->value;
    }
    if (const auto* node = dynamic_cast<const StrLit*>(expr)) {
        return node->value;
    }
    if (const auto* node = dynamic_cast<const BoolLit*>(expr)) {
        return node->value;
    }
    if (const auto* node = dynamic_cast<const VarRef*>(expr)) {
        Variable* var = env_.find(node->name);
        if (var == nullptr) {
            throw LangError("", node->line, node->col, "变量 '" + node->name + "' 未声明");
        }
        if (!var->hasValue) {
            throw LangError("", node->line, node->col,
                "变量 '" + node->name + "' 尚未赋值（处于无值状态）");
        }
        return var->value;
    }
    if (const auto* node = dynamic_cast<const Unary*>(expr)) {
        Value operand = eval(node->operand.get());
        return applyUnary(node->op, operand, node->line, node->col);
    }
    if (const auto* node = dynamic_cast<const Binary*>(expr)) {
        if (node->op == TokenKind::And) {
            Value left = eval(node->left.get());
            if (!isBoolean(left)) {
                throw LangError("", node->line, node->col, "and 要求布尔操作数");
            }
            if (!asBoolean(left)) return false; // 短路
            Value right = eval(node->right.get());
            if (!isBoolean(right)) {
                throw LangError("", node->line, node->col, "and 要求布尔操作数");
            }
            return asBoolean(right);
        }
        if (node->op == TokenKind::Or) {
            Value left = eval(node->left.get());
            if (!isBoolean(left)) {
                throw LangError("", node->line, node->col, "or 要求布尔操作数");
            }
            if (asBoolean(left)) return true; // 短路
            Value right = eval(node->right.get());
            if (!isBoolean(right)) {
                throw LangError("", node->line, node->col, "or 要求布尔操作数");
            }
            return asBoolean(right);
        }
        Value left = eval(node->left.get());
        Value right = eval(node->right.get());
        return applyBinary(node->op, left, right, node->line, node->col);
    }
    if (const auto* node = dynamic_cast<const InvokeExpr*>(expr)) {
        std::vector<Value> arguments;
        arguments.reserve(node->arguments.size());
        for (const auto& argument : node->arguments) {
            arguments.push_back(eval(argument.get()));
        }
        ReturnSignal result = callFunction(node->name, std::move(arguments), node->line, node->col);
        if (!result.hasValue) {
            throw LangError("", node->line, node->col,
                "函数 '" + node->name + "' 作为表达式调用时必须实际返回返回值");
        }
        return result.value;
    }
    throw LangError("", expr->line, expr->col, "无法求值的表达式");
}

void Interpreter::execute(const Stmt* stmt) {
    if (const auto* node = dynamic_cast<const InitializeStmt*>(stmt)) {
        for (const auto& decl : node->declarations) {
            Value raw = eval(decl.value.get());
            if (decl.type == TypeKind::Automatic) {
                env_.declare(decl.name, TypeKind::Automatic, true, raw);
            } else {
                Value converted = convertTo(raw, decl.type, decl.line, decl.col);
                env_.declare(decl.name, decl.type, true, converted);
            }
        }
        return;
    }
    if (const auto* node = dynamic_cast<const ReassignStmt*>(stmt)) {
        Variable& var = env_.require(node->name, node->line, node->col);
        Value raw = eval(node->value.get());
        if (var.declaredType == TypeKind::Automatic) {
            var.value = raw;
        } else {
            var.value = convertTo(raw, var.declaredType, node->line, node->col);
        }
        var.hasValue = true;
        return;
    }
    if (const auto* node = dynamic_cast<const FreeStmt*>(stmt)) {
        Variable& var = env_.require(node->name, node->line, node->col);
        if (!var.hasValue) {
            throw LangError("", node->line, node->col,
                "变量 '" + node->name + "' 当前没有值，无法 free");
        }
        var.hasValue = false;
        return;
    }
    if (const auto* node = dynamic_cast<const DeleteStmt*>(stmt)) {
        Variable* var = env_.find(node->name);
        if (var == nullptr) {
            throw LangError("", node->line, node->col,
                "变量 '" + node->name + "' 未声明，无法 delete");
        }
        env_.remove(node->name);
        return;
    }
    if (const auto* node = dynamic_cast<const AutomaticStmt*>(stmt)) {
        Variable& var = env_.require(node->name, node->line, node->col);
        var.declaredType = TypeKind::Automatic;
        return;
    }
    if (dynamic_cast<const AliasStmt*>(stmt)) {
        // 别名在解析期已解析为目标内建命令，运行期无需动作
        return;
    }
    if (const auto* node = dynamic_cast<const OutputStmt*>(stmt)) {
        for (const auto& value : node->values) {
            out_ << formatValue(eval(value.get()));
        }
        return;
    }
    if (const auto* node = dynamic_cast<const InputStmt*>(stmt)) {
        Variable& var = env_.require(node->name, node->line, node->col);
        std::string line;
        if (!std::getline(in_, line)) {
            throw LangError("", node->line, node->col, "input 到达文件末尾（EOF）");
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (var.declaredType == TypeKind::Automatic) {
            var.value = parseAutomaticInput(line);
        } else {
            var.value = parseTypedInput(line, var.declaredType, node->line, node->col);
        }
        var.hasValue = true;
        return;
    }
    if (const auto* node = dynamic_cast<const RepeatStmt*>(stmt)) {
        executeRepeat(*node);
        return;
    }
    if (const auto* node = dynamic_cast<const IfStmt*>(stmt)) {
        Value condition = eval(node->condition.get());
        if (!isBoolean(condition)) {
            throw LangError("", node->line, node->col, "if 条件必须是布尔类型");
        }
        if (asBoolean(condition)) {
            executeBlock(node->body);
        } else {
            executeBlock(node->otherwise);
        }
        return;
    }
    if (const auto* node = dynamic_cast<const ExitStmt*>(stmt)) {
        Value raw = eval(node->value.get());
        std::int64_t n = 0;
        if (isInteger(raw)) {
            n = asInteger(raw);
        } else if (isFloat(raw)) {
            double d = asFloat(raw);
            if (d < 0.0 || !std::isfinite(d) || std::trunc(d) != d) {
                throw LangError("", node->line, node->col,
                    "exit 的参数必须是非负整数或整数值浮点数");
            }
            n = static_cast<std::int64_t>(d);
        } else {
            throw LangError("", node->line, node->col,
                "exit 的参数必须是整数或整数值浮点数");
        }
        if (n < 0) {
            throw LangError("", node->line, node->col, "exit 的参数必须是非负整数");
        }
        if (n == 0) {
            return; // exit 0：继续执行下一条
        }
        int levels = 1 + blockDepth_;
        if (n > levels) {
            throw ProgramExit{};
        }
        if (n == levels) {
            throw ReturnSignal{false, {}};
        }
        throw ExitSignal{static_cast<int>(n)};
    }
    if (const auto* node = dynamic_cast<const ReturnStmt*>(stmt)) {
        if (node->value) {
            Value value = eval(node->value.get());
            throw ReturnSignal{true, value};
        }
        throw ReturnSignal{false, {}};
    }
    if (const auto* node = dynamic_cast<const InvokeStmt*>(stmt)) {
        std::vector<Value> arguments;
        arguments.reserve(node->arguments.size());
        for (const auto& argument : node->arguments) {
            arguments.push_back(eval(argument.get()));
        }
        (void)callFunction(node->name, std::move(arguments), node->line, node->col);
        return;
    }
    throw LangError("", stmt->line, stmt->col, "无法执行的语句");
}

void Interpreter::executeBlock(const Block& body) {
    BlockDepthGuard guard(&blockDepth_);
    for (const auto& statement : body) {
        try {
            execute(statement.get());
        } catch (ExitSignal& signal) {
            if (signal.levels > 1) {
                signal.levels -= 1;
                throw;
            }
            return; // 跳出本块，回到外层序列继续执行
        }
    }
}

void Interpreter::executeRepeat(const RepeatStmt& stmt) {
    while (true) {
        bool broke = false;
        {
            BlockDepthGuard guard(&blockDepth_);
            try {
                for (const auto& statement : stmt.body) {
                    execute(statement.get());
                }
            } catch (ExitSignal& signal) {
                if (signal.levels > 1) {
                    signal.levels -= 1;
                    throw;
                }
                broke = true;
            }
        }
        if (broke) break;
    }
}

ReturnSignal Interpreter::callFunction(
    const std::string& name,
    std::vector<Value> arguments,
    int line,
    int col) {
    const FunctionDef* function = env_.findFunction(name);
    if (function == nullptr) {
        throw LangError("", line, col, "调用了未定义函数: " + name);
    }

    env_.pushFrame();
    FrameGuard guard(&env_);
    for (std::size_t i = 0; i < function->parameters.size(); ++i) {
        env_.declare(function->parameters[i], TypeKind::Automatic, true, arguments[i]);
    }

    try {
        for (const auto& statement : function->body) {
            execute(statement.get());
        }
    } catch (const ReturnSignal& returned) {
        return returned;
    }
    return ReturnSignal{false, {}};
}

Value Interpreter::parseTypedInput(const std::string& line, TypeKind type, int lline, int col) {
    switch (type) {
    case TypeKind::Integer: {
        if (line.empty()) {
            throw LangError("", lline, col, "空行无法解析为 integer");
        }
        try {
            std::size_t processed = 0;
            std::int64_t value = std::stoll(line, &processed);
            if (processed != line.size()) {
                throw LangError("", lline, col, "无法将输入解析为 integer: " + line);
            }
            return value;
        } catch (const std::exception&) {
            throw LangError("", lline, col, "无法将输入解析为 integer: " + line);
        }
    }
    case TypeKind::Float: {
        if (line.empty()) {
            throw LangError("", lline, col, "空行无法解析为 float");
        }
        try {
            std::size_t processed = 0;
            double value = std::stod(line, &processed);
            if (processed != line.size()) {
                throw LangError("", lline, col, "无法将输入解析为 float: " + line);
            }
            return value;
        } catch (const std::exception&) {
            throw LangError("", lline, col, "无法将输入解析为 float: " + line);
        }
    }
    case TypeKind::Boolean: {
        if (line == "true") return true;
        if (line == "false") return false;
        if (line.empty()) {
            throw LangError("", lline, col, "空行无法解析为 boolean");
        }
        throw LangError("", lline, col, "boolean 只接受 true 或 false，得到: " + line);
    }
    case TypeKind::String:
        return line;
    case TypeKind::Automatic:
        return line;
    }
    return line;
}

Value Interpreter::parseAutomaticInput(const std::string& line) {
    if (line.empty()) {
        return std::string(""); // 空行得到空字符串
    }
    std::string trimmed = trim(line);
    if (trimmed.empty()) {
        return std::string("");
    }
    try {
        std::size_t processed = 0;
        std::int64_t integer = std::stoll(trimmed, &processed);
        if (processed == trimmed.size()) {
            return integer;
        }
    } catch (const std::exception&) {
        // 继续尝试 float
    }
    try {
        std::size_t processed = 0;
        double floating = std::stod(trimmed, &processed);
        if (processed == trimmed.size()) {
            return floating;
        }
    } catch (const std::exception&) {
        // 回退到字符串
    }
    return line; // 保留原始行内容
}
