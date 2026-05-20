#!/usr/bin/env python3
"""统一运行 YACC 测试 1/2/2f/3/4/5/6/7。"""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys


def log(level: str, message: str) -> None:
    print(f"[{level}]{message}")


def run(cmd: list[str], cwd: pathlib.Path) -> int:
    real_cmd = list(cmd)
    if real_cmd and real_cmd[0] == "python3" and (len(real_cmd) < 2 or real_cmd[1] != "-u"):
        real_cmd.insert(1, "-u")

    log("INFO", f"执行命令: {' '.join(real_cmd)}")
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    proc = subprocess.Popen(
        real_cmd,
        cwd=str(cwd),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        env=env,
    )
    assert proc.stdout is not None
    for line in proc.stdout:
        print(line.rstrip())
    rc = proc.wait()
    if rc == 0:
        log("PASS", f"命令执行成功: {' '.join(real_cmd)}")
    else:
        log("ERROR", f"命令执行失败(rc={rc}): {' '.join(real_cmd)}")
    return rc


def main() -> int:
    parser = argparse.ArgumentParser(description="运行 YACC 测试 1/2/2f/3/4/5/6/7")
    parser.add_argument(
        "--tests",
        default="1,2,3,4",
        help="要运行的测试编号，逗号分隔，例如 1,2,2f,4",
    )
    parser.add_argument("--workdir", default=".", help="项目根目录")
    parser.add_argument("--strict-bison", action="store_true", help="测试2/2f/7缺依赖时直接失败")
    parser.add_argument("--update-golden", action="store_true", help="测试1更新 golden 快照")
    args = parser.parse_args()

    root = pathlib.Path(args.workdir).resolve()
    log("INFO", f"测试根目录: {root}")
    test_ids = [x.strip() for x in args.tests.split(",") if x.strip()]
    rc_all = 0

    for test_id in test_ids:
        if test_id == "1":
            test_name = "测试一（golden 回归）"
            cmd = ["python3", "tests/scripts/test1_golden_regression.py"]
            if args.update_golden:
                cmd.append("--update-golden")
        elif test_id == "2":
            test_name = "测试二（Bison 严格对拍）"
            cmd = ["python3", "tests/scripts/test2_bison_compare.py"]
            if args.strict_bison:
                cmd.append("--strict")
        elif test_id == "2f":
            test_name = "测试二扩展（完整 LALR 状态机对拍）"
            cmd = ["python3", "tests/scripts/test2_full_lalr_automaton_compare.py"]
            if args.strict_bison:
                cmd.append("--strict")
        elif test_id == "3":
            test_name = "测试三（表一致性）"
            cmd = ["python3", "tests/scripts/test3_table_consistency.py"]
        elif test_id == "4":
            test_name = "测试四（trace 不变式）"
            cmd = ["python3", "tests/scripts/test4_trace_invariants.py"]
        elif test_id == "5":
            test_name = "测试五（y.tab.h 与 %token 集合一致性）"
            cmd = ["python3", "tests/scripts/test5_emit_y_tab_h_consistency.py"]
        elif test_id == "6":
            test_name = "测试六（token_cases.inc 与 y.tab.h 一致性）"
            cmd = ["python3", "tests/scripts/test6_emit_token_cases_consistency.py"]
        elif test_id == "7":
            test_name = "测试七（导出 y.tab.h 与 bison 头文件对拍）"
            cmd = ["python3", "tests/scripts/test7_bison_tab_h_compare.py"]
            if args.strict_bison:
                cmd.append("--strict")
        else:
            log("WARN", f"忽略未知测试编号: {test_id}")
            continue

        log("INFO", f"测试开始：{test_name}")
        rc = run(cmd, cwd=root)
        if rc != 0:
            rc_all = rc
            log("ERROR", f"测试失败：{test_name}")
        else:
            log("PASS", f"测试通过：{test_name}")

    if rc_all == 0:
        log("PASS", "全部测试通过")
    else:
        log("ERROR", "存在测试失败")
    return rc_all


if __name__ == "__main__":
    sys.exit(main())
