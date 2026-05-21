/*
 * parse_table.h — LR 分析表 (Action/Goto 表) 构造
 *
 * 第 8 步核心模块：基于 LR(1) 项目集规范族构造 Action 表和 Goto 表。
 * LR 分析表的两个核心组成部分：
 *   - Action 表:  action_table[state][terminal] → 移进/归约/接受
 *   - Goto 表:    goto_table[state][nonterminal] → 目标状态
 *
 * 同时也处理 LR 分析表构造中的冲突检测与消解：
 *   - Shift/Reduce 冲突：同一状态同一终结符既可移进也可归约
 *   - Reduce/Reduce 冲突：同一状态同一终结符有多个可能的归约产生式
 *
 * 默认冲突消解策略：Shift/Reduce 冲突优先移进（符合大多数编程语言语义）。
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "yacc/lr1/lr1_items.h"
#include "yacc/model/grammar.h"

namespace seu::yacc {

/*
 * ParseActionType — 分析动作类型枚举
 * Shift:  移进（将输入符号压栈，跳转到目标状态）
 * Reduce: 归约（按产生式 A → β 将栈顶 |β| 个符号归约为 A）
 * Accept: 接受（解析成功结束）
 */
enum class ParseActionType {
    Shift,
    Reduce,
    Accept
};

/*
 * ParseActionEntry — Action 表的一个表项
 *
 * type:                 动作类型（Shift / Reduce / Accept）
 * target_state_id:      Shift 时的目标状态编号
 * reduce_production_id: Reduce 时的归约产生式编号
 */
struct ParseActionEntry {
    ParseActionType type = ParseActionType::Shift;
    int target_state_id = -1;
    int reduce_production_id = -1;
};

/*
 * ParseTableConflict — 分析表构造过程中检测到的冲突
 *
 * state_id:        发生冲突的状态编号
 * symbol_id:       触发冲突的符号编号
 * conflict_type:   冲突类型字符串（如 "shift/reduce"、"reduce/reduce"）
 * existing_action: 已记录在表中的旧动作描述
 * incoming_action: 尝试写入的新动作描述
 * related_items:   冲突相关的 LR(1) 项（用于诊断）
 */
struct ParseTableConflict {
    int state_id = -1;
    int symbol_id = -1;
    std::string conflict_type;
    std::string existing_action;
    std::string incoming_action;
    std::vector<LR1Item> related_items;
};

/*
 * ParseTableConflictResolutionLog — 冲突消解日志
 *
 * 记录最终采用的动作及消解原因，便于：
 *   1. 复现冲突处理过程
 *   2. 在答辩和报告中展示消解策略
 *   3. 发现消解策略引入的问题
 */
struct ParseTableConflictResolutionLog {
    int state_id = -1;
    int symbol_id = -1;
    std::string conflict_type;
    std::string existing_action;
    std::string incoming_action;
    std::string resolved_action;
    std::string reason;
};

/*
 * LR1Step8Result — 第 8 步完整输出
 *
 * action_table:               Action 表：action_table[state][terminal] → action
 * goto_table:                 Goto 表：goto_table[state][nonterminal] → 目标状态
 * conflicts:                  构造过程中检测到的所有冲突
 * conflict_resolution_logs:   每个冲突的消解日志
 */
struct LR1Step8Result {
    std::vector<std::unordered_map<int, ParseActionEntry>> action_table;
    std::vector<std::unordered_map<int, int>> goto_table;
    std::vector<ParseTableConflict> conflicts;
    std::vector<ParseTableConflictResolutionLog> conflict_resolution_logs;
};

/*
 * LR1Step8ValidationReport — 第 8 步校验报告
 *
 * 校验内容：
 *   1. 表项合法性（状态号、产生式号均在有效范围）
 *   2. 与状态转移的一致性（Shift 目标状态与 goto 一致）
 *   3. 与归约项的一致性（Reduce 产生式对应点在最右端的项）
 *   4. 冲突统计信息可追踪
 */
struct LR1Step8ValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/*
 * build_step8_lr1_parsing_table — 构造 LR(1) 分析表
 *
 * 构造规则：
 *   对每个状态 i 中的每个项：
 *     - 若项为 [A → α·aβ, b] 且 a 是终结符：
 *         在 action_table[i][a] 填入 Shift goto(i, a)
 *     - 若项为 [A → α·, a] 且 A ≠ S'：
 *         在 action_table[i][a] 填入 Reduce A → α
 *     - 若项为 [S' → S·, $]：
 *         在 action_table[i][$] 填入 Accept
 *
 *   对每个状态 i 和每个非终结符 A：
 *     若 goto(i, A) = j，则 goto_table[i][A] = j
 *
 * 冲突处理：
 *   默认采用 Shift 优先策略解决 Shift/Reduce 冲突
 *   记录所有冲突和消解日志
 *
 * 参数:
 *   grammar:       文法结构
 *   step7_result:  已构造的 LR(1) 规范族
 *
 * 返回:
 *   包含 Action 表、Goto 表和冲突信息的结果。
 */
LR1Step8Result build_step8_lr1_parsing_table(
    const Grammar& grammar, const LR1Step7Result& step7_result);

/*
 * validate_step8_lr1_parsing_table — 校验第 8 步分析表
 *
 * 参数:
 *   grammar:       文法结构
 *   step7_result:  LR(1) 规范族
 *   step8_result:  待校验的分析表
 *
 * 返回:
 *   校验报告。
 */
LR1Step8ValidationReport validate_step8_lr1_parsing_table(const Grammar& grammar,
    const LR1Step7Result& step7_result, const LR1Step8Result& step8_result);

/*
 * format_parse_action_entry — 将分析动作格式化为可读字符串
 *
 * 渲染格式：
 *   Shift → "s23"       (移进到状态 23)
 *   Reduce → "r45"      (按产生式 45 归约)
 *   Accept → "acc"      (接受)
 *
 * 参数:
 *   entry: 待格式化的动作表项
 *
 * 返回:
 *   人类可读的字符串描述。
 */
std::string format_parse_action_entry(const ParseActionEntry& entry);

}  // namespace seu::yacc
