#!/usr/bin/env python3
"""按 R3 §6.1 运行 tests/cases 下的全部 .fld 用例。

每个用例目录包含：
  main.fld              必填，被测程序
  input.txt              可选，stdin
  expected.out           可选，stdout 精确匹配（统一 CRLF/LF，忽略 BOM）
  expected.err_contains  可选，每一非空行都必须作为子串出现在 stderr 的某一行
  expected.exit          必填，期望退出码
  expected.warning       可选，存在即要求 stderr 至少含一条 ": warning: "

用法：
  python tests/run_tests.py
  python tests/run_tests.py --exe path/to/fluid.exe
  python tests/run_tests.py --filter input
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TESTS_DIR.parent
CASES_DIR = TESTS_DIR / "cases"
TIMEOUT_SECONDS = 10

ERROR_PREFIX = re.compile(r"^.+:\d+:\d+: 错误: ")
WARNING_PREFIX = re.compile(r"^.+:\d+:\d+: warning: ")


def read_text(path: Path) -> str:
    """按 UTF-8-SIG 读取（自动去 BOM），无法解码的字节以替换字符呈现。"""
    return path.read_text(encoding="utf-8-sig", errors="replace")


def normalize_newlines(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def load_expected(case_dir: Path) -> dict:
    exit_path = case_dir / "expected.exit"
    if not exit_path.exists():
        raise FileNotFoundError(f"缺少 expected.exit: {case_dir}")

    out_path = case_dir / "expected.out"
    err_path = case_dir / "expected.err_contains"
    input_path = case_dir / "input.txt"

    err_needles: list[str] = []
    if err_path.exists():
        for line in normalize_newlines(read_text(err_path)).split("\n"):
            if line.strip():
                err_needles.append(line.rstrip("\n"))

    return {
        "exit": int(read_text(exit_path).strip()),
        "out": normalize_newlines(read_text(out_path)) if out_path.exists() else None,
        "err_contains": err_needles,
        "warning": (case_dir / "expected.warning").exists(),
        "input": read_text(input_path) if input_path.exists() else "",
    }


def compare_case(case_dir: Path, stdout: str, stderr: str, exit_code: int) -> tuple[bool, list[str]]:
    """比对单个用例的捕获结果，返回 (是否通过, 失败原因列表)。"""
    expected = load_expected(case_dir)
    stdout = normalize_newlines(stdout)
    stderr = normalize_newlines(stderr)
    stderr_lines = stderr.split("\n")

    problems: list[str] = []

    if exit_code != expected["exit"]:
        problems.append(f"退出码：期望 {expected['exit']}，实际 {exit_code}")

    if expected["out"] is not None and stdout != expected["out"]:
        problems.append(f"stdout 不匹配：期望 {expected['out']!r}，实际 {stdout!r}")

    for needle in expected["err_contains"]:
        if not any(needle in line for line in stderr_lines):
            problems.append(f"stderr 缺少关键字：{needle!r}")

    # 错误用例：退出码 1 且 stderr 至少一行符合 file:line:col: 错误: 前缀
    if expected["exit"] == 1:
        if not any(ERROR_PREFIX.match(line) for line in stderr_lines):
            problems.append("stderr 缺少 '文件:行:列: 错误: ' 格式的行")

    # warning 用例：stderr 至少一行符合 file:line:col: warning: 前缀
    if expected["warning"]:
        if not any(WARNING_PREFIX.match(line) for line in stderr_lines):
            problems.append("stderr 缺少 '文件:行:列: warning: ' 格式的行")

    return (len(problems) == 0, problems)


def discover_cases() -> list[Path]:
    if not CASES_DIR.is_dir():
        return []
    return sorted(p for p in CASES_DIR.iterdir() if p.is_dir())


def run_case(case_dir: Path, exe_path: Path) -> tuple[bool, list[str]]:
    main_lang = case_dir / "main.fld"
    if not main_lang.exists():
        return (False, ["缺少 main.fld"])

    expected = load_expected(case_dir)
    try:
        completed = subprocess.run(
            [str(exe_path), "run", str(main_lang)],
            input=expected["input"].encode("utf-8"),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired:
        return (False, [f"超时（>{TIMEOUT_SECONDS} 秒）"])
    except OSError as error:
        return (False, [f"无法启动解释器：{error}"])

    stdout = completed.stdout.decode("utf-8-sig", errors="replace")
    stderr = completed.stderr.decode("utf-8-sig", errors="replace")
    return compare_case(case_dir, stdout, stderr, completed.returncode)


def main() -> int:
    parser = argparse.ArgumentParser(description="运行 .fld 测试用例")
    parser.add_argument(
        "--exe",
        default=str(REPO_ROOT / "fluid.exe"),
        help="fluid.exe 路径（默认仓库根目录）",
    )
    parser.add_argument("--filter", default=None, help="只运行名称包含该子串的用例")
    args = parser.parse_args()

    exe_path = Path(args.exe)
    if not exe_path.exists():
        print(f"找不到解释器：{exe_path}", file=sys.stderr)
        return 2

    cases = discover_cases()
    if args.filter:
        cases = [case for case in cases if args.filter in case.name]
    if not cases:
        print("没有找到测试用例", file=sys.stderr)
        return 2

    passed = 0
    failed = 0
    for case_dir in cases:
        ok, problems = run_case(case_dir, exe_path)
        if ok:
            passed += 1
            print(f"PASS  {case_dir.name}")
        else:
            failed += 1
            print(f"FAIL  {case_dir.name}")
            for problem in problems:
                print(f"      - {problem}")

    print()
    print(f"汇总：{passed} 通过，{failed} 失败，共 {passed + failed} 个用例")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
