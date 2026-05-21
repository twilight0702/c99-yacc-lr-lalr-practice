/*
 * report_exporter.h — 各步骤结果导出器
 *
 * 本模块负责将 YACC 后端流水线各步骤（第 3-10 步）的中间结果，
 * 按结构化的目录约定导出为可读文本文件（.txt / .md 等格式）。
 * 导出的内容包括：
 *   - 文法分析报告（符号、产生式、语义动作统计）
 *   - First 集（每个符号的 First 集、可空非终结符列表）
 *   - LR(1) 项目集（I0 的闭包和 goto、完整规范族、状态转移图）
 *   - Action/Goto 分析表（含冲突报告和消解日志）
 *   - 解析执行 trace（移进/归约步骤序列）
 *   - LALR(1) 合并结果（合并映射、状态规模对比）
 *
 * 目录约定：
 *   每步的导出目录为 artifacts/yacc/step<N>/<input_stem>/
 *   其中 input_stem 为输入文件名去除路径和扩展名的部分
 */

#pragma once

#include <string>
#include <vector>

#include "yacc/first/first_set.h"
#include "yacc/lr1/lr1_items.h"
#include "yacc/lalr/lalr_builder.h"
#include "yacc/model/grammar.h"
#include "yacc/preprocess/grammar_preprocessor.h"
#include "yacc/runtime/lr_parser.h"
#include "yacc/table/parse_table.h"

namespace seu::yacc {

/*
 * GrammarAnalysis — 文法质量分析结果
 *
 * unused_terminal_ids:          在 %token 中声明但不在任何产生式右部出现的终结符
 * unreachable_nonterminal_ids:  从开始符号不可达的非终结符（可能为遗留定义）
 * productions_with_actions:     包含语义动作代码块的产生式数量
 */
struct GrammarAnalysis {
    std::vector<int> unused_terminal_ids;
    std::vector<int> unreachable_nonterminal_ids;
    int productions_with_actions = 0;
};

/*
 * analyze_grammar — 对文法进行质量分析
 *
 * 检测未使用的终结符、不可达的非终结符等，辅助发现文法定义中的潜在问题。
 *
 * 参数:
 *   grammar: 已完成解析的文法结构
 *
 * 返回:
 *   包含各分析指标的 GrammarAnalysis 结构。
 */
GrammarAnalysis analyze_grammar(const Grammar& grammar);

/*
 * make_default_export_dir — 第 3 步导出目录路径
 * 约定: artifacts/yacc/step3/<input_stem>/
 */
std::string make_default_export_dir(const std::string& input_path);

/*
 * export_report — 导出第 3 步解析结果
 *
 * 导出文法符号表、产生式列表、动作代码等解析阶段产出。
 */
void export_report(
    const Grammar& grammar, const GrammarAnalysis& analysis, const std::string& output_dir);

/*
 * make_default_step4_export_dir — 第 4 步导出目录路径
 * 约定: artifacts/yacc/step4/<input_stem>/
 */
std::string make_default_step4_export_dir(const std::string& input_path);

/*
 * export_step4_report — 导出第 4 步文法预处理结果
 *
 * 导出增广文法校验、索引重建结果、结构一致性报告等。
 */
void export_step4_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const std::string& output_dir);

/*
 * make_default_step5_export_dir — 第 5 步导出目录路径
 * 约定: artifacts/yacc/step5/<input_stem>/
 */
std::string make_default_step5_export_dir(const std::string& input_path);

/*
 * export_step5_report — 导出第 5 步 First 集结果
 *
 * 导出每个符号的 First 集、可空非终结符列表、迭代收敛信息等。
 */
void export_step5_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const std::string& output_dir);

/*
 * make_default_step6_export_dir — 第 6 步导出目录路径
 * 约定: artifacts/yacc/step6/<input_stem>/
 */
std::string make_default_step6_export_dir(const std::string& input_path);

/*
 * export_step6_report — 导出第 6 步 I0 构造和 goto 结果
 *
 * 导出 I0 核心项、闭包项、goto(I0, X) 转移目标、展望符派生过程等。
 */
void export_step6_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_result,
    const LR1Step6ValidationReport& lr1_validation, const std::string& output_dir);

/*
 * make_default_step7_export_dir — 第 7 步导出目录路径
 * 约定: artifacts/yacc/step7/<input_stem>/
 */
std::string make_default_step7_export_dir(const std::string& input_path);

/*
 * export_step7_report — 导出第 7 步完整 LR(1) 项目集规范族
 *
 * 导出所有状态的项集、状态转移图、转移验证信息等。
 */
void export_step7_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const std::string& output_dir);

/*
 * make_default_step8_export_dir — 第 8 步导出目录路径
 * 约定: artifacts/yacc/step8/<input_stem>/
 */
std::string make_default_step8_export_dir(const std::string& input_path);

/*
 * export_step8_report — 导出第 8 步 Action/Goto 分析表
 *
 * 导出 Action 表、Goto 表、冲突列表、冲突消解日志等。
 */
void export_step8_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const LR1Step8Result& lr1_step8_result,
    const LR1Step8ValidationReport& lr1_step8_validation, const std::string& output_dir);

/*
 * make_default_step9_export_dir — 第 9 步导出目录路径
 * 约定: artifacts/yacc/step9/<input_stem>/
 */
std::string make_default_step9_export_dir(const std::string& input_path);

/*
 * export_step9_report — 导出第 9 步 LR 解析执行结果
 *
 * 导出移进/归约步骤 trace、接受/拒绝结果、错误信息、LR/LALR 对比等。
 */
void export_step9_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const LR1Step8Result& lr1_step8_result,
    const LR1Step8ValidationReport& lr1_step8_validation, const LRParseRunResult& lr1_parse_result,
    const LRParseRunResult& lalr_parse_result, const std::vector<RuntimeToken>& input_tokens,
    const std::string& token_source,
    const std::string& output_dir);

/*
 * make_default_step10_export_dir — 第 10 步导出目录路径
 * 约定: artifacts/yacc/step10/<input_stem>/
 */
std::string make_default_step10_export_dir(const std::string& input_path);

/*
 * export_step10_report — 导出第 10 步 LALR(1) 合并结果
 *
 * 导出合并映射、LR(1)/LALR(1) 状态规模对比、冲突变化统计等。
 */
void export_step10_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const LR1Step8Result& lr1_step8_result,
    const LR1Step8ValidationReport& lr1_step8_validation, const LR1Step10Result& step10_result,
    const LR1Step10ValidationReport& step10_validation, const std::string& output_dir);

}  // namespace seu::yacc
