#!/usr/bin/env python3
"""测试7：y.tab.h 与 bison -d 生成头文件对拍。"""

from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import sys
import tempfile

from _common import run_cmd


def fail(msg: str) -> int:
    """输出失败信息并返回统一错误码。"""
    print(f"FAIL: {msg}")
    return 1


def parse_enum_entries(header_text: str) -> list[tuple[str, int]]:
    enum_match = re.search(r"enum\s+yytokentype\s*\{(.*?)\};", header_text, re.S)
    if not enum_match:
        raise ValueError("未找到 enum yytokentype")
    body = enum_match.group(1)
    entries: list[tuple[str, int]] = []
    pattern = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\b\s*=\s*(-?\d+)")
    for name, value_text in pattern.findall(body):
        entries.append((name, int(value_text)))
    return entries


def parse_self_define_macros(header_text: str) -> list[str]:
    names: list[str] = []
    pattern = re.compile(r"^\s*#\s*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]+\1\s*$", re.M)
    for name in pattern.findall(header_text):
        names.append(name)
    return names


def has_marker(header_text: str, marker: str) -> bool:
    return marker in header_text


def main() -> int:
    """脚本主入口：组织流程并给出最终退出码。"""
    parser = argparse.ArgumentParser(description="测试7：导出 y.tab.h 与 bison 头文件对拍")
    parser.add_argument("--bin", default="./build/src/yacc_parse_tool", help="yacc_parse_tool 路径")
    parser.add_argument("--grammar", default="c99.y", help=".y 文法路径")
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--strict", action="store_true", help="缺失 bison 时直接失败")
    args = parser.parse_args()

    bison = shutil.which("bison")
    if not bison:
        msg = "SKIP: 缺少 bison"
        if args.strict:
            return fail(msg)
        print(msg)
        return 0

    root = pathlib.Path(args.workdir).resolve()
    grammar = root / args.grammar
    if not grammar.exists():
        return fail(f"文法文件不存在: {grammar}")

    with tempfile.TemporaryDirectory(prefix="test7_tabh_") as td:
        tmp = pathlib.Path(td)
        ours_h = tmp / "ours.y.tab.h"
        bison_c = tmp / "bison.tab.c"
        bison_h = tmp / "bison.tab.h"

        p1 = run_cmd([args.bin, str(grammar), "--emit-y-tab-h", str(ours_h)], cwd=root)
        if p1.returncode != 0:
            return fail(f"我方导出失败\n{p1.stderr}\n{p1.stdout}")

        p2 = run_cmd([bison, "-d", str(grammar), "-o", str(bison_c)], cwd=root)
        if p2.returncode != 0:
            return fail(f"bison 生成失败\n{p2.stderr}\n{p2.stdout}")
        if not bison_h.exists():
            return fail(f"未找到 bison 头文件: {bison_h}")

        ours_text = ours_h.read_text(encoding="utf-8")
        bison_text = bison_h.read_text(encoding="utf-8")

        ours_enum = parse_enum_entries(ours_text)
        bison_enum = parse_enum_entries(bison_text)
        if ours_enum != bison_enum:
            return fail(
                "enum yytokentype 不一致。\n"
                f"ours(前10): {ours_enum[:10]}\n"
                f"bison(前10): {bison_enum[:10]}"
            )

        ours_self_macros = parse_self_define_macros(ours_text)
        bison_self_macros = parse_self_define_macros(bison_text)
        if ours_self_macros != bison_self_macros:
            return fail(
                "自引用 token 宏列表不一致。\n"
                f"ours(前10): {ours_self_macros[:10]}\n"
                f"bison(前10): {bison_self_macros[:10]}"
            )

        required_markers = [
            "#ifndef YYTOKENTYPE",
            "typedef enum yytokentype yytoken_kind_t;",
            "typedef int YYSTYPE;",
            "extern YYSTYPE yylval;",
            "int yyparse (void);",
        ]
        for marker in required_markers:
            if not has_marker(ours_text, marker):
                return fail(f"我方头文件缺少必要结构: {marker}")
            if not has_marker(bison_text, marker):
                return fail(f"bison 头文件缺少必要结构(环境异常): {marker}")

    print("PASS: 测试7通过（y.tab.h 与 bison 头文件关键结构一致）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
