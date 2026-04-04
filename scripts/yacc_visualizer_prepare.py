#!/usr/bin/env python3
"""
将 artifacts/yacc/stepX/<case_id>/ 原始文本产物适配为 visualizer 使用的 JSON 协议。

设计目标：
1. 与 /src 完全解耦（只读 artifacts 文件）。
2. 输出稳定版本化结构 visualizer/public/data/v1/<case_id>/。
3. 兼容 step3~step6 当前产物。
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List


KV_RE = re.compile(r"^([^=]+)=(.*)$")
PROD_RE = re.compile(r"^#(\d+)\s+(.+?)\s*->\s*(.*?)\s+\[line=(\d+)\](?:\s+\[action=yes\])?$")


@dataclass
class Paths:
    repo_root: Path
    artifacts_root: Path
    output_root: Path


def parse_key_values(path: Path) -> Dict[str, str]:
    result: Dict[str, str] = {}
    if not path.exists():
        return result
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        m = KV_RE.match(line)
        if m:
            result[m.group(1).strip()] = m.group(2).strip()
    return result


def parse_tsv(path: Path) -> List[Dict[str, str]]:
    if not path.exists():
        return []
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines:
        return []
    headers = lines[0].split("\t")
    rows: List[Dict[str, str]] = []
    for line in lines[1:]:
        if not line.strip():
            continue
        cols = line.split("\t")
        row = {}
        for i, h in enumerate(headers):
            row[h] = cols[i] if i < len(cols) else ""
        rows.append(row)
    return rows


def parse_productions(path: Path) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    if not path.exists():
        return result
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        m = PROD_RE.match(line)
        if not m:
            continue
        rhs_text = m.group(3).strip()
        rhs = [] if rhs_text == "epsilon" else rhs_text.split()
        result.append(
            {
                "id": int(m.group(1)),
                "lhs": m.group(2),
                "rhs": rhs,
                "line": int(m.group(4)),
                "action": "[action=yes]" in line,
                "text": line,
            }
        )
    return result


def parse_lr1_items(path: Path) -> Dict[str, List[str]]:
    section = None
    kernel: List[str] = []
    closure: List[str] = []
    if not path.exists():
        return {"kernel": kernel, "closure": closure}

    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        if line == "[I0 kernel]":
            section = "kernel"
            continue
        if line == "[I0 closure]":
            section = "closure"
            continue
        if section == "kernel":
            kernel.append(line)
        elif section == "closure":
            closure.append(line)

    return {"kernel": kernel, "closure": closure}


def parse_goto_items(path: Path) -> Dict[str, List[str]]:
    result: Dict[str, List[str]] = {}
    current = ""
    if not path.exists():
        return result
    header_re = re.compile(r"^\[goto\(I0,\s*(.+)\)\s+items\]$")
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        m = header_re.match(line)
        if m:
            current = m.group(1)
            result[current] = []
            continue
        if current:
            result[current].append(line)
    return result


def parse_notes(path: Path) -> List[str]:
    notes: List[str] = []
    if not path.exists():
        return notes
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if line.startswith("- "):
            notes.append(line[2:])
    return notes


def normalize_symbols(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "id": int(row.get("id", "0")),
                "name": row.get("name", ""),
                "kind": row.get("kind", ""),
                "is_literal_char": row.get("is_literal_char", "0") == "1",
            }
        )
    return result


def normalize_prod_index(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        ids_text = row.get("production_ids", "").strip()
        prod_ids = [int(x) for x in ids_text.split(",") if x] if ids_text else []
        result.append(
            {
                "lhs_id": int(row.get("lhs_id", "0")),
                "lhs_name": row.get("lhs_name", ""),
                "production_ids": prod_ids,
            }
        )
    return result


def normalize_first(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        first_text = row.get("first_set", "").strip()
        first_set = [x for x in first_text.split(",") if x] if first_text else []
        result.append(
            {
                "symbol_id": int(row.get("symbol_id", "0")),
                "symbol_name": row.get("symbol_name", ""),
                "kind": row.get("kind", ""),
                "first_set": first_set,
            }
        )
    return result


def normalize_goto(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "symbol_id": int(row.get("symbol_id", "0")),
                "symbol_name": row.get("symbol_name", ""),
                "item_count": int(row.get("item_count", "0")),
            }
        )
    return result


def read_augmented_text(path: Path) -> str:
    if not path.exists():
        return ""
    lines = [x.strip() for x in path.read_text(encoding="utf-8").splitlines() if x.strip()]
    if not lines:
        return ""
    if len(lines) == 1:
        return lines[0]
    return lines[1]


def write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def collect_step_payload(step_dir: Path, step: int) -> Dict[str, object]:
    summary = parse_key_values(step_dir / "summary.txt")
    analysis = parse_key_values(step_dir / "analysis" / "report.txt")
    source = summary.get("source", "")

    payload: Dict[str, object] = {
        "step": step,
        "source": source,
        "summary": summary,
        "analysis": analysis,
    }

    raw_dir = step_dir / "raw"
    symbols = normalize_symbols(parse_tsv(raw_dir / "symbols.tsv"))
    productions = parse_productions(raw_dir / "productions.txt")
    if symbols:
        payload["symbols"] = symbols
    if productions:
        payload["productions"] = productions

    if step >= 4:
        payload["augmented"] = {"production": read_augmented_text(raw_dir / "augmented_grammar.txt")}
        payload["prod_index_by_lhs"] = normalize_prod_index(parse_tsv(raw_dir / "prod_index_by_lhs.tsv"))

    if step >= 5:
        payload["first_sets"] = normalize_first(parse_tsv(raw_dir / "first_sets.tsv"))

    if step >= 6:
        lr1_items = parse_lr1_items(raw_dir / "lr1_i0_items.txt")
        payload["lr1_i0"] = {
            "kernel_items": lr1_items["kernel"],
            "closure_items": lr1_items["closure"],
            "goto_edges": normalize_goto(parse_tsv(raw_dir / "lr1_i0_goto.tsv")),
            "goto_items": parse_goto_items(raw_dir / "lr1_i0_goto_items.txt"),
            "lookahead_notes": parse_notes(raw_dir / "lr1_lookahead_derivation.txt"),
        }
    return payload


def build_case(paths: Paths, case_id: str, steps: List[int]) -> None:
    case_output_root = paths.output_root / case_id
    case_output_root.mkdir(parents=True, exist_ok=True)

    manifest = {
        "schema_version": "yacc-visualizer/v1",
        "case_id": case_id,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "source": "",
        "steps": {},
    }

    step_titles = {
        3: "输入解析结果",
        4: "文法预处理与增广",
        5: "First 集计算",
        6: "LR(1) I0 闭包与 Goto",
    }

    for step in steps:
        step_dir = paths.artifacts_root / f"step{step}" / case_id
        if not step_dir.exists():
            continue
        payload = collect_step_payload(step_dir, step)
        step_name = f"step{step}"
        step_out = case_output_root / step_name / "data.json"
        write_json(step_out, payload)

        if not manifest["source"]:
            manifest["source"] = str(payload.get("source", ""))
        manifest["steps"][str(step)] = {
            "step": step,
            "title": step_titles.get(step, f"步骤 {step}"),
            "files": {"data": f"{step_name}/data.json"},
        }

    write_json(case_output_root / "manifest.json", manifest)


def discover_cases(artifacts_root: Path, requested_case: str) -> List[str]:
    if requested_case:
        return [requested_case]
    discovered = set()
    for step_dir in artifacts_root.glob("step*"):
        if not step_dir.is_dir():
            continue
        for case_dir in step_dir.iterdir():
            if case_dir.is_dir():
                discovered.add(case_dir.name)
    return sorted(discovered)


def main() -> None:
    parser = argparse.ArgumentParser(description="为 YACC 可视化页面准备 JSON 数据")
    parser.add_argument("--case", default="", help="仅处理指定 case_id，例如 c99")
    parser.add_argument(
        "--steps", default="3,4,5,6", help="处理步骤列表，逗号分隔，默认 3,4,5,6"
    )
    parser.add_argument(
        "--artifacts-root", default="artifacts/yacc", help="YACC 原始产物目录"
    )
    parser.add_argument(
        "--output-root",
        default="visualizer/public/data/v1",
        help="可视化 JSON 输出目录",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    artifacts_root = (repo_root / args.artifacts_root).resolve()
    output_root = (repo_root / args.output_root).resolve()
    steps = [int(x.strip()) for x in args.steps.split(",") if x.strip()]
    paths = Paths(repo_root=repo_root, artifacts_root=artifacts_root, output_root=output_root)

    cases = discover_cases(artifacts_root, args.case)
    if not cases:
        print("未发现可处理的 case。")
        return

    for case_id in cases:
        build_case(paths, case_id, steps)
        print(f"[ok] case={case_id} -> {output_root / case_id}")


if __name__ == "__main__":
    main()
