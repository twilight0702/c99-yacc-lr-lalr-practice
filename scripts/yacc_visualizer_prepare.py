#!/usr/bin/env python3
"""
将 artifacts/yacc/stepX/<case_id>/ 原始文本产物适配为 visualizer 使用的 JSON 协议。

设计目标：
1. 与 /src 完全解耦（只读 artifacts 文件）。
2. 输出稳定版本化结构 visualizer/public/data/v1/<case_id>/。
3. 兼容 step3~step10 当前产物。
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


def resolve_step_dir(paths: Paths, case_id: str, step: int) -> Path | None:
    """
    解析某一步应读取的 artifacts 目录。

    优先使用 stepN/<case>；若不存在，回退到更高步骤目录（如 step10 的聚合导出），
    以兼容 "--export 只产出单一步目录" 的当前 CLI 行为。
    """
    preferred = paths.artifacts_root / f"step{step}" / case_id
    if preferred.exists():
        return preferred

    # step1/2 历史数据兼容：优先尝试 step3。
    if step in (1, 2):
        step3 = paths.artifacts_root / "step3" / case_id
        if step3.exists():
            return step3

    # 通用回退：优先读高步骤聚合目录，再退到低步骤。
    # 常见场景：仅存在 step10/<case> 或 step9/<case>。
    candidates = list(range(10, step, -1)) + list(range(step - 1, 0, -1))
    for s in candidates:
        p = paths.artifacts_root / f"step{s}" / case_id
        if p.exists():
            return p

    return None


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


def strip_augmented_productions(rows: List[Dict[str, object]]) -> List[Dict[str, object]]:
    return [row for row in rows if int(row.get("id", -1)) != 0 and str(row.get("lhs", "")) != "S'"]


def strip_augmented_symbols(rows: List[Dict[str, object]]) -> List[Dict[str, object]]:
    return [row for row in rows if str(row.get("name", "")) != "S'"]


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


def parse_reductions(path: Path) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    if not path.exists():
        return result
    # 形如：1. #99 declaration -> declaration_specifiers ';'
    line_re = re.compile(r"^(\d+)\.\s+#(-?\d+)\s*(.*)$")
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        m = line_re.match(line)
        if not m:
            result.append({"index": len(result) + 1, "production_id": -1, "text": line})
            continue
        result.append(
            {
                "index": int(m.group(1)),
                "production_id": int(m.group(2)),
                "text": m.group(3).strip(),
            }
        )
    return result


def parse_error_kv(path: Path) -> Dict[str, str]:
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8").strip()
    if not text:
        return {}
    if text == "no_error":
        return {"status": "no_error"}
    result: Dict[str, str] = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or "=" not in line:
            continue
        k, v = line.split("=", 1)
        result[k.strip()] = v.strip()
    if result:
        result.setdefault("status", "error")
    return result


def parse_ast_json(path: Path) -> Dict[str, object]:
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8").strip()
    if not text:
        return {}
    try:
        value = json.loads(text)
        if isinstance(value, dict):
            return value
    except Exception:
        return {}
    return {}


def read_text(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8")


def parse_state_items(path: Path) -> Dict[str, List[str]]:
    result: Dict[str, List[str]] = {}
    current_state = ""
    if not path.exists():
        return result

    header_re = re.compile(r"^\[state\s+(\d+)\]$")
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        m = header_re.match(line)
        if m:
            current_state = m.group(1)
            result[current_state] = []
            continue
        if current_state:
            result[current_state].append(line)
    return result


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


def normalize_goto(rows: List[Dict[str, str]], i0_targets: Dict[str, int] | None = None) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    i0_targets = i0_targets or {}
    for row in rows:
        symbol_name = row.get("symbol_name", "")
        result.append(
            {
                "symbol_id": int(row.get("symbol_id", "0")),
                "symbol_name": symbol_name,
                "item_count": int(row.get("item_count", "0")),
                "to_state": int(i0_targets.get(symbol_name, -1)),
            }
        )
    return result


def normalize_lr1_states(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "state_id": int(row.get("state_id", "0")),
                "item_count": int(row.get("item_count", "0")),
            }
        )
    return result


def normalize_lr1_transitions(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "from_state": int(row.get("from_state", "0")),
                "symbol_id": int(row.get("symbol_id", "0")),
                "symbol_name": row.get("symbol_name", ""),
                "to_state": int(row.get("to_state", "0")),
            }
        )
    return result


def normalize_predecessors(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        pred_text = row.get("predecessors", "").strip()
        predecessors = [x for x in pred_text.split(",") if x] if pred_text else []
        result.append(
            {
                "state_id": int(row.get("state_id", "0")),
                "predecessors": predecessors,
            }
        )
    return result


def normalize_action_table(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "state_id": int(row.get("state_id", "0")),
                "terminal_id": int(row.get("terminal_id", "0")),
                "terminal_name": row.get("terminal_name", ""),
                "action": row.get("action", ""),
                "target": row.get("target", ""),
            }
        )
    return result


def normalize_goto_table(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "state_id": int(row.get("state_id", "0")),
                "nonterminal_id": int(row.get("nonterminal_id", "0")),
                "nonterminal_name": row.get("nonterminal_name", ""),
                "to_state": int(row.get("to_state", "0")),
            }
        )
    return result


def normalize_parse_conflicts(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "state_id": int(row.get("state_id", "0")),
                "symbol_id": int(row.get("symbol_id", "0")),
                "symbol_name": row.get("symbol_name", ""),
                "conflict_type": row.get("conflict_type", ""),
                "existing_action": row.get("existing_action", ""),
                "incoming_action": row.get("incoming_action", ""),
                "related_items": row.get("related_items", ""),
            }
        )
    return result


def normalize_conflict_resolutions(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "state_id": int(row.get("state_id", "0")),
                "symbol_id": int(row.get("symbol_id", "0")),
                "symbol_name": row.get("symbol_name", ""),
                "conflict_type": row.get("conflict_type", ""),
                "existing_action": row.get("existing_action", ""),
                "incoming_action": row.get("incoming_action", ""),
                "resolved_action": row.get("resolved_action", ""),
                "reason": row.get("reason", ""),
            }
        )
    return result


def normalize_parse_input_tokens(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "index": int(row.get("index", "0")),
                "symbol_id": int(row.get("symbol_id", "0")),
                "symbol_name": row.get("symbol_name", ""),
                "lexeme": row.get("lexeme", ""),
                "line": int(row.get("line", "0")),
                "column": int(row.get("column", "0")),
            }
        )
    return result


def normalize_parse_trace(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        result.append(
            {
                "step": int(row.get("step", "0")),
                "state": int(row.get("state", "0")),
                "lookahead_id": int(row.get("lookahead_id", "0")),
                "lookahead": row.get("lookahead", ""),
                "action": row.get("action", ""),
                "production_id": int(row.get("production_id", "-1")),
                "state_stack": row.get("state_stack", ""),
                "symbol_stack": row.get("symbol_stack", ""),
                "input_index": int(row.get("input_index", "0")),
            }
        )
    return result


def normalize_state_map(rows: List[Dict[str, str]]) -> List[Dict[str, int]]:
    result: List[Dict[str, int]] = []
    for row in rows:
        result.append(
            {
                "lr1_state": int(row.get("lr1_state", "0")),
                "lalr_state": int(row.get("lalr_state", "0")),
            }
        )
    return result


def normalize_merge_groups(rows: List[Dict[str, str]]) -> List[Dict[str, object]]:
    result: List[Dict[str, object]] = []
    for row in rows:
        src = row.get("source_lr1_states", "").strip()
        source_ids = [int(x) for x in src.split(",") if x] if src else []
        result.append(
            {
                "lalr_state": int(row.get("lalr_state", "0")),
                "source_lr1_states": source_ids,
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


def load_source_preview(repo_root: Path, source_rel_path: str, max_lines: int = 80) -> Dict[str, object]:
    """
    读取源语法文件预览（用于 Step1 轻量可视化）。
    """
    if not source_rel_path:
        return {"source_exists": False, "total_lines": 0, "nonempty_lines": 0, "preview_lines": []}

    source_path = (repo_root / source_rel_path).resolve()
    if not source_path.exists():
        return {"source_exists": False, "total_lines": 0, "nonempty_lines": 0, "preview_lines": []}

    lines = source_path.read_text(encoding="utf-8", errors="ignore").splitlines()
    preview = [{"line": i + 1, "text": line} for i, line in enumerate(lines[:max_lines])]
    nonempty = sum(1 for x in lines if x.strip())
    return {
        "source_exists": True,
        "total_lines": len(lines),
        "nonempty_lines": nonempty,
        "preview_lines": preview,
    }


def build_input_spec_checks(repo_root: Path, source_rel_path: str) -> List[Dict[str, object]]:
    checks: List[Dict[str, object]] = []
    if not source_rel_path:
        return [{"id": "source_path_present", "label": "source 路径存在", "status": "fail", "detail": "summary.source 为空"}]
    source_path = (repo_root / source_rel_path).resolve()
    if not source_path.exists():
        return [{
            "id": "source_file_exists",
            "label": "源文件可读取",
            "status": "fail",
            "detail": f"未找到文件: {source_rel_path}",
        }]

    text = source_path.read_text(encoding="utf-8", errors="ignore")
    lines = text.splitlines()
    sep_lines = [idx + 1 for idx, line in enumerate(lines) if line.strip() == "%%"]
    checks.append({
        "id": "segment_markers",
        "label": "三段结构分隔符(%%)数量",
        "status": "pass" if len(sep_lines) >= 2 else "fail",
        "detail": f"找到 {len(sep_lines)} 处，行号: {sep_lines[:6]}",
    })

    defs_text = text
    rules_text = ""
    user_text = ""
    if len(sep_lines) >= 2:
        a = sep_lines[0] - 1
        b = sep_lines[1] - 1
        defs_text = "\n".join(lines[:a])
        rules_text = "\n".join(lines[a + 1:b])
        user_text = "\n".join(lines[b + 1:])

    start_lines = [idx + 1 for idx, line in enumerate(lines) if line.strip().startswith("%start")]
    checks.append({
        "id": "start_decl",
        "label": "%start 声明",
        "status": "pass" if len(start_lines) == 1 else ("warn" if len(start_lines) > 1 else "fail"),
        "detail": f"数量={len(start_lines)}，行号: {start_lines[:6]}",
    })

    unsupported = []
    unsupported_keys = ("%left", "%right", "%nonassoc", "%union", "%type", "%prec")
    for idx, line in enumerate(lines):
        s = line.strip()
        for key in unsupported_keys:
            if s.startswith(key):
                unsupported.append(f"{key}@L{idx+1}")
                break
    checks.append({
        "id": "unsupported_directives",
        "label": "未支持指令扫描",
        "status": "warn" if unsupported else "pass",
        "detail": "无" if not unsupported else ", ".join(unsupported[:20]),
    })

    has_rules = bool(rules_text.strip())
    checks.append({
        "id": "rules_section_nonempty",
        "label": "Rules 区非空",
        "status": "pass" if has_rules else "fail",
        "detail": "rules 区存在内容" if has_rules else "rules 区为空",
    })

    has_user = bool(user_text.strip())
    checks.append({
        "id": "user_subroutines_section",
        "label": "User Subroutines 区",
        "status": "pass" if has_user else "warn",
        "detail": "存在用户代码区" if has_user else "为空（可接受）",
    })
    return checks


def write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def collect_step_payload(paths: Paths, case_id: str, step: int) -> Dict[str, object]:
    step_dir = resolve_step_dir(paths, case_id, step)
    if step_dir is None:
        step_dir = paths.artifacts_root / f"step{step}" / case_id

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

    if step == 1:
        payload["step1_overview"] = load_source_preview(paths.repo_root, source)
        payload["step1_baseline"] = {
            "symbols": int(summary.get("symbols", "0") or "0"),
            "terminals": int(summary.get("terminals", "0") or "0"),
            "nonterminals": int(summary.get("nonterminals", "0") or "0"),
            "productions_with_augmented": int(summary.get("productions_with_augmented", "0") or "0"),
        }
        payload["input_spec_checks"] = build_input_spec_checks(paths.repo_root, source)

    if step == 2:
        symbols = normalize_symbols(parse_tsv(raw_dir / "symbols.tsv"))
        productions = strip_augmented_productions(parse_productions(raw_dir / "productions.txt"))
        terminals = [s for s in symbols if s.get("kind") == "Terminal"]
        nonterminals = [s for s in symbols if s.get("kind") == "NonTerminal"]
        payload["grammar_model"] = {
            "start_symbol": summary.get("start_symbol", ""),
            "terminals": terminals,
            "nonterminals": nonterminals,
            "productions_count": len(productions),
        }

    # 为防止 step8/step9 体积过大导致前端卡死，按步骤最小化装载字段。
    # 各页面只读取本步骤核心字段，不再把前序步骤大对象累加进当前步骤。
    if step == 3:
        symbols = strip_augmented_symbols(normalize_symbols(parse_tsv(raw_dir / "symbols.tsv")))
        productions = strip_augmented_productions(parse_productions(raw_dir / "productions.txt"))
        if symbols:
            payload["symbols"] = symbols
        if productions:
            payload["productions"] = productions

    if step == 4:
        payload["augmented"] = {"production": read_augmented_text(raw_dir / "augmented_grammar.txt")}
        payload["prod_index_by_lhs"] = normalize_prod_index(parse_tsv(raw_dir / "prod_index_by_lhs.tsv"))
        payload["preprocess"] = {
            "passed": summary.get("preprocess_passed", analysis.get("preprocess_passed", "")),
            "errors_count": int(analysis.get("preprocess_errors_count", "0") or "0"),
            "warnings_count": int(analysis.get("preprocess_warnings_count", "0") or "0"),
        }

    if step == 5:
        payload["first_sets"] = normalize_first(parse_tsv(raw_dir / "first_sets.tsv"))

    if step == 6:
        lr1_items = parse_lr1_items(raw_dir / "lr1_i0_items.txt")
        i0_targets: Dict[str, int] = {}
        step7_dir = resolve_step_dir(paths, case_id, 7)
        step7_raw_dir = (step7_dir / "raw") if step7_dir else (paths.artifacts_root / "step7" / case_id / "raw")
        for row in parse_tsv(step7_raw_dir / "lr1_transitions.tsv"):
            from_state = int(row.get("from_state", "0"))
            if from_state != 0:
                continue
            symbol_name = row.get("symbol_name", "")
            if not symbol_name:
                continue
            i0_targets[symbol_name] = int(row.get("to_state", "-1"))
        payload["lr1_i0"] = {
            "kernel_items": lr1_items["kernel"],
            "closure_items": lr1_items["closure"],
            "goto_edges": normalize_goto(parse_tsv(raw_dir / "lr1_i0_goto.tsv"), i0_targets),
            "goto_items": parse_goto_items(raw_dir / "lr1_i0_goto_items.txt"),
            "lookahead_notes": parse_notes(raw_dir / "lr1_lookahead_derivation.txt"),
        }
    if step == 7:
        payload["lr1_canonical"] = {
            "states": normalize_lr1_states(parse_tsv(raw_dir / "lr1_states.tsv")),
            "state_items": parse_state_items(raw_dir / "lr1_state_items.txt"),
            "transitions": normalize_lr1_transitions(parse_tsv(raw_dir / "lr1_transitions.tsv")),
            "predecessors": normalize_predecessors(parse_tsv(raw_dir / "lr1_predecessors.tsv")),
        }
    if step == 8:
        payload["parse_table"] = {
            "action_rows": normalize_action_table(parse_tsv(raw_dir / "action_table.tsv")),
            "goto_rows": normalize_goto_table(parse_tsv(raw_dir / "goto_table.tsv")),
            "conflicts": normalize_parse_conflicts(parse_tsv(raw_dir / "parse_table_conflicts.tsv")),
            "conflict_resolutions": normalize_conflict_resolutions(
                parse_tsv(raw_dir / "parse_table_conflict_resolution.tsv")
            ),
        }
    if step == 9:
        payload["parse_runtime"] = {
            "input_tokens": normalize_parse_input_tokens(parse_tsv(raw_dir / "parse_input_tokens.tsv")),
            "lr1_trace_rows": normalize_parse_trace(parse_tsv(raw_dir / "parse_trace.tsv")),
            "lalr_trace_rows": normalize_parse_trace(parse_tsv(raw_dir / "parse_trace_lalr.tsv")),
            "lr1_reductions": parse_reductions(raw_dir / "parse_reductions.txt"),
            "lalr_reductions": parse_reductions(raw_dir / "parse_reductions_lalr.txt"),
            "lr1_error": parse_error_kv(raw_dir / "parse_error.txt"),
            "lalr_error": parse_error_kv(raw_dir / "parse_error_lalr.txt"),
            "lalr_ast_json": parse_ast_json(raw_dir / "ast_lalr.json"),
            "lalr_ast_text": read_text(raw_dir / "ast_lalr.txt"),
        }
    if step == 10:
        payload["lalr"] = {
            "state_map": normalize_state_map(parse_tsv(raw_dir / "lr1_to_lalr_state_map.tsv")),
            "merge_groups": normalize_merge_groups(parse_tsv(raw_dir / "lalr_merge_groups.tsv")),
            "state_items": parse_state_items(raw_dir / "lalr_state_items.txt"),
            "transitions": normalize_lr1_transitions(parse_tsv(raw_dir / "lalr_transitions.tsv")),
            "action_rows": normalize_action_table(parse_tsv(raw_dir / "lalr_action_table.tsv")),
            "goto_rows": normalize_goto_table(parse_tsv(raw_dir / "lalr_goto_table.tsv")),
            "conflicts": normalize_parse_conflicts(parse_tsv(raw_dir / "lalr_parse_table_conflicts.tsv")),
            "conflict_resolutions": normalize_conflict_resolutions(
                parse_tsv(raw_dir / "lalr_parse_table_conflict_resolution.tsv")
            ),
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
        "assets": {
            "lex": "lexer.l",
            "input": "input.c",
            "jimple": "output.jimple",
        },
    }

    step_titles = {
        1: "输入文件与词法符号基线",
        2: "文法模型初始化与合法性校验",
        3: "输入解析结果",
        4: "文法预处理与增广",
        5: "First 集计算",
        6: "LR(1) I0 闭包与 Goto",
        7: "LR(1) 规范族与状态转移图",
        8: "Action/Goto 分析表与冲突处理",
        9: "LR 总控程序执行与解析轨迹",
        10: "LR(1) 到 LALR(1) 合并与对比",
    }

    for step in steps:
        step_dir = resolve_step_dir(paths, case_id, step)
        if step_dir is None:
            continue
        payload = collect_step_payload(paths, case_id, step)
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
        "--steps", default="1,2,3,4,5,6,7,8,9,10", help="处理步骤列表，逗号分隔，默认 1,2,3,4,5,6,7,8,9,10"
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
