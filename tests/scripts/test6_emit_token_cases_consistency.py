#!/usr/bin/env python3
"""测试6：导出的 token_cases.inc 与 y.tab.h 命名 token 一一对应。"""

from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import sys

from _common import run_cmd


def fail(msg: str) -> int:
    print(f"FAIL: {msg}")
    return 1


def parse_tokens_from_y_tab_h(y_tab_h_path: pathlib.Path) -> set[str]:
    text = y_tab_h_path.read_text(encoding="utf-8")
    pattern = re.compile(r"^\s*([A-Z_][A-Z0-9_]*)\s*=\s*-?\d+\s*,?\s*$", re.MULTILINE)
    skip = {"YYEMPTY", "YYEOF", "YYerror", "YYUNDEF"}
    names: set[str] = set()
    for match in pattern.finditer(text):
        name = match.group(1)
        if name not in skip:
            names.add(name)
    return names


def parse_tokens_from_cases(cases_path: pathlib.Path) -> set[str]:
    pattern = re.compile(r'^\s*case\s+([A-Z_][A-Z0-9_]*)\s*:\s*return\s+"([A-Z_][A-Z0-9_]*)";\s*$')
    names: set[str] = set()
    for raw in cases_path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        match = pattern.match(line)
        if not match:
            raise ValueError(f"无效 case 行: {line}")
        lhs, rhs = match.group(1), match.group(2)
        if lhs != rhs:
            raise ValueError(f"case/return 名称不一致: {line}")
        names.add(lhs)
    return names


def main() -> int:
    parser = argparse.ArgumentParser(description="测试6：token_cases.inc 与 y.tab.h 一致性")
    parser.add_argument("--bin", default="./build/src/yacc_parse_tool", help="yacc_parse_tool 路径")
    parser.add_argument("--grammar", default="c99.y", help=".y 文法路径")
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--out-dir", default="tests/out/test6_emit_token_cases", help="输出目录")
    args = parser.parse_args()

    root = pathlib.Path(args.workdir).resolve()
    grammar = root / args.grammar
    out_dir = root / args.out_dir
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    y_tab_h = out_dir / "y.tab.h"
    cases_inc = out_dir / "token_cases.inc"

    proc = run_cmd(
        [
            args.bin,
            str(grammar),
            "--emit-y-tab-h",
            str(y_tab_h),
            "--emit-token-cases-inc",
            str(cases_inc),
        ],
        cwd=root,
    )
    if proc.returncode != 0:
        return fail(f"工具执行失败，exit={proc.returncode}\n{proc.stderr}\n{proc.stdout}")

    ytab_tokens = parse_tokens_from_y_tab_h(y_tab_h)
    try:
        case_tokens = parse_tokens_from_cases(cases_inc)
    except ValueError as ex:
        return fail(str(ex))

    missing = sorted(ytab_tokens - case_tokens)
    extra = sorted(case_tokens - ytab_tokens)
    if missing or extra:
        return fail(f"token 不一致，missing={missing}, extra={extra}")

    print("PASS: 测试6通过")
    print(f"  tokens={len(case_tokens)}")
    print(f"  token_cases={cases_inc}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
