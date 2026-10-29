# Fluid 解释器（C++17）

Fluid 是一门缩进敏感、显式类型、支持 `automatic` 自动类型的脚本语言，源文件扩展名为 `.fld`。本仓库是其 C++17 解释器。

> 当前版本：**v0.1.2**。

## 构建

```powershell
.\build.ps1            # 构建（-std=c++17 -Wall -Wextra -pedantic）
.\build.ps1 -Debug     # 调试构建（-g -O0）
.\build.ps1 -Sanitize  # 尝试 ASan（-fsanitize=address），不可用时自动回退 -Debug
```

产物：仓库根目录 `fluid.exe`。

## 运行

```powershell
.\fluid.exe run <file.fld>
```

退出码：

| 退出码 | 含义 |
| --- | --- |
| `0` | 正常结束，或 `exit` 主动退出程序 |
| `1` | 词法 / 语法 / 解析期语义 / 运行期错误（诊断写 stderr，格式 `文件:行:列: 错误: 消息`） |
| `2` | CLI 用法错误（缺子命令、非 `run`、缺文件、多余参数、文件不存在） |

示例：

```powershell
.\fluid.exe run examples\hello.fld
.\fluid.exe run examples\demo.fld
.\fluid.exe run examples\loop_and_exit.fld
```

## 测试

```powershell
.\run_tests.ps1
# 或
python tests\run_tests.py
# 指定解释器 / 过滤用例
python tests\run_tests.py --exe .\fluid.exe --filter input
```

用例位于 `tests/cases/<name>/`：

| 文件 | 含义 |
| --- | --- |
| `main.fld` | 必填，被测程序 |
| `input.txt` | 可选，stdin |
| `expected.out` | 可选，stdout 精确匹配（统一 CRLF/LF，忽略 BOM） |
| `expected.err_contains` | 可选，每一非空行都必须作为子串出现在 stderr 的某一行 |
| `expected.exit` | 必填，期望退出码 |
| `expected.warning` | 可选，存在即要求 stderr 至少含一条 `: warning:` |

- stdout 精确比较；`expected.err_contains` 的每个非空行须作为子串出现在 stderr 某一行（不要求顺序）；单例超时 10 秒。
- 错误用例（`expected.exit` = 1）额外要求 stderr 至少一行匹配 `文件:行:列: 错误: `。
- warning 用例要求 stderr 至少一行匹配 `文件:行:列: warning: `，且 stdout 精确匹配、退出码 0。
- 当前共 **49** 个用例：正例 19、错误例 28、warning 例 2。

## 已实现

- 缩进敏感词法：空格缩进、统一缩进单位、INDENT/DEDENT、tab 报错
- `\note ... \note` 跨行注释（字符串内不生效）；裸 `\n` 换行字符串
- 类型：`integer` / `float` / `boolean` / `string` / `automatic`
- 变量状态机：`initialize` / `reassign` / `free` / `delete` / `automatic`
- 语句：`output` / `input` / `repeat` / `if` / `otherwise` / `exit` / `return` / `invoke` / `alias`
- 函数：签名预扫描、前向引用、参数个数解析期校验、名称冲突检查（参数 / 变量 / 别名）
- 表达式优先级：`and`/`or`（同级左结合，短路）→ `not`（作用于比较）→ 比较（禁止链式）→ `+ -` → `* /` → 一元 `-` → 基本项
- 整数 `+ - *` 溢出检查；`/` 恒返回 double 且除零报错；浮点 inf 报错
- 字符串拼接：`+` 支持 `string + string`（如 `"a" + "b"` = `"ab"`）；`string` 与数值混合（`string + integer` / `integer + string`）报类型不匹配错误；`automatic` 变量当前存字符串时同样适用
- 浮点转整数须小数部分为 0 且在 int64 范围；字符串与数值比较报错；`string → boolean` 报错
- `exit N` 层级语义（函数体计 1 层，每个外层 `if`/`repeat` 加 1 层）
- 浮点格式化：最短可回读表示，整数值补 `.0`

## 未实现

REPL、格式化器、LSP、科学计数法、十六进制、`else if`、参数类型标注、链式别名。

## 已知限制

- 空 `repeat` 或没有退出路径的 `repeat` 会无限循环；不做运行期检测，请自行保证循环可终止。
- 函数参数以逗号分隔，例如 `function add a, b`。
- `invoke` 的实参是逗号分隔的基本项；实参内部若需 `+ -` 等运算请用括号分组，例如 `invoke fib (n - 1)`。裸写 `invoke fib n - 1` 会被解析为 `(invoke fib n) - 1`。
- `alias 原命令 新名`：第一个词是被替换的内建命令（如 `initialize` / `output`），第二个词是新别名，例如 `alias output say` 让 `say` 成为 `output` 的别名；`alias` 只允许出现在函数体顶层，且不允许链式（别名不能再被别名）。
- 变量声明初值可省略：`initialize integer a` 或 `initialize automatic b` 均合法，省略初值时变量处于「无值状态」，使用前必须先 `reassign` / `input` 赋值，否则报「尚未赋值」。

## 语义决策摘要

- 平台：Windows，仅 C++17。
- 浮点转整数：小数部分必须为 0，否则报错（不截断）。
- 数值溢出：浮点转整数越界、整数 `+ - *` 越界、浮点上溢（inf）均报错。
- 比较：类型不匹配一律报错；数值可混合（提升 double）；字符串只与字符串；布尔只与布尔。
- `invoke` 参数个数在解析期检查（含前向引用）。
- 空块：控制语句后紧跟同列语句视为空块。
- CLI：`fluid.exe run <file.fld>`。
- 作用域以函数为界；`if` / `repeat` 不新建作用域。
- 运行时错误同样带 `文件:行:列`，写 stderr，退出码 1。
- warning 写 stderr，不影响退出码。
