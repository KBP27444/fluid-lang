#include "environment.h"

#include "error.h"

void Environment::addFunction(const std::string& name, const FunctionDef* function) {
    functions_.emplace(name, function);
}

const FunctionDef* Environment::findFunction(const std::string& name) const {
    auto found = functions_.find(name);
    return found == functions_.end() ? nullptr : found->second;
}

void Environment::pushFrame() {
    frames_.emplace_back();
}

void Environment::popFrame() {
    frames_.pop_back();
}

Frame& Environment::current() {
    return frames_.back();
}

const Frame& Environment::current() const {
    return frames_.back();
}

Variable* Environment::find(const std::string& name) {
    auto found = frames_.back().find(name);
    return found == frames_.back().end() ? nullptr : &found->second;
}

const Variable* Environment::find(const std::string& name) const {
    const auto& frame = frames_.back();
    auto found = frame.find(name);
    return found == frame.end() ? nullptr : &found->second;
}

Variable& Environment::require(const std::string& name, int line, int col) {
    Variable* var = find(name);
    if (var == nullptr) {
        throw LangError("", line, col, "变量 '" + name + "' 未声明");
    }
    return *var;
}

void Environment::declare(
    const std::string& name,
    TypeKind type,
    bool hasValue,
    const Value& value) {
    Variable var;
    var.declaredType = type;
    var.hasValue = hasValue;
    var.value = value;
    frames_.back()[name] = var;
}

void Environment::remove(const std::string& name) {
    frames_.back().erase(name);
}
