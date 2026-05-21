/*
 * grammar_preprocessor.h — 文法预处理
 *
 * 第 4 步核心模块：对解析完成的 Grammar 进行预处理和结构校验，
 * 确保文法满足后续 First 集 / LR 算法所需的前置条件。
 *
 * 预处理操作包括：
 *   1. 重建 lhs → productions 索引（确保 prod_ids_by_lhs 完整）
 *   2. 校验增广文法的完整性（S'、$ 等特殊符号存在且正确）
 *   3. 检查文法结构一致性（无悬挂符号引用，无零产生式非终结符等）
 *   4. 补充后续算法需要的结构信息
 */

#pragma once

#include <string>
#include <vector>

#include "yacc/model/grammar.h"

namespace seu::yacc {

/*
 * GrammarPreprocessReport — 预处理报告
 *
 * passed:                              预处理是否通过（所有必需条件满足）
 * augmented_production_id:             增广产生式 S' → S 的 ID
 * nonterminals_without_productions:    没有任何产生式的非终结符列表（可能导致解析异常）
 * productions_with_epsilon_symbol_in_rhs:  产生式右部包含 epsilon 符号的产生式列表
 * warnings:  不影响算法执行但可能指示问题的警告
 * errors:    阻止算法继续执行的严重错误
 */
struct GrammarPreprocessReport {
    bool passed = false;
    int augmented_production_id = -1;

    std::vector<int> nonterminals_without_productions;
    std::vector<int> productions_with_epsilon_symbol_in_rhs;

    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

/*
 * preprocess_grammar — 对文法执行预处理操作
 *
 * 执行步骤：
 *   1. 调用 prod_ids_by_lhs 重建索引，确保每个非终结符到其产生式的查找有效
 *   2. 验证增广文法结构：S'、$、epsilon 等特殊符号都已正确设置
 *   3. 检查每个非终结符至少有一条产生式
 *   4. 检查产生式右部中是否有 epsilon 符号（应通过空产生式表示）
 *   5. 补充文法结构一致性信息
 *
 * 参数:
 *   grammar: 待预处理的文法（传入/传出，将被原地修改）
 *
 * 返回:
 *   预处理报告。若 passed == false，不应继续进行后续算法步骤。
 */
GrammarPreprocessReport preprocess_grammar(Grammar& grammar);

}  // namespace seu::yacc
