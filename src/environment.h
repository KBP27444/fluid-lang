#ifndef LANG_ENVIRONMENT_H
#define LANG_ENVIRONMENT_H

#include "ast.h"
#include "value.h"

#include <string>
#include <unordered_map>
#include <vector>

// 单个变量：声明模式（manual 类型或 Automatic）、是否有值、当前值
struct Variable {
    TypeKind declaredType = TypeKind::Automatic;
    bool hasValue = false;
    Value value{};
};

using Frame = std::unordered_map<std::string, Variable>;

class Environment {
public:
    void addFunction(const std::string& name, const FunctionDef* function);
    const FunctionDef* findFunction(const std::string& name) const;

    void pushFrame();
    void popFrame();

    // 当前（最内层）函数帧
    Frame& current();
    const Frame& current() const;

    // 当前帧中查找变量；未声明返回 nullptr
    Variable* find(const std::string& name);
    const Variable* find(const std::string& name) const;

    // 要求变量已声明，否则抛出 LangError
    Variable& require(const std::string& name, int line, int col);

    // 在当前帧中声明/覆盖一个变量
    void declare(const std::string& name, TypeKind type, bool hasValue, const Value& value);

    // 从当前帧移除变量（delete）
    void remove(const std::string& name);

private:
    std::unordered_map<std::string, const FunctionDef*> functions_;
    std::vector<Frame> frames_;
};

#endif
