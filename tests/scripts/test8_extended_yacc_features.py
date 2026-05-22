#!/usr/bin/env python3
"""测试8：扩展 YACC 特性（8a 指令解析，8b %expect 门禁，8c mid-rule action）。"""

from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys

from _common import parse_kv_file


def run_cmd(cmd: list[str], cwd: pathlib.Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd), text=True, capture_output=True)


def assert_ok(cp: subprocess.CompletedProcess[str], msg: str) -> None:
    if cp.returncode != 0:
        raise AssertionError(f"{msg}\nrc={cp.returncode}\nstdout=\n{cp.stdout}\nstderr=\n{cp.stderr}")


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[2]
    bin_path = root / "build" / "src" / "yacc_parse_tool"
    if not bin_path.exists():
        print(f"FAIL: 缺少可执行文件: {bin_path}")
        return 1

    out_root = root / "tests" / "out" / "test8"
    if out_root.exists():
        shutil.rmtree(out_root)
    out_root.mkdir(parents=True, exist_ok=True)

    # case8a-1: 第二个%%可缺失 + %union/%type 可解析
    case1_dir = out_root / "case8a_1"
    cp1 = run_cmd([
        str(bin_path),
        "tests/grammars/extended_defs_no_second_percent.y",
        "--parse-tokens",
        "tests/tokens/extended_defs_num.tokens",
        "--export",
        "--export-dir",
        str(case1_dir),
    ], root)
    assert_ok(cp1, "case1 执行失败")
    s1 = parse_kv_file(case1_dir / "summary.txt")
    if s1.get("lalr_parse_accepted") != "true":
        raise AssertionError(f"case1 预期 accept=true, 实际={s1.get('lalr_parse_accepted')}")

    # case8a-2: %prec + %nonassoc 生效（NUM < NUM < NUM 应报错）
    case2_dir = out_root / "case8a_2"
    cp2 = run_cmd([
        str(bin_path),
        "tests/grammars/precedence_nonassoc_prec.y",
        "--parse-tokens",
        "tests/tokens/precedence_nonassoc_should_error.tokens",
        "--export",
        "--export-dir",
        str(case2_dir),
    ], root)
    assert_ok(cp2, "case2 执行失败")
    s2 = parse_kv_file(case2_dir / "summary.txt")
    if s2.get("lalr_parse_accepted") != "false":
        raise AssertionError(f"case2 预期 accept=false, 实际={s2.get('lalr_parse_accepted')}")

    # case8a-3: parsed-only 指令在 strict-bison-ish 下触发失败
    cp3 = run_cmd([
        str(bin_path),
        "emit",
        "tests/grammars/parsed_only_directives.y",
        "--strict-bison-ish",
        "--emit-y-tab-h",
        str(out_root / "strict.tab.h"),
    ], root)
    if cp3.returncode == 0:
        raise AssertionError("case8a_3 预期 strict-bison-ish 失败，但返回成功")

    # case8b: %expect 门禁（声明 1，实际 2 个 S/R 冲突）应失败
    cp4 = run_cmd([
        str(bin_path),
        "tests/grammars/expect_mismatch.y",
        "--parse-tokens",
        "tests/tokens/precedence_nonassoc_should_error.tokens",
    ], root)
    if cp4.returncode == 0:
        raise AssertionError("case8b 预期 %expect 门禁失败，但返回成功")

    # case8c: mid-rule action 可运行
    case5_dir = out_root / "case8c"
    cp5 = run_cmd([
        str(bin_path),
        "tests/grammars/midrule_action.y",
        "--parse-tokens",
        "tests/tokens/midrule_action.tokens",
        "--export",
        "--export-dir",
        str(case5_dir),
    ], root)
    assert_ok(cp5, "case8c 执行失败")
    s5 = parse_kv_file(case5_dir / "summary.txt")
    if s5.get("lalr_parse_accepted") != "true":
        raise AssertionError(f"case8c 预期 accept=true, 实际={s5.get('lalr_parse_accepted')}")

    print("PASS: 测试8通过（8a/8b/8c）")
    print(f"  out_root={out_root}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print(f"FAIL: {e}")
        sys.exit(1)
