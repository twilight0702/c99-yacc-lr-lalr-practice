#pragma once

#include <string>
#include <vector>

#include "yacc/model/grammar.h"

namespace seu::yacc {

// 解析结果分析：用于识别潜在质量问题，辅助后续阶段调试。
struct GrammarAnalysis {
    std::vector<int> unused_terminal_ids;         // 声明/注册但在 RHS 未使用的终结符
    std::vector<int> unreachable_nonterminal_ids; // 从开始符号不可达的非终结符
    int productions_with_actions = 0;             // 含动作块的产生式数量
};

GrammarAnalysis analyze_grammar(const Grammar& grammar);

// 导出目录约定：artifacts/yacc/step3/<input_stem>/
std::string make_default_export_dir(const std::string& input_path);

// 将解析结果和分析结果导出到结构化目录。
void export_report(
    const Grammar& grammar, const GrammarAnalysis& analysis, const std::string& output_dir);

}  // namespace seu::yacc

