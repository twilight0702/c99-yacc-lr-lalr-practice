#!/usr/bin/env python3
"""测试9：emit parser cpp + from-lexer + AST/IR 导出。"""

from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys


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

    out = root / "tests" / "out" / "test9"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True, exist_ok=True)

    parser_cpp = out / "parser_generated.cpp"
    cp1 = run_cmd([str(bin_path), "emit", "c99.y", "--emit-parser-cpp", str(parser_cpp)], root)
    assert_ok(cp1, "emit parser cpp 失败")

    parser_bin = out / "parser_generated"
    cp2 = run_cmd(["g++", "-std=c++17", str(parser_cpp), "-O2", "-o", str(parser_bin)], root)
    assert_ok(cp2, "编译 parser_generated.cpp 失败")

    tokens = "contracts/yacc/tokens/c99_decl_int.tokens"
    cp3 = run_cmd([str(parser_bin), tokens], root)
    if cp3.returncode != 0 or "accept" not in cp3.stdout:
        raise AssertionError(f"生成 parser 运行失败: rc={cp3.returncode}, stdout={cp3.stdout}, stderr={cp3.stderr}")

    ast_json = out / "ast.json"
    ir_txt = out / "quads.txt"
    cp4 = run_cmd(
        [
            str(bin_path),
            "run",
            "c99.y",
            "--from-lexer",
            tokens,
            "--ast-out",
            str(ast_json),
            "--ast-format",
            "json",
            "--ir-out",
            str(ir_txt),
        ],
        root,
    )
    assert_ok(cp4, "run --from-lexer/--ast-out/--ir-out 失败")
    if not ast_json.exists() or ast_json.stat().st_size == 0:
        raise AssertionError("ast.json 未生成")
    if not ir_txt.exists() or ir_txt.stat().st_size == 0:
        raise AssertionError("quads.txt 未生成")
    if "\"production_id\"" not in ast_json.read_text(encoding="utf-8"):
        raise AssertionError("ast.json 缺少 production_id 字段")

    print("PASS: 测试9通过（emit-parser/from-lexer/AST/IR）")
    print(f"  out={out}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print(f"FAIL: {e}")
        sys.exit(1)

