#!/usr/bin/env python3
"""测试5：导出的 y.tab.h 与 c99.y 中 %token 集合一致。"""

from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import sys

from _common import run_cmd


def fail(msg: str) -> int:
    """输出失败信息并返回统一错误码。"""
    print(f"FAIL: {msg}")
    return 1


def parse_tokens_from_grammar(grammar_path: pathlib.Path) -> set[str]:
    tokens: set[str] = set()
    for raw in grammar_path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line.startswith("%token"):
            continue
        parts = line.split()
        for token in parts[1:]:
            tokens.add(token)
    return tokens


def parse_tokens_from_y_tab_h(y_tab_h_path: pathlib.Path) -> set[str]:
    text = y_tab_h_path.read_text(encoding="utf-8")
    pattern = re.compile(r"^\s*([A-Z_][A-Z0-9_]*)\s*=\s*-?\d+\s*,?\s*$", re.MULTILINE)
    tokens: set[str] = set()
    skip = {"YYEMPTY", "YYEOF", "YYerror", "YYUNDEF"}
    for match in pattern.finditer(text):
        name = match.group(1)
        if name not in skip:
            tokens.add(name)
    return tokens


def main() -> int:
    """脚本主入口：组织流程并给出最终退出码。"""
    parser = argparse.ArgumentParser(description="测试5：y.tab.h token 集合一致性")
    parser.add_argument("--bin", default="./build/src/yacc_parse_tool", help="yacc_parse_tool 路径")
    parser.add_argument("--grammar", default="c99.y", help=".y 文法路径")
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--out-dir", default="tests/out/test5_emit_y_tab_h", help="输出目录")
    args = parser.parse_args()

    root = pathlib.Path(args.workdir).resolve()
    grammar = root / args.grammar
    out_dir = root / args.out_dir
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    y_tab_h = out_dir / "y.tab.h"

    proc = run_cmd([args.bin, str(grammar), "--emit-y-tab-h", str(y_tab_h)], cwd=root)
    if proc.returncode != 0:
        return fail(f"工具执行失败，exit={proc.returncode}\n{proc.stderr}\n{proc.stdout}")

    expected = parse_tokens_from_grammar(grammar)
    actual = parse_tokens_from_y_tab_h(y_tab_h)
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if missing or extra:
        return fail(f"token 集合不一致，missing={missing}, extra={extra}")

    print("PASS: 测试5通过")
    print(f"  tokens={len(actual)}")
    print(f"  y_tab_h={y_tab_h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
