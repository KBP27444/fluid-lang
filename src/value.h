#ifndef LANG_VALUE_H
#define LANG_VALUE_H

#include "ast.h"
#include "error.h"
#include "token.h"

#include <cstdint>
#include <string>
#include <variant>

// 语言值：四种底层类型
using Value = std::variant<std::int64_t, double, std::string, bool>;

bool isInteger(const Value& value);
bool isFloat(const Value& value);
bool isNumeric(const Value& value);
bool isString(const Value& value);
bool isBoolean(const Value& value);

std::int64_t asInteger(const Value& value);
double asFloat(const Value& value);
bool asBoolean(const Value& value);
const std::string& asString(const Value& value);

// 把源值转换为声明类型（manual 类型）。失败时抛出带位置的 LangError。
Value convertTo(const Value& value, TypeKind target, int line, int col);

// 二元运算：算术 / 比较 / 逻辑。失败时抛出 LangError。
Value applyBinary(TokenKind op, const Value& left, const Value& right, int line, int col);

// 一元运算：负号 / not。失败时抛出 LangError。
Value applyUnary(TokenKind op, const Value& operand, int line, int col);

// 最短可回读格式化；整数值浮点补 .0（如 1.0）。
std::string formatValue(const Value& value);

#endif
