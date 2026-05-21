/*
 * lalr_builder.h — LALR(1) 分析表构造器
 *
 * 第 10 步核心模块：从已构造的 LR(1) 项目集规范族出发，通过合并具有相同
 * LR(0) 核的状态，构造 LALR(1) 项目集和分析表。
 *
 * LALR(1) 的核心思想：
 *   - LR(1) 的每个状态包含项 [A → α·β, a]，其中 a 是展望符
 *   - 将具有相同 [A → α·β] 核心（忽略展望符 a）的 LR(1) 状态合并为一个 LALR 状态
 *   - LALR 的表大小与 SLR 相同（远小于 LR(1)），但保留了 LR(1) 的大部分能力
 *
 * 本模块包括：
 *   - LR(1) 状态到 LALR 状态的合并映射
 *   - LALR 状态机的构造
 *   - LALR Action/Goto 分析表的生成
 *   - 合并前后冲突数量变化的统计与对比
 */

#pragma once

#include <string>
#include <vector>

#include "yacc/lr1/lr1_items.h"
#include "yacc/model/grammar.h"
#include "yacc/table/parse_table.h"

namespace seu::yacc {

/*
 * LALRMergeGroup — 单个 LALR 状态的合并来源
 *
 * lalr_state_id:        合并后的 LALR 状态编号
 * source_lr1_state_ids: 被合并到该 LALR 状态的原始 LR(1) 状态编号列表
 * merged_items:         合并后的项集（取所有来源项的并集）
 */
struct LALRMergeGroup {
    int lalr_state_id = -1;
    std::vector<int> source_lr1_state_ids;
    std::vector<LR1Item> merged_items;
};

/*
 * LR1Step10Result — 第 10 步完整输出
 *
 * lr1_to_lalr_state_id:  从 LR(1) 状态号到 LALR 状态号的映射表
 * merge_groups:           每个 LALR 状态的合并来源详情
 * lalr_step7_result:      合并后的 LALR 状态机（结构与 step7 对齐）
 * lalr_step8_result:      基于 LALR 状态构造的 Action/Goto 分析表
 * lr1_state_count:        原始 LR(1) 状态总数
 * lalr_state_count:       LALR 合并后状态总数
 * lr1_conflict_count:     原始 LR(1) 冲突数量
 * lalr_conflict_count:    LALR 合并后冲突数量（通常更多，因合并导致展望符集合扩大）
 */
struct LR1Step10Result {
    std::vector<int> lr1_to_lalr_state_id;
    std::vector<LALRMergeGroup> merge_groups;

    LR1Step7Result lalr_step7_result;
    LR1Step8Result lalr_step8_result;

    std::size_t lr1_state_count = 0;
    std::size_t lalr_state_count = 0;
    std::size_t lr1_conflict_count = 0;
    std::size_t lalr_conflict_count = 0;
};

/*
 * LR1Step10ValidationReport — 第 10 步校验报告
 *
 * 校验内容：
 *   1. 每个 LR(1) 状态都已映射到某个 LALR 状态
 *   2. LALR 状态转移函数是确定的（同一输入符号只对应唯一目标状态）
 *   3. LALR Action/Goto 表合法（表项完整且与状态机一致）
 *   4. 冲突统计信息可追踪
 */
struct LR1Step10ValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/*
 * build_step10_lalr_from_lr1 — 从 LR(1) 构造 LALR(1)
 *
 * 算法步骤：
 *   1. 为每个 LR(1) 状态计算其 LR(0) 核（即去掉展望符的项集）
 *   2. 将核相同的 LR(1) 状态分配到同一合并组
 *   3. 每个合并组形成一个 LALR 状态，其项集为来源项集的并集
 *   4. 基于合并后的 LALR 状态机构造 Action/Goto 表
 *
 * 参数:
 *   grammar:           文法结构
 *   lr1_step7_result:  已构造的 LR(1) 状态机
 *   lr1_step8_result:  已构造的 LR(1) 分析表
 *
 * 返回:
 *   完整的合并结果，包括状态映射、LALR 状态机和分析表。
 */
LR1Step10Result build_step10_lalr_from_lr1(
    const Grammar& grammar, const LR1Step7Result& lr1_step7_result, const LR1Step8Result& lr1_step8_result);

/*
 * validate_step10_lalr_from_lr1 — 校验第 10 步合并结果
 *
 * 校验要点：
 *   1. 每个 LR(1) 状态都在 lr1_to_lalr_state_id 中有对应的映射
 *   2. LALR 状态间的转移是确定的（无二义性）
 *   3. LALR Action/Goto 表与合并后的状态机一致
 *   4. 冲突统计信息完整且可追踪
 *
 * 参数:
 *   grammar:           文法结构
 *   lr1_step7_result:  原始 LR(1) 状态机
 *   lr1_step8_result:  原始 LR(1) 分析表
 *   step10_result:     已构造的 LALR 合并结果
 *
 * 返回:
 *   校验报告，passed == true 表示合并结果合法。
 */
LR1Step10ValidationReport validate_step10_lalr_from_lr1(const Grammar& grammar,
    const LR1Step7Result& lr1_step7_result, const LR1Step8Result& lr1_step8_result,
    const LR1Step10Result& step10_result);

}  // namespace seu::yacc
