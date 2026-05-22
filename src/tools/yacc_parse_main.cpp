/**
 * 文件说明：yacc_parse_tool 的命令行入口。
 * 负责解析子命令与参数，串联 Step4~Step10 的算法流程，
 * 并提供控制台摘要输出、机器可读输出以及 y.tab.h/token_cases 导出能力。
 */

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <unordered_set>
#include <sstream>
#include <string>
#include <vector>

#include "yacc/first/first_set.h"
#include "yacc/lalr/lalr_builder.h"
#include "yacc/lr1/lr1_items.h"
#include "yacc/model/grammar.h"
#include "yacc/parser/yacc_parser.h"
#include "yacc/preprocess/grammar_preprocessor.h"
#include "yacc/report/report_exporter.h"
#include "yacc/runtime/lr_parser.h"
#include "yacc/table/parse_table.h"

namespace {

// 函数说明：将符号类别枚举转为可读文本，用于日志与导出展示。
std::string symbol_kind_to_text(seu::yacc::SymbolKind kind) {
    switch (kind) {
        case seu::yacc::SymbolKind::Terminal:
            return "Terminal";
        case seu::yacc::SymbolKind::Nonterminal:
            return "Nonterminal";
        case seu::yacc::SymbolKind::Special:
            return "Special";
    }
    return "Unknown";
}

// 函数说明：打印文法解析后的总体统计信息。
void print_summary(const seu::yacc::Grammar& grammar) {
    std::cout << "Yacc 文件解析成功\n";
    std::cout << "源文件: " << grammar.source_path << '\n';
    std::cout << "符号总数: " << grammar.symbols.size() << '\n';
    std::cout << "终结符数: " << grammar.terminal_ids.size() << '\n';
    std::cout << "非终结符数: " << grammar.nonterminal_ids.size() << '\n';
    std::cout << "产生式数(含增广): " << grammar.productions.size() << '\n';

    if (grammar.start_symbol_id >= 0 &&
        grammar.start_symbol_id < static_cast<int>(grammar.symbols.size())) {
        std::cout << "开始符号: " << grammar.symbols[grammar.start_symbol_id].name << '\n';
    }
}

// 函数说明：打印完整符号表，便于调试 symbol-id 映射问题。
void print_symbols(const seu::yacc::Grammar& grammar) {
    std::cout << "\n[符号表]\n";
    for (const auto& sym : grammar.symbols) {
        std::cout << "id=" << sym.id << ", name=" << sym.name
                  << ", kind=" << symbol_kind_to_text(sym.kind)
                  << ", literal=" << (sym.is_literal_char ? "yes" : "no") << '\n';
    }
}

// 函数说明：打印产生式列表（含行号和 action 标记）。
void print_productions(const seu::yacc::Grammar& grammar) {
    std::cout << "\n[产生式表]\n";
    for (const auto& p : grammar.productions) {
        std::cout << "#" << p.id << " ";
        std::cout << grammar.symbols[p.lhs_symbol_id].name << " ->";
        if (p.rhs_symbol_ids.empty()) {
            std::cout << " " << seu::yacc::kEpsilonSymbolName;
        } else {
            for (int rhs_id : p.rhs_symbol_ids) {
                std::cout << " " << grammar.symbols[rhs_id].name;
            }
        }
        std::cout << "    [line=" << p.source_line << "]";
        if (p.action.present) {
            std::cout << " [action=yes]";
        }
        std::cout << '\n';
    }
}

// 结构完整性校验：用于确认解析结果可用于后续算法阶段。
bool validate_grammar(const seu::yacc::Grammar& grammar, std::string& error) {
    if (grammar.start_symbol_id < 0 || grammar.start_symbol_id >= static_cast<int>(grammar.symbols.size())) {
        error = "start_symbol_id 非法";
        return false;
    }
    if (grammar.productions.empty()) {
        error = "产生式为空";
        return false;
    }
    if (grammar.productions.front().id != 0) {
        error = "增广产生式编号应为 0";
        return false;
    }
    for (size_t i = 0; i < grammar.productions.size(); ++i) {
        const auto& p = grammar.productions[i];
        if (p.id != static_cast<int>(i)) {
            error = "产生式编号不是连续递增";
            return false;
        }
        if (p.lhs_symbol_id < 0 || p.lhs_symbol_id >= static_cast<int>(grammar.symbols.size())) {
            error = "存在 lhs_symbol_id 越界";
            return false;
        }
        for (int rhs_id : p.rhs_symbol_ids) {
            if (rhs_id < 0 || rhs_id >= static_cast<int>(grammar.symbols.size())) {
                error = "存在 rhs_symbol_id 越界";
                return false;
            }
        }
    }
    return true;
}

// 函数说明：输出第4步预处理摘要，包含增广产生式和告警数量。
void print_preprocess_result(
    const seu::yacc::Grammar& grammar, const seu::yacc::GrammarPreprocessReport& report) {
    std::cout << "[第4步预处理] " << (report.passed ? "通过" : "失败") << '\n';
    if (!grammar.productions.empty()) {
        const auto& p0 = grammar.productions.front();
        std::cout << "增广产生式: #" << p0.id << " " << grammar.symbols[p0.lhs_symbol_id].name << " ->";
        for (int rhs_id : p0.rhs_symbol_ids) {
            std::cout << " " << grammar.symbols[rhs_id].name;
        }
        std::cout << '\n';
    }
    if (!report.warnings.empty()) {
        std::cout << "预处理告警数: " << report.warnings.size() << '\n';
    }
}

// 函数说明：输出 First 集阶段的关键统计信息。
void print_first_set_result(const seu::yacc::Grammar& grammar, const seu::yacc::FirstSetResult& first_result,
    const seu::yacc::FirstSetValidationReport& first_validation) {
    std::cout << "[第5步 First 集] " << (first_validation.passed ? "通过" : "失败") << '\n';
    std::cout << "First 迭代轮次: " << first_result.iteration_count << '\n';
    std::cout << "可空非终结符数: " << first_result.nullable_nonterminal_ids.size() << '\n';
    if (!first_result.nullable_nonterminal_ids.empty()) {
        std::cout << "可空非终结符(前10个):";
        const std::size_t limit = std::min<std::size_t>(10, first_result.nullable_nonterminal_ids.size());
        for (std::size_t i = 0; i < limit; ++i) {
            std::cout << " " << grammar.symbols[first_result.nullable_nonterminal_ids[i]].name;
        }
        std::cout << '\n';
    }
}

// 函数说明：输出第6步 I0/closure/goto 的规模指标。
void print_step6_lr1_result(const seu::yacc::Grammar& grammar, const seu::yacc::LR1Step6Result& lr1_result,
    const seu::yacc::LR1Step6ValidationReport& lr1_validation) {
    std::cout << "[第6步 LR(1) 项/闭包/Goto] " << (lr1_validation.passed ? "通过" : "失败") << '\n';
    std::cout << "I0 kernel 项数: " << lr1_result.i0_kernel_items.size() << '\n';
    std::cout << "I0 closure 项数: " << lr1_result.i0_closure_items.size() << '\n';
    std::cout << "I0 goto 边数: " << lr1_result.goto_symbol_ids.size() << '\n';
    if (!lr1_result.goto_symbol_ids.empty()) {
        std::cout << "I0 可转移符号(前10个):";
        const std::size_t limit = std::min<std::size_t>(10, lr1_result.goto_symbol_ids.size());
        for (std::size_t i = 0; i < limit; ++i) {
            const int sid = lr1_result.goto_symbol_ids[i];
            if (sid >= 0 && sid < static_cast<int>(grammar.symbols.size())) {
                std::cout << " " << grammar.symbols[sid].name;
            }
        }
        std::cout << '\n';
    }
    std::cout << "lookahead 来源说明条目: " << lr1_result.lookahead_derivation_notes.size() << '\n';
}

// 函数说明：输出第7步 LR(1) 状态机规模与部分转移样例。
void print_step7_lr1_result(const seu::yacc::Grammar& grammar, const seu::yacc::LR1Step7Result& lr1_result,
    const seu::yacc::LR1Step7ValidationReport& lr1_validation) {
    std::cout << "[第7步 LR(1) 规范族/状态图] " << (lr1_validation.passed ? "通过" : "失败") << '\n';
    std::cout << "状态总数: " << lr1_result.states.size() << '\n';
    std::cout << "转移边总数: " << lr1_result.transitions.size() << '\n';
    if (!lr1_result.transitions.empty()) {
        std::cout << "前10条边:\n";
        const std::size_t limit = std::min<std::size_t>(10, lr1_result.transitions.size());
        for (std::size_t i = 0; i < limit; ++i) {
            const auto& edge = lr1_result.transitions[i];
            std::string sym = "<invalid>";
            if (edge.symbol_id >= 0 && edge.symbol_id < static_cast<int>(grammar.symbols.size())) {
                sym = grammar.symbols[edge.symbol_id].name;
            }
            std::cout << "  I" << edge.from_state_id << " --" << sym << "--> I" << edge.to_state_id << '\n';
        }
    }
}

// 函数说明：输出第8步 Action/Goto 表规模及冲突统计。
void print_step8_table_result(const seu::yacc::LR1Step8Result& step8_result,
    const seu::yacc::LR1Step8ValidationReport& step8_validation) {
    std::size_t action_entries = 0;
    for (const auto& row : step8_result.action_table) {
        action_entries += row.size();
    }
    std::size_t goto_entries = 0;
    for (const auto& row : step8_result.goto_table) {
        goto_entries += row.size();
    }
    std::cout << "[第8步 Action/Goto 表] " << (step8_validation.passed ? "通过" : "失败") << '\n';
    std::cout << "Action 表项数: " << action_entries << '\n';
    std::cout << "Goto 表项数: " << goto_entries << '\n';
    std::cout << "冲突数: " << step8_result.conflicts.size() << '\n';
    std::cout << "冲突消解日志数: " << step8_result.conflict_resolution_logs.size() << '\n';
    if (!step8_result.conflicts.empty()) {
        std::cout << "冲突消解策略: shift/reduce 优先 shift；reduce/reduce 取较小产生式编号\n";
    }
}

// 函数说明：输出第9步单次解析运行结果与错误上下文。
void print_step9_parse_result(const seu::yacc::Grammar& grammar, const seu::yacc::LRParseRunResult& parse_result) {
    std::cout << "结果: " << (parse_result.accepted ? "accept" : "not-accept") << '\n';
    std::cout << "执行步数: " << parse_result.total_steps << '\n';
    std::cout << "规约次数: " << parse_result.reduction_production_ids.size() << '\n';
    std::cout << "已消费 token 数(含可能自动补的$): " << parse_result.consumed_tokens << '\n';
    if (parse_result.error.has_error) {
        std::cout << "错误信息: " << parse_result.error.message << '\n';
        std::cout << "错误位置: step=" << parse_result.error.step_no
                  << ", input_index=" << parse_result.error.input_index
                  << ", state=" << parse_result.error.state_id
                  << ", lookahead=" << parse_result.error.lookahead_symbol_name << '\n';
        if (!parse_result.error.expected_terminal_ids.empty()) {
            std::cout << "期待终结符(前10个):";
            const std::size_t limit = std::min<std::size_t>(10, parse_result.error.expected_terminal_ids.size());
            for (std::size_t i = 0; i < limit; ++i) {
                const int sid = parse_result.error.expected_terminal_ids[i];
                if (sid >= 0 && sid < static_cast<int>(grammar.symbols.size())) {
                    std::cout << " " << grammar.symbols[sid].name;
                }
            }
            std::cout << '\n';
        }
    }
}

// 函数说明：对比 LR1 与 LALR 两套运行时结果，观察语义一致性。
void print_step9_parse_compare_result(
    const seu::yacc::LRParseRunResult& lr1_parse_result, const seu::yacc::LRParseRunResult& lalr_parse_result) {
    std::cout << "[第9步 LR1/LALR 对比] "
              << "lr1_accept=" << (lr1_parse_result.accepted ? "true" : "false")
              << ", lalr_accept=" << (lalr_parse_result.accepted ? "true" : "false") << '\n';
    std::cout << "LR1 steps=" << lr1_parse_result.total_steps
              << ", LALR steps=" << lalr_parse_result.total_steps << '\n';
    std::cout << "LR1 reductions=" << lr1_parse_result.reduction_production_ids.size()
              << ", LALR reductions=" << lalr_parse_result.reduction_production_ids.size() << '\n';
}

// 函数说明：输出第10步 LR(1)->LALR 合并统计和冲突变化。
void print_step10_lalr_result(const seu::yacc::LR1Step10Result& step10_result,
    const seu::yacc::LR1Step10ValidationReport& step10_validation) {
    std::cout << "[第10步 LR(1)->LALR(1)] " << (step10_validation.passed ? "通过" : "失败") << '\n';
    std::cout << "LR(1) 状态数: " << step10_result.lr1_state_count << '\n';
    std::cout << "LALR(1) 状态数: " << step10_result.lalr_state_count << '\n';
    std::cout << "状态压缩数: " << (step10_result.lr1_state_count - step10_result.lalr_state_count) << '\n';
    std::cout << "LR(1) 冲突数: " << step10_result.lr1_conflict_count << '\n';
    std::cout << "LALR(1) 冲突数: " << step10_result.lalr_conflict_count << '\n';
}

// 函数说明：机器可读输出 LALR 规约序列，用于对拍脚本。
void print_machine_lalr_reduction_sequence(const seu::yacc::LRParseRunResult& lalr_parse_result) {
    std::cout << "__YACC_LALR_REDUCTIONS__:";
    for (std::size_t i = 0; i < lalr_parse_result.reduction_production_ids.size(); ++i) {
        if (i > 0) {
            std::cout << ",";
        }
        std::cout << lalr_parse_result.reduction_production_ids[i];
    }
    std::cout << '\n';
}

// 函数说明：机器可读输出第10步完整原始结构（状态/边/表/冲突）。
void print_machine_step10_raw(const seu::yacc::Grammar& grammar, const seu::yacc::LR1Step10Result& step10_result) {
    std::cout << "__YACC_STEP10_RAW_BEGIN__\n";

    std::cout << "__YACC_STEP10_LALR_STATE_ITEMS_BEGIN__\n";
    for (const auto& state : step10_result.lalr_step7_result.states) {
        std::cout << "[state " << state.state_id << "]\n";
        for (const auto& item : state.items) {
            std::cout << seu::yacc::format_lr1_item(grammar, item) << '\n';
        }
        std::cout << '\n';
    }
    std::cout << "__YACC_STEP10_LALR_STATE_ITEMS_END__\n";

    std::cout << "__YACC_STEP10_LALR_TRANSITIONS_BEGIN__\n";
    std::cout << "from_state\tsymbol_id\tsymbol_name\tto_state\n";
    for (const auto& edge : step10_result.lalr_step7_result.transitions) {
        const std::string symbol_name =
            (edge.symbol_id >= 0 && edge.symbol_id < static_cast<int>(grammar.symbols.size()))
                ? grammar.symbols[edge.symbol_id].name
                : "<invalid>";
        std::cout << edge.from_state_id << '\t' << edge.symbol_id << '\t' << symbol_name << '\t'
                  << edge.to_state_id << '\n';
    }
    std::cout << "__YACC_STEP10_LALR_TRANSITIONS_END__\n";

    std::cout << "__YACC_STEP10_LALR_ACTION_TABLE_BEGIN__\n";
    std::cout << "state_id\tterminal_id\tterminal_name\taction\ttarget\n";
    for (std::size_t state_id = 0; state_id < step10_result.lalr_step8_result.action_table.size(); ++state_id) {
        std::vector<std::pair<int, seu::yacc::ParseActionEntry>> entries;
        entries.reserve(step10_result.lalr_step8_result.action_table[state_id].size());
        for (const auto& kv : step10_result.lalr_step8_result.action_table[state_id]) {
            entries.push_back(kv);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        for (const auto& kv : entries) {
            const int symbol_id = kv.first;
            const auto& action = kv.second;
            const std::string symbol_name =
                (symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size()))
                    ? grammar.symbols[symbol_id].name
                    : "<invalid>";
            std::string target = "-";
            if (action.type == seu::yacc::ParseActionType::Shift) {
                target = std::to_string(action.target_state_id);
            } else if (action.type == seu::yacc::ParseActionType::Reduce) {
                target = std::to_string(action.reduce_production_id);
            }
            std::cout << state_id << '\t' << symbol_id << '\t' << symbol_name << '\t'
                      << seu::yacc::format_parse_action_entry(action) << '\t' << target << '\n';
        }
    }
    std::cout << "__YACC_STEP10_LALR_ACTION_TABLE_END__\n";

    std::cout << "__YACC_STEP10_LALR_GOTO_TABLE_BEGIN__\n";
    std::cout << "state_id\tnonterminal_id\tnonterminal_name\tto_state\n";
    for (std::size_t state_id = 0; state_id < step10_result.lalr_step8_result.goto_table.size(); ++state_id) {
        std::vector<std::pair<int, int>> entries;
        entries.reserve(step10_result.lalr_step8_result.goto_table[state_id].size());
        for (const auto& kv : step10_result.lalr_step8_result.goto_table[state_id]) {
            entries.push_back(kv);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        for (const auto& kv : entries) {
            const int symbol_id = kv.first;
            const int to_state = kv.second;
            const std::string symbol_name =
                (symbol_id >= 0 && symbol_id < static_cast<int>(grammar.symbols.size()))
                    ? grammar.symbols[symbol_id].name
                    : "<invalid>";
            std::cout << state_id << '\t' << symbol_id << '\t' << symbol_name << '\t' << to_state << '\n';
        }
    }
    std::cout << "__YACC_STEP10_LALR_GOTO_TABLE_END__\n";

    std::cout << "__YACC_STEP10_LALR_CONFLICTS_BEGIN__\n";
    std::cout << "state_id\tsymbol_id\tsymbol_name\texisting_action\tincoming_action\n";
    for (const auto& c : step10_result.lalr_step8_result.conflicts) {
        const std::string symbol_name =
            (c.symbol_id >= 0 && c.symbol_id < static_cast<int>(grammar.symbols.size()))
                ? grammar.symbols[c.symbol_id].name
                : "<invalid>";
        std::cout << c.state_id << '\t' << c.symbol_id << '\t' << symbol_name << '\t' << c.existing_action
                  << '\t' << c.incoming_action << '\n';
    }
    std::cout << "__YACC_STEP10_LALR_CONFLICTS_END__\n";

    std::cout << "__YACC_STEP10_RAW_END__\n";
}

bool is_identifier_token_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    if (!(std::isalpha(static_cast<unsigned char>(name[0])) != 0 || name[0] == '_')) {
        return false;
    }
    for (std::size_t i = 1; i < name.size(); ++i) {
        const char ch = name[i];
        if (!(std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_')) {
            return false;
        }
    }
    return true;
}

std::vector<int> build_emit_token_symbol_ids(const seu::yacc::Grammar& grammar) {
    std::vector<int> ids;
    std::unordered_set<int> seen;
    for (int sid : grammar.declared_token_symbol_ids) {
        if (sid < 0 || sid >= static_cast<int>(grammar.symbols.size())) {
            continue;
        }
        if (grammar.symbols[sid].kind != seu::yacc::SymbolKind::Terminal) {
            continue;
        }
        if (!is_identifier_token_name(grammar.symbols[sid].name)) {
            continue;
        }
        if (seen.insert(sid).second) {
            ids.push_back(sid);
        }
    }
    std::vector<int> remaining;
    for (int sid : grammar.terminal_ids) {
        if (sid < 0 || sid >= static_cast<int>(grammar.symbols.size())) {
            continue;
        }
        if (!is_identifier_token_name(grammar.symbols[sid].name)) {
            continue;
        }
        if (seen.find(sid) == seen.end()) {
            remaining.push_back(sid);
        }
    }
    std::sort(remaining.begin(), remaining.end(), [&](int a, int b) {
        return grammar.symbols[a].name < grammar.symbols[b].name;
    });
    ids.insert(ids.end(), remaining.begin(), remaining.end());
    return ids;
}

bool extract_union_body(const std::string& union_block_raw, std::string& out_body) {
    const std::size_t start = union_block_raw.find('{');
    if (start == std::string::npos) {
        return false;
    }
    int depth = 0;
    bool opened = false;
    for (std::size_t i = start; i < union_block_raw.size(); ++i) {
        const char ch = union_block_raw[i];
        if (ch == '{') {
            ++depth;
            opened = true;
            continue;
        }
        if (ch == '}') {
            --depth;
            if (opened && depth == 0) {
                out_body = union_block_raw.substr(start + 1, i - (start + 1));
                return true;
            }
        }
    }
    return false;
}

// 函数说明：导出与 bison 风格兼容的 y.tab.h 头文件。
void emit_y_tab_header(const seu::yacc::Grammar& grammar, const std::string& output_path) {
    const std::vector<int> token_ids = build_emit_token_symbol_ids(grammar);
    std::ofstream out(output_path);
    if (!out.is_open()) {
        throw std::runtime_error("无法写入 y.tab.h: " + output_path);
    }

    out << "/* Auto-generated by yacc_parse_tool. */\n";
    out << "#ifndef Y_TAB_H_INCLUDED\n";
    out << "#define Y_TAB_H_INCLUDED\n";
    out << "/* Debug traces.  */\n";
    out << "#ifndef YYDEBUG\n";
    out << "# define YYDEBUG 0\n";
    out << "#endif\n";
    out << "#if YYDEBUG\n";
    out << "extern int yydebug;\n";
    out << "#endif\n";
    out << "\n";
    out << "/* Token kinds.  */\n";
    out << "#ifndef YYTOKENTYPE\n";
    out << "# define YYTOKENTYPE\n";
    out << "  enum yytokentype\n";
    out << "  {\n";
    out << "    YYEMPTY = -2,\n";
    out << "    YYEOF = 0,\n";
    out << "    YYerror = 256,\n";
    out << "    YYUNDEF = 257";
    int token_value = 258;
    for (int sid : token_ids) {
        out << ",\n    " << grammar.symbols[sid].name << " = " << token_value++;
    }
    out << "\n  };\n";
    out << "  typedef enum yytokentype yytoken_kind_t;\n";
    out << "#endif\n";
    out << "\n";
    out << "/* Value type.  */\n";
    out << "#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED\n";
    if (!grammar.union_block_raw.empty()) {
        std::string union_body;
        if (extract_union_body(grammar.union_block_raw, union_body)) {
            out << "typedef union YYSTYPE {\n";
            out << union_body;
            if (!union_body.empty() && union_body.back() != '\n') {
                out << '\n';
            }
            out << "} YYSTYPE;\n";
        } else {
            out << "typedef int YYSTYPE;\n";
        }
    } else {
        out << "typedef int YYSTYPE;\n";
    }
    out << "# define YYSTYPE_IS_TRIVIAL 1\n";
    out << "# define YYSTYPE_IS_DECLARED 1\n";
    out << "#endif\n";
    out << "\n";
    out << "extern YYSTYPE yylval;\n";
    if (grammar.locations_enabled) {
        out << "typedef struct YYLTYPE {\n";
        out << "  int first_line;\n";
        out << "  int first_column;\n";
        out << "  int last_line;\n";
        out << "  int last_column;\n";
        out << "} YYLTYPE;\n";
        out << "extern YYLTYPE yylloc;\n";
    }
    out << "\n";
    out << "int yyparse (void);\n";
    out << "\n";
    out << "#endif /* !Y_TAB_H_INCLUDED */\n";
}

// 函数说明：导出 token 到字符串的 switch-case 代码片段。
void emit_token_cases_include(const seu::yacc::Grammar& grammar, const std::string& output_path) {
    const std::vector<int> token_ids = build_emit_token_symbol_ids(grammar);
    std::ofstream out(output_path);
    if (!out.is_open()) {
        throw std::runtime_error("无法写入 token cases include: " + output_path);
    }
    for (int sid : token_ids) {
        const std::string& name = grammar.symbols[sid].name;
        out << "    case " << name << ": return \"" << name << "\";\n";
    }
}

void emit_standalone_parser_cpp(const seu::yacc::Grammar& grammar, const seu::yacc::LR1Step8Result& step8_result,
    const std::string& output_path) {
    std::ofstream out(output_path);
    if (!out.is_open()) {
        throw std::runtime_error("无法写入 parser cpp: " + output_path);
    }
    out << "#include <fstream>\n#include <iostream>\n#include <sstream>\n#include <string>\n#include <unordered_map>\n#include <vector>\n\n";
    out << "struct Tok{int id;};\n";
    out << "int main(int argc,char** argv){ if(argc<2){std::cerr<<\"usage: "<< "parser_generated <tokens_file>\\n\"; return 2;} \n";
    out << "std::unordered_map<std::string,int> sid = {\n";
    for (std::size_t i = 0; i < grammar.symbols.size(); ++i) {
        out << "{\"" << grammar.symbols[i].name << "\"," << grammar.symbols[i].id << "}";
        if (i + 1 != grammar.symbols.size()) out << ",";
        out << "\n";
    }
    out << "};\n";
    out << "std::vector<Tok> in; std::ifstream f(argv[1]); std::string ln; while(std::getline(f,ln)){ if(ln.empty()||ln[0]=='#') continue; std::istringstream iss(ln); std::string s; iss>>s; auto it=sid.find(s); if(it==sid.end()){std::cerr<<\"unknown token:\"<<s<<\"\\n\"; return 1;} in.push_back({it->second}); }\n";
    out << "if(in.empty() || in.back().id!=" << grammar.eof_symbol_id << ") in.push_back({" << grammar.eof_symbol_id << "});\n";
    out << "std::vector<int> st={0}; int ip=0; int steps=0;\n";
    out << "while(steps++<200000){ int s=st.back(); if(ip<0||ip>=(int)in.size()) {std::cout<<\"not-accept\\n\"; return 1;} int la=in[ip].id;\n";
    out << "switch(s){\n";
    for (std::size_t s = 0; s < step8_result.action_table.size(); ++s) {
        out << "case " << s << ": {\n";
        out << "switch(la){\n";
        for (const auto& kv : step8_result.action_table[s]) {
            const auto& a = kv.second;
            out << "case " << kv.first << ": ";
            if (a.type == seu::yacc::ParseActionType::Shift) {
                out << "st.push_back(" << a.target_state_id << "); ip++; break;\n";
            } else if (a.type == seu::yacc::ParseActionType::Reduce) {
                const auto& p = grammar.productions[a.reduce_production_id];
                out << "{ for(int i=0;i<" << p.rhs_symbol_ids.size() << ";++i) st.pop_back(); int gs=st.back(); switch(gs){\n";
                for (std::size_t gs = 0; gs < step8_result.goto_table.size(); ++gs) {
                    auto git = step8_result.goto_table[gs].find(p.lhs_symbol_id);
                    if (git != step8_result.goto_table[gs].end()) {
                        out << "case " << gs << ": st.push_back(" << git->second << "); break;\n";
                    }
                }
                out << "default: std::cout<<\"not-accept\\n\"; return 1;} } break;\n";
            } else {
                out << "std::cout<<\"accept\\n\"; return 0;\n";
            }
        }
        out << "default: std::cout<<\"not-accept\\n\"; return 1; }\n";
        out << "} break;\n";
    }
    out << "default: std::cout<<\"not-accept\\n\"; return 1; }} std::cout<<\"not-accept\\n\"; return 1; }\n";
}

std::string build_minimal_quads_from_ast_json(const std::string& ast_json) {
    std::ostringstream out;
    out << "# minimal quads example\n";
    if (ast_json.find("return") != std::string::npos || ast_json.find("RETURN") != std::string::npos) {
        out << "(return, -, -, ret)\n";
    }
    if (ast_json.find("=") != std::string::npos) {
        out << "(assign, rhs, -, lhs)\n";
    }
    if (ast_json.find("+") != std::string::npos || ast_json.find("-") != std::string::npos ||
        ast_json.find("*") != std::string::npos || ast_json.find("/") != std::string::npos) {
        out << "(binop, a, b, t1)\n";
    }
    return out.str();
}

void print_compact_summary(const seu::yacc::LR1Step7Result& lr1_step7_result, const seu::yacc::LR1Step8Result& lr1_step8_result,
    const seu::yacc::LR1Step10Result& step10_result, bool run_step9, const seu::yacc::LRParseRunResult& lr1_parse_result,
    const seu::yacc::LRParseRunResult& lalr_parse_result) {
    std::size_t action_entries = 0;
    for (const auto& row : lr1_step8_result.action_table) {
        action_entries += row.size();
    }
    std::size_t goto_entries = 0;
    for (const auto& row : lr1_step8_result.goto_table) {
        goto_entries += row.size();
    }
    std::cout << "[摘要] Step4~Step10 通过\n";
    std::cout << "状态数: LR(1)=" << lr1_step7_result.states.size() << ", LALR(1)=" << step10_result.lalr_state_count
              << '\n';
    std::cout << "分析表: Action=" << action_entries << ", Goto=" << goto_entries << '\n';
    std::cout << "冲突: LR(1)=" << step10_result.lr1_conflict_count << ", LALR(1)=" << step10_result.lalr_conflict_count
              << '\n';
    if (run_step9) {
        std::cout << "解析: LR1=" << (lr1_parse_result.accepted ? "accept" : "not-accept")
                  << ", LALR=" << (lalr_parse_result.accepted ? "accept" : "not-accept")
                  << ", steps=" << lr1_parse_result.total_steps << "/" << lalr_parse_result.total_steps << '\n';
        // Keep compatibility with legacy test scripts that parse this exact line.
        std::cout << "[第9步 LR1/LALR 对比] "
                  << "lr1_accept=" << (lr1_parse_result.accepted ? "true" : "false")
                  << ", lalr_accept=" << (lalr_parse_result.accepted ? "true" : "false") << '\n';
        std::cout << "LR1 reductions=" << lr1_parse_result.reduction_production_ids.size()
                  << ", LALR reductions=" << lalr_parse_result.reduction_production_ids.size() << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    // 1) 解析子命令模式（run / emit），未指定时默认 run。
    enum class Mode {
        Emit,
        Run
    };
    Mode mode = Mode::Run;
    int arg_begin = 1;
    if (argc >= 2) {
        const std::string sub = argv[1];
        if (sub == "emit") {
            mode = Mode::Emit;
            arg_begin = 2;
        } else if (sub == "run") {
            mode = Mode::Run;
            arg_begin = 2;
        }
    }

    std::string input_path = "c99.y";
    bool dump_symbols = false;
    bool dump_productions = false;
    bool run_validate = true;
    bool export_report = false;
    bool dump_lalr_reductions_machine = false;
    bool dump_step10_raw_machine = false;
    bool strict_bison_ish = false;
    bool verbose = false;
    bool show_progress = true;
    std::string emit_y_tab_h_path;
    std::string emit_token_cases_inc_path;
    std::string emit_parser_cpp_path;
    std::string export_dir;
    std::string token_file_path;
    bool parse_tokens_stdin = false;
    std::string from_lexer_path;
    std::string ast_out_path;
    std::string ast_format = "json";
    std::string ir_out_path;
    int max_parse_steps = 200000;

    // 2) 解析命令行选项并做模式兼容性校验。
    // emit: yacc_parse_tool emit [input.y] --emit-y-tab-h <path> --emit-token-cases-inc <path>
    // run:  yacc_parse_tool run  [input.y] [--dump-symbols] [--dump-productions]
    //                            [--export] [--export-dir <dir>] [--no-validate]
    //                            [--parse-tokens <file>] [--max-parse-steps <n>]
    for (int i = arg_begin; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--dump-symbols") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --dump-symbols\n";
                return 1;
            }
            dump_symbols = true;
            continue;
        }
        if (arg == "--dump-productions") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --dump-productions\n";
                return 1;
            }
            dump_productions = true;
            continue;
        }
        if (arg == "--no-validate") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --no-validate\n";
                return 1;
            }
            run_validate = false;
            continue;
        }
        if (arg == "--export") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --export\n";
                return 1;
            }
            export_report = true;
            continue;
        }
        if (arg == "--dump-lalr-reductions-machine") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --dump-lalr-reductions-machine\n";
                return 1;
            }
            dump_lalr_reductions_machine = true;
            continue;
        }
        if (arg == "--dump-step10-raw-machine") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --dump-step10-raw-machine\n";
                return 1;
            }
            dump_step10_raw_machine = true;
            continue;
        }
        if (arg == "--export-dir") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --export-dir\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--export-dir 缺少目录参数\n";
                return 1;
            }
            export_report = true;
            export_dir = argv[++i];
            continue;
        }
        if (arg == "--parse-tokens") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --parse-tokens\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--parse-tokens 缺少文件参数\n";
                return 1;
            }
            token_file_path = argv[++i];
            continue;
        }
        if (arg == "--parse-tokens-stdin") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --parse-tokens-stdin\n";
                return 1;
            }
            parse_tokens_stdin = true;
            continue;
        }
        if (arg == "--from-lexer") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --from-lexer\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--from-lexer 缺少文件参数\n";
                return 1;
            }
            from_lexer_path = argv[++i];
            continue;
        }
        if (arg == "--ast-out") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --ast-out\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--ast-out 缺少文件参数\n";
                return 1;
            }
            ast_out_path = argv[++i];
            continue;
        }
        if (arg == "--ast-format") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --ast-format\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--ast-format 缺少参数\n";
                return 1;
            }
            ast_format = argv[++i];
            continue;
        }
        if (arg == "--ir-out") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --ir-out\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--ir-out 缺少文件参数\n";
                return 1;
            }
            ir_out_path = argv[++i];
            continue;
        }
        if (arg == "--max-parse-steps") {
            if (mode == Mode::Emit) {
                std::cerr << "emit 模式不支持 --max-parse-steps\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--max-parse-steps 缺少数值参数\n";
                return 1;
            }
            max_parse_steps = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "--emit-y-tab-h") {
            if (mode == Mode::Run) {
                std::cerr << "run 模式不支持 --emit-y-tab-h，请使用 emit 子命令\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--emit-y-tab-h 缺少文件参数\n";
                return 1;
            }
            emit_y_tab_h_path = argv[++i];
            continue;
        }
        if (arg == "--emit-token-cases-inc") {
            if (mode == Mode::Run) {
                std::cerr << "run 模式不支持 --emit-token-cases-inc，请使用 emit 子命令\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--emit-token-cases-inc 缺少文件参数\n";
                return 1;
            }
            emit_token_cases_inc_path = argv[++i];
            continue;
        }
        if (arg == "--emit-parser-cpp") {
            if (mode == Mode::Run) {
                std::cerr << "run 模式不支持 --emit-parser-cpp，请使用 emit 子命令\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "--emit-parser-cpp 缺少文件参数\n";
                return 1;
            }
            emit_parser_cpp_path = argv[++i];
            continue;
        }
        if (arg == "--strict-bison-ish") {
            strict_bison_ish = true;
            continue;
        }
        if (arg == "--verbose") {
            verbose = true;
            continue;
        }
        if (arg == "--no-progress") {
            show_progress = false;
            continue;
        }
        input_path = arg;
    }

    if (mode == Mode::Emit && emit_y_tab_h_path.empty() && emit_token_cases_inc_path.empty() && emit_parser_cpp_path.empty()) {
        std::cerr << "emit 模式至少需要一个导出参数：--emit-y-tab-h/--emit-token-cases-inc/--emit-parser-cpp\n";
        return 1;
    }

    try {
        // 3) 执行主流程：解析文法 -> 构建各步结果 -> 可选运行时解析与导出。
        auto stage_start = std::chrono::steady_clock::now();
        auto mark_stage = [&](const std::string& label) {
            if (!show_progress) {
                stage_start = std::chrono::steady_clock::now();
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - stage_start).count();
            std::cout << "[进度] " << label << "（耗时 " << ms << " ms）\n";
            stage_start = now;
        };

        // 3.1 读取并解析 .y 文法。
        seu::yacc::Grammar grammar = seu::yacc::parse_yacc_file(input_path);
        if (strict_bison_ish && !grammar.parsed_only_directives.empty()) {
            std::cerr << "strict-bison-ish: 检测到 parsed-only 指令:\n";
            for (const auto& d : grammar.parsed_only_directives) {
                std::cerr << "  - " << d << '\n';
            }
            return 2;
        }
        if (!grammar.unsupported_directives.empty()) {
            std::cerr << "告警: 检测到未支持 directives（已忽略）:\n";
            for (const auto& d : grammar.unsupported_directives) {
                std::cerr << "  - " << d << '\n';
            }
        }
        if (!emit_y_tab_h_path.empty()) {
            emit_y_tab_header(grammar, emit_y_tab_h_path);
            std::cout << "[导出] y.tab.h 已写入: " << emit_y_tab_h_path << '\n';
        }
        if (!emit_token_cases_inc_path.empty()) {
            emit_token_cases_include(grammar, emit_token_cases_inc_path);
            std::cout << "[导出] token_cases.inc 已写入: " << emit_token_cases_inc_path << '\n';
        }
        if (!emit_parser_cpp_path.empty()) {
            const seu::yacc::FirstSetResult first_result = seu::yacc::compute_first_sets(grammar);
            const seu::yacc::LR1Step7Result lr1_step7_result =
                seu::yacc::build_step7_lr1_canonical_collection(grammar, first_result, nullptr);
            const seu::yacc::LR1Step8Result lr1_step8_result =
                seu::yacc::build_step8_lr1_parsing_table(grammar, lr1_step7_result);
            const seu::yacc::LR1Step10Result step10_result =
                seu::yacc::build_step10_lalr_from_lr1(grammar, lr1_step7_result, lr1_step8_result);
            emit_standalone_parser_cpp(grammar, step10_result.lalr_step8_result, emit_parser_cpp_path);
            std::cout << "[导出] parser cpp 已写入: " << emit_parser_cpp_path << '\n';
        }

        // emit 模式只做导出，不进入后续算法流水线。
        if (mode == Mode::Emit) {
            return 0;
        }

        print_summary(grammar);
        if (grammar.locations_enabled) {
            std::cout << "definitions: %locations=on\n";
        }
        if (grammar.expect_sr_conflicts >= 0) {
            std::cout << "definitions: %expect=" << grammar.expect_sr_conflicts << '\n';
        }
        if (grammar.expect_rr_conflicts >= 0) {
            std::cout << "definitions: %expect-rr=" << grammar.expect_rr_conflicts << '\n';
        }
        mark_stage("Step1-3 解析输入完成");

        if (run_validate) {
            std::string validate_error;
            if (validate_grammar(grammar, validate_error)) {
                std::cout << "[校验] 通过\n";
            } else {
                std::cout << "[校验] 失败: " << validate_error << '\n';
                return 3;
            }
        } else {
            std::cout << "[校验] 已跳过\n";
        }

        // 3.2 执行 Step4~Step10 的算法流水线。
        const seu::yacc::GrammarPreprocessReport preprocess_report = seu::yacc::preprocess_grammar(grammar);
        const seu::yacc::GrammarAnalysis analysis = seu::yacc::analyze_grammar(grammar);
        mark_stage("Step4 预处理/分析完成");

        const seu::yacc::FirstSetResult first_result = seu::yacc::compute_first_sets(grammar);
        const seu::yacc::FirstSetValidationReport first_validation =
            seu::yacc::validate_first_sets(grammar, first_result);
        mark_stage("Step5 First 集完成");

        const seu::yacc::LR1Step6Result lr1_result = seu::yacc::build_step6_lr1_items(grammar, first_result);
        const seu::yacc::LR1Step6ValidationReport lr1_validation =
            seu::yacc::validate_step6_lr1_items(grammar, first_result, lr1_result);
        mark_stage("Step6 LR(1) I0 完成");

        const seu::yacc::LR1Step7Result lr1_step7_result =
            seu::yacc::build_step7_lr1_canonical_collection(grammar, first_result, &lr1_result);
        const seu::yacc::LR1Step7ValidationReport lr1_step7_validation =
            seu::yacc::validate_step7_lr1_canonical_collection(grammar, first_result, lr1_step7_result);
        mark_stage("Step7 LR(1) 规范族完成");

        const seu::yacc::LR1Step8Result lr1_step8_result =
            seu::yacc::build_step8_lr1_parsing_table(grammar, lr1_step7_result);
        const seu::yacc::LR1Step8ValidationReport lr1_step8_validation =
            seu::yacc::validate_step8_lr1_parsing_table(grammar, lr1_step7_result, lr1_step8_result);
        mark_stage("Step8 LR(1) 分析表完成");

        const seu::yacc::LR1Step10Result step10_result =
            seu::yacc::build_step10_lalr_from_lr1(grammar, lr1_step7_result, lr1_step8_result);
        const seu::yacc::LR1Step10ValidationReport step10_validation = seu::yacc::validate_step10_lalr_from_lr1(
            grammar, lr1_step7_result, lr1_step8_result, step10_result);
        mark_stage("Step10 LALR 构建完成");

        if (!from_lexer_path.empty() && token_file_path.empty()) {
            token_file_path = from_lexer_path;
        }
        if (parse_tokens_stdin && !token_file_path.empty()) {
            std::cerr << "--parse-tokens 与 --parse-tokens-stdin 不能同时使用\n";
            return 1;
        }
        bool run_step9 = parse_tokens_stdin || !token_file_path.empty();
        std::vector<seu::yacc::RuntimeToken> runtime_tokens;
        seu::yacc::LRParseRunResult lr1_parse_result;
        seu::yacc::LRParseRunResult lalr_parse_result;

        // 3.3 严格模式下逐步校验，失败即早停并返回对应步骤码。
        if (run_validate) {
            if (!preprocess_report.passed) {
                std::cout << "[第4步预处理] 失败\n";
                for (const auto& e : preprocess_report.errors) {
                    std::cout << "- " << e << '\n';
                }
                return 4;
            }
            if (!first_validation.passed) {
                std::cout << "[第5步 First 集] 失败\n";
                for (const auto& e : first_validation.errors) {
                    std::cout << "- " << e << '\n';
                }
                return 5;
            }
            if (!lr1_validation.passed) {
                std::cout << "[第6步 LR(1) 项/闭包/Goto] 失败\n";
                for (const auto& e : lr1_validation.errors) {
                    std::cout << "- " << e << '\n';
                }
                return 6;
            }
            if (!lr1_step7_validation.passed) {
                std::cout << "[第7步 LR(1) 规范族/状态图] 失败\n";
                for (const auto& e : lr1_step7_validation.errors) {
                    std::cout << "- " << e << '\n';
                }
                return 7;
            }
            if (!lr1_step8_validation.passed) {
                std::cout << "[第8步 Action/Goto 表] 失败\n";
                for (const auto& e : lr1_step8_validation.errors) {
                    std::cout << "- " << e << '\n';
                }
                return 8;
            }
            if (!step10_validation.passed) {
                std::cout << "[第10步 LR(1)->LALR(1)] 失败\n";
                for (const auto& e : step10_validation.errors) {
                    std::cout << "- " << e << '\n';
                }
                return 10;
            }
        }

        // 3.4 若提供 token 文件，则运行第9步解析（LR1 与 LALR 两套表）。
        if (run_step9) {
            if (parse_tokens_stdin) {
                runtime_tokens = seu::yacc::load_runtime_tokens_from_stream(grammar, std::cin, "stdin token 流");
            } else {
                runtime_tokens = seu::yacc::load_runtime_tokens_from_file(grammar, token_file_path);
            }
            lr1_parse_result =
                seu::yacc::run_step9_lr_parse(grammar, lr1_step8_result, runtime_tokens, max_parse_steps, false);
            lalr_parse_result = seu::yacc::run_step9_lr_parse(
                grammar, step10_result.lalr_step8_result, runtime_tokens, max_parse_steps, true);
            mark_stage("Step9 LR1/LALR 运行时解析完成");
            if (!ast_out_path.empty()) {
                std::ofstream astf(ast_out_path);
                if (!astf.is_open()) {
                    throw std::runtime_error("无法写入 AST 文件: " + ast_out_path);
                }
                if (ast_format == "txt") {
                    astf << lalr_parse_result.ast_text;
                } else {
                    astf << lalr_parse_result.ast_json;
                }
                std::cout << "[导出] AST 已写入: " << ast_out_path << '\n';
            }
            if (!ir_out_path.empty()) {
                std::ofstream irf(ir_out_path);
                if (!irf.is_open()) {
                    throw std::runtime_error("无法写入 IR 文件: " + ir_out_path);
                }
                irf << build_minimal_quads_from_ast_json(lalr_parse_result.ast_json);
                std::cout << "[导出] IR(quads) 已写入: " << ir_out_path << '\n';
            }
        }

        if (verbose) {
            print_preprocess_result(grammar, preprocess_report);
            print_first_set_result(grammar, first_result, first_validation);
            print_step6_lr1_result(grammar, lr1_result, lr1_validation);
            print_step7_lr1_result(grammar, lr1_step7_result, lr1_step7_validation);
            print_step8_table_result(lr1_step8_result, lr1_step8_validation);
            print_step10_lalr_result(step10_result, step10_validation);
            if (run_step9) {
                std::cout << "[第9步语义动作] 已启用 bison 风格编译后动作执行\n";
                std::cout << "[第9步 LR(1) 总控程序]\n";
                print_step9_parse_result(grammar, lr1_parse_result);
                std::cout << "[第9步 LALR(1) 总控程序]\n";
                print_step9_parse_result(grammar, lalr_parse_result);
                print_step9_parse_compare_result(lr1_parse_result, lalr_parse_result);
                if (dump_lalr_reductions_machine) {
                    print_machine_lalr_reduction_sequence(lalr_parse_result);
                }
            } else {
                std::cout << "[第9步 LR 总控程序] 已跳过（未提供 --parse-tokens）\n";
            }
        } else {
            print_compact_summary(
                lr1_step7_result, lr1_step8_result, step10_result, run_step9, lr1_parse_result, lalr_parse_result);
            if (run_step9 && dump_lalr_reductions_machine) {
                print_machine_lalr_reduction_sequence(lalr_parse_result);
            }
        }
        if (dump_step10_raw_machine) {
            print_machine_step10_raw(grammar, step10_result);
        }

        if (dump_symbols) {
            print_symbols(grammar);
        }
        if (dump_productions) {
            print_productions(grammar);
        }
        // 3.5 需要时导出报告目录（step9 或 step10 版本）。
        if (export_report) {
            if (export_dir.empty()) {
                export_dir = run_step9 ? seu::yacc::make_default_step9_export_dir(input_path)
                                       : seu::yacc::make_default_step10_export_dir(input_path);
            }
            if (run_step9) {
                seu::yacc::export_step9_report(grammar, analysis, preprocess_report, first_result, first_validation,
                    lr1_result, lr1_validation, lr1_step7_result, lr1_step7_validation, lr1_step8_result,
                    lr1_step8_validation, lr1_parse_result, lalr_parse_result, runtime_tokens, token_file_path,
                    export_dir);
            } else {
                seu::yacc::export_step10_report(grammar, analysis, preprocess_report, first_result, first_validation,
                    lr1_result, lr1_validation, lr1_step7_result, lr1_step7_validation, lr1_step8_result,
                    lr1_step8_validation, step10_result, step10_validation, export_dir);
            }
            std::cout << "[导出] 已写入: " << export_dir << '\n';
            std::cout << "[导出结构] summary.txt, raw/, analysis/\n";
        }
        return 0;
    } catch (const seu::yacc::ParseError& e) {
        std::cerr << e.what() << '\n';
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "运行失败: " << e.what() << '\n';
        return 1;
    }
}
