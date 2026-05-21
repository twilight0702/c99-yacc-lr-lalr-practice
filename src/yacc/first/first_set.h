/*
 * first_set.h — First 集计算
 *
 * 第 5 步核心模块：对给定的上下文无关文法，计算每个文法符号的 First 集。
 * First 集是 LL/LR 语法分析的基础：
 *   - First(X) = { a | X ⇒* aβ, a 是终结符 }
 *
 * 本模块同时提供：
 *   - 符号串的 First 集计算（用于 LR(1) 闭包中的 lookahead 传播）
 *   - 可空非终结符集合的确定
 *   - First 集正确性校验
 *
 * First 集计算采用不动点迭代算法：不断合并各符号的 First 贡献，
 * 直到所有符号的 First 集不再增大（收敛）为止。
 */

#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "yacc/model/grammar.h"

namespace seu::yacc {

/*
 * FirstSetResult — First 集计算结果
 *
 * converged:                       迭代是否收敛（若未收敛则表明算法有误）
 * iteration_count:                 达到收敛所用的迭代次数
 * first_sets_by_symbol_id:         first_sets_by_symbol_id[sym_id] = 该符号的 First 集
 * nullable_nonterminal_ids:        可推导出空串的非终结符 ID 列表
 */
struct FirstSetResult {
    bool converged = false;
    int iteration_count = 0;
    std::vector<std::set<int>> first_sets_by_symbol_id;
    std::vector<int> nullable_nonterminal_ids;
};

/*
 * FirstSetValidationReport — First 集校验报告
 *
 * passed:    校验是否通过
 * errors:    严重违反不变量的问题列表
 * warnings:  可能影响后续步骤的潜在问题列表
 */
struct FirstSetValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/*
 * compute_first_sets — 计算整套文法的 First 集
 *
 * 参数:
 *   grammar: 已完成预处理的文法
 *
 * 返回:
 *   包含所有符号 First 集和可空非终结符集合的计算结果。
 *
 * 算法要点:
 *   1. 初始化：所有终结符的 First 集 = {自身}；非终结符为空集
 *   2. 迭代：对每条产生式 A → X1 X2 ... Xn，逐符号合并 First(Xi) 至 First(A)
 *      若 Xi 可空则继续取 First(Xi+1)，直到遇到不可空符号或取完整条产生式
 *   3. 重复直到 First 集不再增长
 */
FirstSetResult compute_first_sets(const Grammar& grammar);

/*
 * compute_first_of_sequence — 计算符号串的 First 集
 *
 * 用于 LR(1) 闭包运算中计算展望符：当 A → α · B β 时，
 * 需要计算 First(βa) 作为 B 的产生式的展望符。
 *
 * 参数:
 *   grammar:       文法结构
 *   result:        已计算好的 First 集结果
 *   symbol_ids:    符号 ID 序列
 *   start_index:   从序列的哪个位置开始计算（默认为 0）
 *
 * 返回:
 *   First(symbol_ids[start_index] symbol_ids[start_index+1] ...)
 *   若 start_index 超出范围则返回空集。
 */
std::set<int> compute_first_of_sequence(const Grammar& grammar, const FirstSetResult& result,
    const std::vector<int>& symbol_ids, std::size_t start_index = 0);

/*
 * validate_first_sets — 校验 First 集正确性
 *
 * 检查的关键不变量：
 *   1. 每个终结符的 First 集 = {自身}
 *   2. 若 A ⇒* ε，则 ε 在 First(A) 中（或 A 在 nullable 列表中）
 *   3. 所有 First 集不含有 ε（epsilon 通过 nullable 集合单独管理）
 *   4. 对每条产生式 A → X1...Xn，First(X1...Xn) ⊆ First(A)
 *
 * 参数:
 *   grammar: 文法结构
 *   result:  已计算好的 First 集结果
 *
 * 返回:
 *   校验报告，passed == true 表示满足所有不变量。
 */
FirstSetValidationReport validate_first_sets(const Grammar& grammar, const FirstSetResult& result);

}  // namespace seu::yacc
