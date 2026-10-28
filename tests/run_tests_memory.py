#!/usr/bin/env python3
"""沙箱/受限环境下的内存验证驱动。

本脚本复用 run_tests.py 的比对逻辑，但把 main.fld 源码通过 stdin 传给
membatch.exe 在内存中执行，避免原生 exe 读取文件被系统拒绝。
正式测试仍应使用 `python tests/run_tests.py`。
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import run_tests

TESTS_DIR = Path(__file__).resolve().parent
HELPER = TESTS_DIR / "membatch.exe"
TIMEOUT_SECONDS = 10


def run_case(case_dir: Path) -> tuple[bool, list[str]]:
    main_lang = case_dir / "main.fld"
    if not main_lang.exists():
        return (False, ["缺少 main.fld"])
    source = main_lang.read_bytes()
    input_path = case_dir / "input.txt"
    input_bytes = input_path.read_bytes() if input_path.exists() else b""

    frame = f"{len(source)} {len(input_bytes)}\n".encode("ascii") + source + input_bytes
    try:
        completed = subprocess.run(
            [str(HELPER)],
            input=frame,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired:
        return (False, [f"超时（>{TIMEOUT_SECONDS} 秒）"])

    stdout = completed.stdout.decode("utf-8-sig", errors="replace")
    stderr = completed.stderr.decode("utf-8-sig", errors="replace")
    return run_tests.compare_case(case_dir, stdout, stderr, completed.returncode)


def main() -> int:
    if not HELPER.exists():
        print(f"找不到 {HELPER}，请先编译 tests/membatch.cpp", file=sys.stderr)
        return 2

    cases = run_tests.discover_cases()
    if not cases:
        print("没有找到测试用例", file=sys.stderr)
        return 2

    passed = 0
    failed = 0
    for case_dir in cases:
        ok, problems = run_case(case_dir)
        if ok:
            passed += 1
            print(f"PASS  {case_dir.name}")
        else:
            failed += 1
            print(f"FAIL  {case_dir.name}")
            for problem in problems:
                print(f"      - {problem}")

    print()
    print(f"汇总（内存模式）：{passed} 通过，{failed} 失败，共 {passed + failed} 个用例")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
