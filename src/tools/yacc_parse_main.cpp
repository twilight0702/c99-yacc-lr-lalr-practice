#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "yacc/first/first_set.h"
#include "yacc/lr1/lr1_items.h"
#include "yacc/model/grammar.h"
#include "yacc/parser/yacc_parser.h"
#include "yacc/preprocess/grammar_preprocessor.h"
#include "yacc/report/report_exporter.h"

namespace {

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

void print_symbols(const seu::yacc::Grammar& grammar) {
    std::cout << "\n[符号表]\n";
    for (const auto& sym : grammar.symbols) {
        std::cout << "id=" << sym.id << ", name=" << sym.name
                  << ", kind=" << symbol_kind_to_text(sym.kind)
                  << ", literal=" << (sym.is_literal_char ? "yes" : "no") << '\n';
    }
}

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

}  // namespace

int main(int argc, char** argv) {
    std::string input_path = "c99.y";
    bool dump_symbols = false;
    bool dump_productions = false;
    bool run_validate = true;
    bool export_report = false;
    std::string export_dir;

    // 轻量参数解析：
    // yacc_parse_tool [input.y] [--dump-symbols] [--dump-productions]
    //                [--export] [--export-dir <dir>] [--no-validate]
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--dump-symbols") {
            dump_symbols = true;
            continue;
        }
        if (arg == "--dump-productions") {
            dump_productions = true;
            continue;
        }
        if (arg == "--no-validate") {
            run_validate = false;
            continue;
        }
        if (arg == "--export") {
            export_report = true;
            continue;
        }
        if (arg == "--export-dir") {
            if (i + 1 >= argc) {
                std::cerr << "--export-dir 缺少目录参数\n";
                return 1;
            }
            export_report = true;
            export_dir = argv[++i];
            continue;
        }
        input_path = arg;
    }

    try {
        seu::yacc::Grammar grammar = seu::yacc::parse_yacc_file(input_path);
        print_summary(grammar);
        const seu::yacc::GrammarPreprocessReport preprocess_report = seu::yacc::preprocess_grammar(grammar);
        const seu::yacc::GrammarAnalysis analysis = seu::yacc::analyze_grammar(grammar);
        const seu::yacc::FirstSetResult first_result = seu::yacc::compute_first_sets(grammar);
        const seu::yacc::FirstSetValidationReport first_validation =
            seu::yacc::validate_first_sets(grammar, first_result);
        const seu::yacc::LR1Step6Result lr1_result = seu::yacc::build_step6_lr1_items(grammar, first_result);
        const seu::yacc::LR1Step6ValidationReport lr1_validation =
            seu::yacc::validate_step6_lr1_items(grammar, first_result, lr1_result);
        const seu::yacc::LR1Step7Result lr1_step7_result =
            seu::yacc::build_step7_lr1_canonical_collection(grammar, first_result);
        const seu::yacc::LR1Step7ValidationReport lr1_step7_validation =
            seu::yacc::validate_step7_lr1_canonical_collection(grammar, first_result, lr1_step7_result);

        if (run_validate) {
            std::string validate_error;
            if (validate_grammar(grammar, validate_error)) {
                std::cout << "[校验] 通过\n";
            } else {
                std::cout << "[校验] 失败: " << validate_error << '\n';
                return 3;
            }
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
        } else {
            std::cout << "[校验] 已跳过\n";
        }
        print_preprocess_result(grammar, preprocess_report);
        print_first_set_result(grammar, first_result, first_validation);
        print_step6_lr1_result(grammar, lr1_result, lr1_validation);
        print_step7_lr1_result(grammar, lr1_step7_result, lr1_step7_validation);

        if (dump_symbols) {
            print_symbols(grammar);
        }
        if (dump_productions) {
            print_productions(grammar);
        }
        if (export_report) {
            if (export_dir.empty()) {
                export_dir = seu::yacc::make_default_step7_export_dir(input_path);
            }
            seu::yacc::export_step7_report(grammar, analysis, preprocess_report, first_result,
                first_validation, lr1_result, lr1_validation, lr1_step7_result, lr1_step7_validation, export_dir);
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
