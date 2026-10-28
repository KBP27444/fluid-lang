from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "fluid.exe"

VALID = """function add a, b
  return a
function main
  initialize integer c 3
  output invoke add 1, 2, c
  if not 1 = 2
    output "yes"
"""

CHAIN = """function main
  output 1 < 2 < 3
"""

CONFLICT = """function main
  alias value output
  initialize integer value 1
"""


def run_case(name, source, expected_exit, expected_text):
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "main.fld"
        path.write_text(source, encoding="utf-8")
        result = subprocess.run(
            [str(EXE), "run", str(path)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=10,
        )
    combined = result.stdout + result.stderr
    assert result.returncode == expected_exit, (name, result.returncode, combined)
    assert expected_text in combined, (name, combined)
    if expected_exit == 1:
        assert re.search(r"^.+:\d+:\d+: 错误: ", result.stderr, re.MULTILINE), result.stderr
    print(f"PASS {name}: exit={result.returncode}, output={combined.strip()!r}")


def main():
    assert EXE.is_file(), f"missing {EXE}"
    run_case("valid", VALID, 0, "13yes")
    run_case("comparison_chain", CHAIN, 1, "链式比较")
    run_case("out_of_order_conflict", CONFLICT, 1, "冲突")


if __name__ == "__main__":
    main()
