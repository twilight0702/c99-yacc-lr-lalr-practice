/*
 * lr1_items.h — LR(1) 项目集规范族构造
 *
 * 第 6/7 步核心模块：构造 LR(1) 项目集规范族（canonical collection of LR(1) items）。
 * LR(1) 分析器的基础数据结构包括：
 *   - LR(1) 项 (item)：   A → α·β, a  （a 为展望符/lookahead）
 *   - LR(1) 状态 (state)： 一组 LR(1) 项的集合
 *   - LR(1) 转移 (transition)： 状态间通过符号的转移
 *
 * 构造过程分为两步：
 *   第 6 步：构造初始状态 I0 及其闭包，验证基本操作的正确性
 *   第 7 步：从 I0 出发，通过 goto 操作迭代构造完整的项目集规范族
 *
 * 核心操作：
 *   - closure(I):  若 [A → α·Bβ, a] ∈ I，则对 B 的每条产生式 B → γ
 *                   和每个 b ∈ First(βa)，将 [B → ·γ, b] 加入闭包
 *   - goto(I, X):  从 I 中所有形如 [A → α·Xβ, a] 的项，构造新核心
 *                   [A → αX·β, a]，再取闭包
 */

#pragma once

#include <string>
#include <vector>

#include "yacc/first/first_set.h"
#include "yacc/model/grammar.h"

namespace seu::yacc {

/*
 * LR1Item — 单个 LR(1) 项
 *
 * A → α · β, a  其中:
 *   production_id:        产生式编号（确定 A → αβ）
 *   dot_pos:              圆点位置，取值范围 [0, rhs_size]
 *                         表示已经识别了 α（0..dot_pos-1），尚未识别 β（dot_pos..）
 *   lookahead_symbol_id:  展望符（必须是终结符或 $），标识该产生式可被归约的
 *                         上下文条件
 */
struct LR1Item {
    int production_id = -1;
    int dot_pos = 0;
    int lookahead_symbol_id = -1;
};

/*
 * LR1Step6Result — 第 6 步输出：I0 的构造与转移
 *
 * i0_kernel_items:            I0 的核心项（初始为 [S' → ·S, $]）
 * i0_closure_items:           I0 的完整闭包（核心项 + closure 展开）
 * goto_symbol_ids:            从 I0 出发可转移的符号列表
 * goto_item_sets:             与 goto_symbol_ids 一一对应，每个是对应
 *                             goto(I0, X) 的项集结果
 * lookahead_derivation_notes: 展望符派生过程的人类可读说明（用于验收与调试）
 */
struct LR1Step6Result {
    std::vector<LR1Item> i0_kernel_items;
    std::vector<LR1Item> i0_closure_items;

    std::vector<int> goto_symbol_ids;
    std::vector<std::vector<LR1Item>> goto_item_sets;

    std::vector<std::string> lookahead_derivation_notes;
};

/*
 * LR1Step6ValidationReport — 第 6 步校验报告
 *
 * 校验重点：
 *   1. 所有项的合法性（production_id 有效，dot_pos 在合法范围）
 *   2. 闭包完备性（对每个可展开的非终结符，其所有产生式已加入）
 *   3. 去重一致性（同一项不会在项集中重复出现）
 */
struct LR1Step6ValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/*
 * LR1State — LR(1) 状态（项目集规范族中的一个节点）
 *
 * state_id: 状态在规范族中的唯一编号
 * items:    该状态中包含的 LR(1) 项列表
 */
struct LR1State {
    int state_id = -1;
    std::vector<LR1Item> items;
};

/*
 * LR1Transition — LR(1) 状态转移边
 *
 * from_state_id: 源状态 ID
 * symbol_id:     驱动转移的符号 ID
 * to_state_id:   目标状态 ID
 */
struct LR1Transition {
    int from_state_id = -1;
    int symbol_id = -1;
    int to_state_id = -1;
};

/*
 * LR1Step7Result — 第 7 步输出：完整 LR(1) 规范族
 *
 * states:                          所有 LR(1) 状态列表（按状态 ID 索引）
 * transitions:                     所有状态转移边列表
 * transition_target_item_set_keys: 与 transitions 一一对应，记录目标项集的
 *                                  标识键值。用于校验阶段避免重复执行 goto 计算。
 */
struct LR1Step7Result {
    std::vector<LR1State> states;
    std::vector<LR1Transition> transitions;
    std::vector<std::string> transition_target_item_set_keys;
};

/*
 * LR1Step7ValidationReport — 第 7 步校验报告
 *
 * 校验重点：
 *   1. 规范族完整性（从 I0 出发，所有可达状态均已生成）
 *   2. 状态去重一致性（核心相同的状态被正确识别和合并）
 *   3. goto 转移正确性（每次转移的目标状态与独立计算一致）
 */
struct LR1Step7ValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/*
 * build_step6_lr1_items — 构建第 6 步：I0 及其 goto
 *
 * 执行步骤：
 *   1. 创建初始核心项 [S' → ·S, $]
 *   2. 计算 closure([S' → ·S, $]) 得到 I0
 *   3. 对每个从 I0 出发可转移的符号 X，计算 goto(I0, X)
 *
 * 参数:
 *   grammar:      已完成预处理的文法
 *   first_result: 已计算好的 First 集结果
 *
 * 返回:
 *   第 6 步结果，包含 I0、I0 的闭包和 goto 计算结果。
 */
LR1Step6Result build_step6_lr1_items(
    const Grammar& grammar, const FirstSetResult& first_result);

/*
 * validate_step6_lr1_items — 校验第 6 步结果
 *
 * 参数:
 *   grammar:      文法结构
 *   first_result: First 集结果
 *   result:       待校验的第 6 步结果
 *
 * 返回:
 *   校验报告。
 */
LR1Step6ValidationReport validate_step6_lr1_items(
    const Grammar& grammar, const FirstSetResult& first_result, const LR1Step6Result& result);

/*
 * build_step7_lr1_canonical_collection — 构建完整 LR(1) 项目集规范族
 *
 * 算法：
 *   1. 从 I0 = closure([S' → ·S, $]) 开始
 *   2. 对每个尚未处理的状态 I，对每个文法符号 X：
 *      计算 J = goto(I, X)
 *      若 J 非空：
 *        - 若 J 的核心与已有状态相同，则复用该状态
 *        - 否则创建新状态 J
 *      添加转移 I --X--> J
 *   3. 重复直到没有新状态产生
 *
 * 参数:
 *   grammar:      已完成预处理的文法
 *   first_result: First 集计算结果
 *   step6_hint:   可选的第 6 步结果（提供已计算好的 I0，避免重复计算）
 *
 * 返回:
 *   完整的 LR(1) 项目集规范族。
 */
LR1Step7Result build_step7_lr1_canonical_collection(
    const Grammar& grammar, const FirstSetResult& first_result, const LR1Step6Result* step6_hint = nullptr);

/*
 * validate_step7_lr1_canonical_collection — 校验第 7 步结果
 *
 * 参数:
 *   grammar:      文法结构
 *   first_result: First 集计算结果
 *   result:       待校验的规范族
 *
 * 返回:
 *   校验报告。
 */
LR1Step7ValidationReport validate_step7_lr1_canonical_collection(
    const Grammar& grammar, const FirstSetResult& first_result, const LR1Step7Result& result);

/*
 * format_lr1_item — 将 LR(1) 项格式化为可读字符串
 *
 * 渲染格式示例: [S → expr · + term, $]
 *
 * 参数:
 *   grammar: 文法结构（用于符号名查找）
 *   item:    待格式化的 LR(1) 项
 *
 * 返回:
 *   人类可读的格式化字符串。
 */
std::string format_lr1_item(const Grammar& grammar, const LR1Item& item);

}  // namespace seu::yacc
