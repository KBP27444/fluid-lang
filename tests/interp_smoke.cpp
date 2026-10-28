#include "environment.h"
#include "error.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include "source.h"
#include "value.h"

#include <cassert>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

struct RunResult {
    int exitCode = 0;
    std::string output;
    std::string error;
    bool crashed = false;
};

RunResult run(const std::string& src, const std::string& input = "") {
    RunResult result;
    std::ostringstream out;
    std::istringstream in(input);
    try {
        Source source{"<test>", src};
        Lexer lexer(source);
        Parser parser(source.file, lexer.tokenize());
        Program program = parser.parse();
        Interpreter interp("<test>", program, out, in);
        result.exitCode = interp.run();
    } catch (const LangError& error) {
        result.error = error.what();
        result.exitCode = 1;
    } catch (const std::exception& error) {
        result.error = error.what();
        result.crashed = true;
        result.exitCode = 2;
    } catch (...) {
        result.error = "unknown exception";
        result.crashed = true;
        result.exitCode = 2;
    }
    result.output = out.str();
    return result;
}

int passCount = 0;
int failCount = 0;

void check(const std::string& name, bool ok, const RunResult& r = RunResult{}) {
    if (ok) {
        ++passCount;
        std::cout << "[PASS] " << name << '\n';
    } else {
        ++failCount;
        std::cout << "[FAIL] " << name << '\n';
        std::cout << "    exit=" << r.exitCode
                  << " crashed=" << (r.crashed ? "yes" : "no") << '\n';
        std::cout << "    out=[" << r.output << "]\n";
        std::cout << "    err=[" << r.error << "]\n";
    }
    std::cout.flush();
}

int main() {
    std::cout << std::unitbuf;
    // 1. 基础输出与浮点格式化补 .0（output 无分隔符）
    {
        auto r = run("function main\n  output 1.0, 0.1, 1.1\n");
        check("float 格式化 1.0,0.1,1.1", r.exitCode == 0 && r.output == "1.00.11.1", r);
    }

    // 2. 整数除法返回 double
    {
        auto r = run("function main\n  output 1 / 2, 4.0 / 2.0\n");
        check("除法返回 double 0.5 与 2.0", r.exitCode == 0 && r.output == "0.52.0", r);
    }

    // 3. 变量状态机：free 后使用报错，reassign 可赋值
    {
        auto r = run("function main\n  initialize integer a 10\n  free a\n  output a\n");
        check("free 后使用 -> 错误", r.exitCode == 1 && r.error.find("尚未赋值") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize integer a 10\n  free a\n  reassign a 2\n  output a\n");
        check("free 后 reassign 可赋值", r.exitCode == 0 && r.output == "2", r);
    }

    // 4. delete 后 reassign 报错；delete 未声明报错
    {
        auto r = run("function main\n  initialize integer a 10\n  delete a\n  reassign a 5\n");
        check("delete 后 reassign -> 错误", r.exitCode == 1 && r.error.find("未声明") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  delete ghost\n");
        check("delete 未声明 -> 错误", r.exitCode == 1 && r.error.find("未声明") != std::string::npos, r);
    }

    // 5. automatic 切换模式并保留值
    {
        auto r = run("function main\n  initialize integer a 10\n  automatic a\n  reassign a 5.5\n  output a\n");
        check("automatic 切换为 auto 并保留值", r.exitCode == 0 && r.output == "5.5", r);
    }
    {
        auto r = run("function main\n  automatic a\n");
        check("automatic 未声明变量 -> 错误", r.exitCode == 1 && r.error.find("未声明") != std::string::npos, r);
    }

    // 6. 类型转换：浮点->整数要求整数值；字符串->数值报错
    {
        auto r = run("function main\n  initialize integer a 1.5\n");
        check("float 1.5 -> integer 报错", r.exitCode == 1 && r.error.find("integer") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize float a \"x\"\n");
        check("string -> float 报错", r.exitCode == 1 && r.error.find("float") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize integer a 5.0\n  output a\n");
        check("float 5.0 -> integer 合法", r.exitCode == 0 && r.output == "5", r);
    }

    // 7. 除零报错
    {
        auto r = run("function main\n  output 1 / 0\n");
        check("整数除零报错", r.exitCode == 1 && r.error.find("0") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  output 1.0 / 0.0\n");
        check("浮点除零报错", r.exitCode == 1, r);
    }

    // 8. 字符串与数值比较报错
    {
        auto r = run("function main\n  output \"a\" = 1\n");
        check("字符串与数值比较报错", r.exitCode == 1 && r.error.find("字符串") != std::string::npos, r);
    }

    // 9. not 优先级与比较链（解析期）
    {
        auto r = run("function main\n  if not true\n    output \"y\"\n  otherwise\n    output \"n\"\n");
        check("not true -> otherwise n", r.exitCode == 0 && r.output == "n", r);
    }
    {
        auto r = run("function main\n  output 1 < 2 < 3\n");
        check("比较链 1 < 2 < 3 解析报错", r.exitCode == 1 && r.error.find("链式") != std::string::npos, r);
    }

    // 10. if / otherwise
    {
        auto r = run("function main\n  if false\n    output \"y\"\n  otherwise\n    output \"n\"\n");
        check("if false -> n", r.exitCode == 0 && r.output == "n", r);
    }

    // 11. invoke 作为表达式（返回值）与语句（丢弃返回值）
    {
        auto r = run("function add a, b\n  return a + b\nfunction main\n  output invoke add 2, 3\n");
        check("invoke 表达式返回 5", r.exitCode == 0 && r.output == "5", r);
    }
    {
        auto r = run("function add a, b\n  return a + b\nfunction main\n  invoke add 2, 3\n  output \"y\"\n");
        check("invoke 语句丢弃返回值", r.exitCode == 0 && r.output == "y", r);
    }
    {
        auto r = run("function nothing\n  return\nfunction main\n  output invoke nothing\n");
        check("void 函数作表达式报错", r.exitCode == 1 && r.error.find("返回值") != std::string::npos, r);
    }

    // 12. exit 层级语义
    {
        auto r = run(
            "function main\n"
            "  initialize integer x 0\n"
            "  repeat\n"
            "    if x = 5\n"
            "      exit 2\n"
            "    reassign x x + 1\n"
            "  output \"done\"\n");
        check("exit 2 跳出 repeat+if 两级后继续", r.exitCode == 0 && r.output == "done", r);
    }
    {
        auto r = run(
            "function main\n"
            "  repeat\n"
            "    exit 1\n"
            "  output \"inside\"\n"
            "  output \"done\"\n");
        check("exit 1 跳出 repeat 后继续外层", r.exitCode == 0 && r.output == "insidedone", r);
    }
    {
        auto r = run(
            "function helper\n"
            "  exit 1\n"
            "function main\n"
            "  invoke helper\n"
            "  output \"after\"\n");
        check("exit N==层级 返回调用者", r.exitCode == 0 && r.output == "after", r);
    }
    {
        auto r = run(
            "function main\n"
            "  exit 5\n"
            "  output \"after\"\n");
        check("exit N>层级 退出程序", r.exitCode == 0 && r.output.empty(), r);
    }
    {
        auto r = run(
            "function main\n"
            "  exit 0\n"
            "  output \"continued\"\n");
        check("exit 0 继续执行", r.exitCode == 0 && r.output == "continued", r);
    }

    // 13. 嵌套函数返回与递归（实参分组用括号）
    {
        auto r = run(
            "function fib n\n"
            "  if n < 2\n"
            "    return n\n"
            "  return invoke fib (n - 1) + invoke fib (n - 2)\n"
            "function main\n"
            "  output invoke fib 10\n");
        check("递归 fib(10) = 55", r.exitCode == 0 && r.output == "55", r);
    }

    // 14. input：automatic 与 typed（变量必须先声明）
    {
        auto r = run("function main\n  initialize automatic a 0\n  input a\n  output a\n", "5");
        check("automatic input 整数", r.exitCode == 0 && r.output == "5", r);
    }
    {
        auto r = run("function main\n  initialize automatic a 0\n  input a\n  output a\n", "3.5");
        check("automatic input 浮点", r.exitCode == 0 && r.output == "3.5", r);
    }
    {
        auto r = run("function main\n  initialize automatic a 0\n  input a\n  output a\n", "hello");
        check("automatic input 字符串", r.exitCode == 0 && r.output == "hello", r);
    }
    {
        auto r = run("function main\n  initialize integer a 0\n  input a\n  output a\n", "42");
        check("typed integer input", r.exitCode == 0 && r.output == "42", r);
    }
    {
        auto r = run("function main\n  initialize integer a 0\n  input a\n  output a\n", "abc");
        check("typed integer input 非法 -> 错误", r.exitCode == 1, r);
    }
    {
        auto r = run("function main\n  initialize boolean b false\n  input b\n  output b\n", "true");
        check("typed boolean input true", r.exitCode == 0 && r.output == "true", r);
    }

    std::cout << "\n==== interp_smoke 汇总: PASS=" << passCount << " FAIL=" << failCount
              << " ====\n";
    std::cout.flush();
    return failCount == 0 ? 0 : 1;
}
