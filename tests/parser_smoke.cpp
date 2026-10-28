#include "../src/ast.h"
#include "../src/error.h"
#include "../src/lexer.h"
#include "../src/parser.h"

#include <cassert>
#include <string>

namespace {

Program parseText(const std::string& text) {
    Source source{"parser_smoke.fld", text};
    Lexer lexer(source);
    Parser parser(source.file, lexer.tokenize());
    return parser.parse();
}

void expectError(const std::string& text, const std::string& fragment) {
    try {
        (void)parseText(text);
    } catch (const LangError& error) {
        assert(std::string(error.what()).find(fragment) != std::string::npos);
        return;
    }
    assert(false && "expected LangError");
}

} // namespace

int main() {
    Program program = parseText(R"LANG(function add a, b
  return a
function main
  initialize integer c 3
  output invoke add 1, 2, c
  if not 1 = 2
    output "yes"
)LANG");

    assert(program.functions.size() == 2);
    const FunctionDef& mainFunction = *program.functions[1];
    assert(mainFunction.body.size() == 3);

    const auto& output = dynamic_cast<const OutputStmt&>(*mainFunction.body[1]);
    assert(output.values.size() == 2);
    const auto& invocation = dynamic_cast<const InvokeExpr&>(*output.values[0]);
    assert(invocation.name == "add");
    assert(invocation.arguments.size() == 2);
    const auto& thirdOutput = dynamic_cast<const VarRef&>(*output.values[1]);
    assert(thirdOutput.name == "c");

    const auto& ifStatement = dynamic_cast<const IfStmt&>(*mainFunction.body[2]);
    const auto& notExpression = dynamic_cast<const Unary&>(*ifStatement.condition);
    assert(notExpression.op == TokenKind::Not);
    const auto& comparison = dynamic_cast<const Binary&>(*notExpression.operand);
    assert(comparison.op == TokenKind::Equal);

    expectError(R"LANG(function main
  output 1 < 2 < 3
)LANG", "链式比较");

    expectError(R"LANG(function main
  alias value output
  initialize integer value 1
)LANG", "冲突");

    expectError(R"LANG(function main
  initialize integer value 1
  alias value output
)LANG", "冲突");

    expectError(R"LANG(function main
  output invoke add 1
function add a, b
  return a
)LANG", "参数个数不匹配");

    expectError(R"LANG(function main
  invoke add 1, 2, 3
function add a, b
  return a
)LANG", "参数个数不匹配");

    Program statements = parseText(R"LANG(function helper p
  alias show output
  show p
  return
function main
  initialize automatic a 1, string s "x"
  reassign a 2
  free a
  automatic a
  input s
  repeat
    exit 1
  if true
    return 1
  otherwise
    invoke helper "x"
  exit 0
)LANG");
    assert(statements.functions.size() == 2);
    assert(statements.functions[1]->body.size() == 8);

    Program emptyBlocks = parseText(R"LANG(function empty
function main
  if true
  otherwise
  repeat
  exit 2
)LANG");
    assert(emptyBlocks.functions.size() == 2);
    assert(emptyBlocks.functions[0]->body.empty());

    expectError(R"LANG(function main
  if true
    alias show output
)LANG", "函数体顶层");

    return 0;
}
