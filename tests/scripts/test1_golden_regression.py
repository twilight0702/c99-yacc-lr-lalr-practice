#!/usr/bin/env python3
"""测试1：基线回归（golden 快照比对）。"""

from __future__ import annotations

import argparse
import difflib
import hashlib
import pathlib
import shutil
import sys

from _common import DEFAULT_CASES, run_cmd


SNAPSHOT_FILES = [
    "summary.txt",
    "raw/parse_trace_lalr.tsv",
    "raw/parse_reductions_lalr.txt",
    "raw/parse_error_lalr.txt",
]


def log(level: str, message: str) -> None:
    """打印带级别前缀的日志行。"""
    print(f"[{level}]{message}")


def file_fingerprint(path: pathlib.Path) -> str:
    st = path.stat()
    return f"{path.resolve()}::{st.st_size}::{st.st_mtime_ns}"


def stable_key(parts: list[str]) -> str:
    return hashlib.sha256("||".join(parts).encode("utf-8")).hexdigest()


def read_text(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def ensure_parent(path: pathlib.Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def main() -> int:
    """脚本主入口：组织流程并给出最终退出码。"""
    parser = argparse.ArgumentParser(description="测试1：golden 基线回归")
    parser.add_argument("--bin", default="./build/src/yacc_parse_tool", help="yacc_parse_tool 路径")
    parser.add_argument("--grammar", default="c99.y", help=".y 文法路径")
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--out-root", default="tests/out/test1", help="临时导出目录")
    parser.add_argument("--cache-root", default="tests/out/.cache/test1", help="缓存目录")
    parser.add_argument("--golden-root", default="tests/golden/test1", help="golden 目录")
    parser.add_argument("--max-steps", type=int, default=200000, help="最大解析步数")
    parser.add_argument("--update-golden", action="store_true", help="用当前结果更新 golden")
    args = parser.parse_args()

    root = pathlib.Path(args.workdir).resolve()
    out_root = root / args.out_root
    golden_root = root / args.golden_root
    if out_root.exists():
        shutil.rmtree(out_root)
    out_root.mkdir(parents=True, exist_ok=True)
    if args.update_golden:
        golden_root.mkdir(parents=True, exist_ok=True)
    cache_root = root / args.cache_root
    cache_root.mkdir(parents=True, exist_ok=True)

    failures: list[str] = []
    passes: list[str] = []
    total_cases = len(DEFAULT_CASES)

    for idx, case in enumerate(DEFAULT_CASES, start=1):
        log("INFO", f"[{idx}/{total_cases}] case 开始: {case.name}")
        export_dir = out_root / case.name
        case_tokens_path = (root / case.tokens).resolve()
        case_key = stable_key(
            [
                file_fingerprint((root / args.bin).resolve()),
                file_fingerprint((root / args.grammar).resolve()),
                file_fingerprint(case_tokens_path),
                str(args.max_steps),
            ]
        )
        case_cache_dir = cache_root / "case_exports" / case_key
        if export_dir.exists():
            shutil.rmtree(export_dir)
        export_dir.mkdir(parents=True, exist_ok=True)

        if case_cache_dir.exists():
            log("INFO", f"[{case.name}] 缓存命中，复用快照导出")
            for rel in SNAPSHOT_FILES:
                src = case_cache_dir / rel
                dst = export_dir / rel
                if src.exists():
                    ensure_parent(dst)
                    shutil.copy2(src, dst)
        else:
            log("INFO", f"[{case.name}] 缓存未命中，运行 yacc_parse_tool 并导出快照")
            proc = run_cmd(
                [
                    args.bin,
                    args.grammar,
                    "--parse-tokens",
                    case.tokens,
                    "--export",
                    "--export-dir",
                    str(export_dir),
                    "--max-parse-steps",
                    str(args.max_steps),
                ],
                cwd=root,
            )
            if proc.returncode != 0:
                failures.append(f"{case.name}: 工具执行失败 exit={proc.returncode}")
                log("ERROR", f"[{case.name}] 工具执行失败 exit={proc.returncode}")
                continue
            for rel in SNAPSHOT_FILES:
                src = export_dir / rel
                dst = case_cache_dir / rel
                if src.exists():
                    ensure_parent(dst)
                    shutil.copy2(src, dst)
            log("PASS", f"[{case.name}] 快照已写入缓存")

        case_ok = True
        log("INFO", f"[{case.name}] 开始校验快照文件")
        for rel in SNAPSHOT_FILES:
            current = export_dir / rel
            golden = golden_root / case.name / rel
            if not current.exists():
                failures.append(f"{case.name}: 缺少当前输出文件 {current}")
                case_ok = False
                log("ERROR", f"[{case.name}] 缺少当前输出文件: {current}")
                continue
            if args.update_golden:
                ensure_parent(golden)
                golden.write_text(read_text(current), encoding="utf-8")
                log("INFO", f"[{case.name}] update-golden 写入: {rel}")
                continue
            if not golden.exists():
                failures.append(f"{case.name}: 缺少 golden 文件 {golden}")
                case_ok = False
                log("ERROR", f"[{case.name}] 缺少 golden 文件: {golden}")
                continue

            cur_text = read_text(current)
            old_text = read_text(golden)
            if cur_text != old_text:
                diff = "\n".join(
                    difflib.unified_diff(
                        old_text.splitlines(),
                        cur_text.splitlines(),
                        fromfile=str(golden.relative_to(root)),
                        tofile=str(current.relative_to(root)),
                        lineterm="",
                        n=2,
                    )
                )
                if len(diff.splitlines()) > 60:
                    diff = "\n".join(diff.splitlines()[:60]) + "\n... (diff truncated)"
                failures.append(f"{case.name}: 快照不一致 ({rel})\n{diff}")
                case_ok = False
                log("ERROR", f"[{case.name}] 快照不一致: {rel}")
                break

        if case_ok:
            passes.append(case.name)
            log("PASS", f"[{case.name}] case 通过")

    if args.update_golden:
        if failures:
            log("ERROR", "update 模式执行失败")
            for f in failures:
                log("ERROR", f"- {f}")
            return 1
        log("PASS", "golden 已更新")
        log("INFO", f"golden_root={golden_root}")
        log("INFO", f"cases={len(passes)}")
        return 0

    if failures:
        log("ERROR", "测试1基线回归失败")
        for f in failures[:20]:
            log("ERROR", f"- {f}")
        if len(failures) > 20:
            log("WARN", f"... 其余 {len(failures) - 20} 条已省略")
        return 1

    log("PASS", "测试1通过（golden 一致）")
    log("INFO", f"cases={len(passes)}")
    log("INFO", f"golden_root={golden_root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
