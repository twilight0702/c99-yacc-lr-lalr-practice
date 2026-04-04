#include "yacc/report/report_exporter.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace seu::yacc {
namespace {

std::string symbol_kind_to_text(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Terminal:
            return "Terminal";
        case SymbolKind::Nonterminal:
            return "Nonterminal";
        case SymbolKind::Special:
            return "Special";
    }
    return "Unknown";
}

void write_text_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("无法写入文件: " + path.string());
    }
    out << content;
}

}  // namespace

GrammarAnalysis analyze_grammar(const Grammar& grammar) {
    GrammarAnalysis result;

    std::unordered_set<int> rhs_used_terminal_ids;
    for (const auto& prod : grammar.productions) {
        if (prod.action.present) {
            ++result.productions_with_actions;
        }
        for (int rhs_id : prod.rhs_symbol_ids) {
            const auto& sym = grammar.symbols[rhs_id];
            if (sym.kind == SymbolKind::Terminal) {
                rhs_used_terminal_ids.insert(rhs_id);
            }
        }
    }

    for (int tid : grammar.terminal_ids) {
        if (rhs_used_terminal_ids.find(tid) == rhs_used_terminal_ids.end()) {
            result.unused_terminal_ids.push_back(tid);
        }
    }

    // 可达性分析：从开始符号出发，沿非终结符展开产生式 RHS 中的非终结符。
    std::unordered_set<int> reachable;
    std::queue<int> q;
    if (grammar.start_symbol_id >= 0) {
        reachable.insert(grammar.start_symbol_id);
        q.push(grammar.start_symbol_id);
    }
    while (!q.empty()) {
        const int cur = q.front();
        q.pop();

        auto it = grammar.prod_ids_by_lhs.find(cur);
        if (it == grammar.prod_ids_by_lhs.end()) {
            continue;
        }
        for (int pid : it->second) {
            const auto& prod = grammar.productions[pid];
            for (int rhs_id : prod.rhs_symbol_ids) {
                const auto& sym = grammar.symbols[rhs_id];
                if (sym.kind == SymbolKind::Nonterminal && reachable.insert(rhs_id).second) {
                    q.push(rhs_id);
                }
            }
        }
    }
    for (int nid : grammar.nonterminal_ids) {
        if (reachable.find(nid) == reachable.end()) {
            result.unreachable_nonterminal_ids.push_back(nid);
        }
    }

    return result;
}

std::string make_default_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step3" / stem).string();
}

std::string make_default_step4_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step4" / stem).string();
}

std::string make_default_step5_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step5" / stem).string();
}

std::string make_default_step6_export_dir(const std::string& input_path) {
    const std::filesystem::path p(input_path);
    const std::string stem = p.stem().string().empty() ? "input" : p.stem().string();
    return (std::filesystem::path("artifacts") / "yacc" / "step6" / stem).string();
}

void export_report(
    const Grammar& grammar, const GrammarAnalysis& analysis, const std::string& output_dir) {
    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";

    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    if (grammar.start_symbol_id >= 0 &&
        grammar.start_symbol_id < static_cast<int>(grammar.symbols.size())) {
        summary += "start_symbol=" + grammar.symbols[grammar.start_symbol_id].name + "\n";
    }
    write_text_file(root / "summary.txt", summary);

    // 2) raw/symbols.tsv
    std::string symbols_tsv = "id\tname\tkind\tis_literal_char\n";
    for (const auto& sym : grammar.symbols) {
        symbols_tsv += std::to_string(sym.id) + "\t" + sym.name + "\t" +
                       symbol_kind_to_text(sym.kind) + "\t" +
                       (sym.is_literal_char ? "1" : "0") + "\n";
    }
    write_text_file(raw_dir / "symbols.tsv", symbols_tsv);

    // 3) raw/productions.txt
    std::string productions_text;
    for (const auto& p : grammar.productions) {
        productions_text += "#" + std::to_string(p.id) + " ";
        productions_text += grammar.symbols[p.lhs_symbol_id].name + " ->";
        if (p.rhs_symbol_ids.empty()) {
            productions_text += " epsilon";
        } else {
            for (int rhs_id : p.rhs_symbol_ids) {
                productions_text += " " + grammar.symbols[rhs_id].name;
            }
        }
        productions_text += "    [line=" + std::to_string(p.source_line) + "]";
        if (p.action.present) {
            productions_text += " [action=yes]";
        }
        productions_text += "\n";
    }
    write_text_file(raw_dir / "productions.txt", productions_text);

    // 4) raw/user_subroutines.c
    write_text_file(raw_dir / "user_subroutines.c", grammar.user_subroutines_raw);

    // 5) analysis/report.txt
    std::string report;
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    if (!analysis.unused_terminal_ids.empty()) {
        report += "unused_terminals:\n";
        for (int id : analysis.unused_terminal_ids) {
            report += "- " + grammar.symbols[id].name + " (id=" + std::to_string(id) + ")\n";
        }
    }
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";
    if (!analysis.unreachable_nonterminal_ids.empty()) {
        report += "unreachable_nonterminals:\n";
        for (int id : analysis.unreachable_nonterminal_ids) {
            report += "- " + grammar.symbols[id].name + " (id=" + std::to_string(id) + ")\n";
        }
    }
    write_text_file(analysis_dir / "report.txt", report);
}

void export_step4_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const std::string& output_dir) {
    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";

    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=4\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    summary += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    if (grammar.start_symbol_id >= 0 &&
        grammar.start_symbol_id < static_cast<int>(grammar.symbols.size())) {
        summary += "start_symbol=" + grammar.symbols[grammar.start_symbol_id].name + "\n";
    }
    if (grammar.augmented_start_symbol_id >= 0 &&
        grammar.augmented_start_symbol_id < static_cast<int>(grammar.symbols.size())) {
        summary += "augmented_start_symbol=" + grammar.symbols[grammar.augmented_start_symbol_id].name + "\n";
    }
    summary += "augmented_production_id=" + std::to_string(preprocess_report.augmented_production_id) + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/symbols.tsv
    std::string symbols_tsv = "id\tname\tkind\tis_literal_char\n";
    for (const auto& sym : grammar.symbols) {
        symbols_tsv += std::to_string(sym.id) + "\t" + sym.name + "\t" +
                       symbol_kind_to_text(sym.kind) + "\t" +
                       (sym.is_literal_char ? "1" : "0") + "\n";
    }
    write_text_file(raw_dir / "symbols.tsv", symbols_tsv);

    // 3) raw/productions.txt
    std::string productions_text;
    for (const auto& p : grammar.productions) {
        productions_text += "#" + std::to_string(p.id) + " ";
        productions_text += grammar.symbols[p.lhs_symbol_id].name + " ->";
        if (p.rhs_symbol_ids.empty()) {
            productions_text += " epsilon";
        } else {
            for (int rhs_id : p.rhs_symbol_ids) {
                productions_text += " " + grammar.symbols[rhs_id].name;
            }
        }
        productions_text += "    [line=" + std::to_string(p.source_line) + "]";
        if (p.action.present) {
            productions_text += " [action=yes]";
        }
        productions_text += "\n";
    }
    write_text_file(raw_dir / "productions.txt", productions_text);

    // 4) raw/augmented_grammar.txt
    std::string aug;
    if (!grammar.productions.empty()) {
        const auto& p0 = grammar.productions.front();
        aug += "augmented_production=#" + std::to_string(p0.id) + "\n";
        aug += grammar.symbols[p0.lhs_symbol_id].name + " ->";
        for (int rhs_id : p0.rhs_symbol_ids) {
            aug += " " + grammar.symbols[rhs_id].name;
        }
        aug += "\n";
    }
    write_text_file(raw_dir / "augmented_grammar.txt", aug);

    // 5) raw/prod_index_by_lhs.tsv
    std::string index_tsv = "lhs_id\tlhs_name\tproduction_ids\n";
    for (int lhs_id : grammar.nonterminal_ids) {
        index_tsv += std::to_string(lhs_id) + "\t" + grammar.symbols[lhs_id].name + "\t";
        auto it = grammar.prod_ids_by_lhs.find(lhs_id);
        if (it != grammar.prod_ids_by_lhs.end()) {
            for (size_t i = 0; i < it->second.size(); ++i) {
                index_tsv += std::to_string(it->second[i]);
                if (i + 1 < it->second.size()) {
                    index_tsv += ",";
                }
            }
        }
        index_tsv += "\n";
    }
    write_text_file(raw_dir / "prod_index_by_lhs.tsv", index_tsv);

    // 6) raw/user_subroutines.c
    write_text_file(raw_dir / "user_subroutines.c", grammar.user_subroutines_raw);

    // 7) analysis/report.txt
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "preprocess_errors_count=" + std::to_string(preprocess_report.errors.size()) + "\n";
    if (!preprocess_report.errors.empty()) {
        report += "preprocess_errors:\n";
        for (const auto& e : preprocess_report.errors) {
            report += "- " + e + "\n";
        }
    }
    report += "preprocess_warnings_count=" + std::to_string(preprocess_report.warnings.size()) + "\n";
    if (!preprocess_report.warnings.empty()) {
        report += "preprocess_warnings:\n";
        for (const auto& w : preprocess_report.warnings) {
            report += "- " + w + "\n";
        }
    }
    report += "nonterminals_without_productions_count=" +
              std::to_string(preprocess_report.nonterminals_without_productions.size()) + "\n";
    report += "productions_with_epsilon_symbol_in_rhs_count=" +
              std::to_string(preprocess_report.productions_with_epsilon_symbol_in_rhs.size()) + "\n";

    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";

    write_text_file(analysis_dir / "report.txt", report);
}

void export_step5_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const std::string& output_dir) {
    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";

    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=5\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    summary += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    summary += "first_converged=" + std::string(first_result.converged ? "true" : "false") + "\n";
    summary += "first_iterations=" + std::to_string(first_result.iteration_count) + "\n";
    summary += "nullable_nonterminals_count=" +
               std::to_string(first_result.nullable_nonterminal_ids.size()) + "\n";
    summary += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/symbols.tsv
    std::string symbols_tsv = "id\tname\tkind\tis_literal_char\n";
    for (const auto& sym : grammar.symbols) {
        symbols_tsv += std::to_string(sym.id) + "\t" + sym.name + "\t" +
                       symbol_kind_to_text(sym.kind) + "\t" +
                       (sym.is_literal_char ? "1" : "0") + "\n";
    }
    write_text_file(raw_dir / "symbols.tsv", symbols_tsv);

    // 3) raw/productions.txt
    std::string productions_text;
    for (const auto& p : grammar.productions) {
        productions_text += "#" + std::to_string(p.id) + " ";
        productions_text += grammar.symbols[p.lhs_symbol_id].name + " ->";
        if (p.rhs_symbol_ids.empty()) {
            productions_text += " epsilon";
        } else {
            for (int rhs_id : p.rhs_symbol_ids) {
                productions_text += " " + grammar.symbols[rhs_id].name;
            }
        }
        productions_text += "    [line=" + std::to_string(p.source_line) + "]";
        if (p.action.present) {
            productions_text += " [action=yes]";
        }
        productions_text += "\n";
    }
    write_text_file(raw_dir / "productions.txt", productions_text);

    // 4) raw/augmented_grammar.txt
    std::string aug;
    if (!grammar.productions.empty()) {
        const auto& p0 = grammar.productions.front();
        aug += "augmented_production=#" + std::to_string(p0.id) + "\n";
        aug += grammar.symbols[p0.lhs_symbol_id].name + " ->";
        for (int rhs_id : p0.rhs_symbol_ids) {
            aug += " " + grammar.symbols[rhs_id].name;
        }
        aug += "\n";
    }
    write_text_file(raw_dir / "augmented_grammar.txt", aug);

    // 5) raw/prod_index_by_lhs.tsv
    std::string index_tsv = "lhs_id\tlhs_name\tproduction_ids\n";
    for (int lhs_id : grammar.nonterminal_ids) {
        index_tsv += std::to_string(lhs_id) + "\t" + grammar.symbols[lhs_id].name + "\t";
        auto it = grammar.prod_ids_by_lhs.find(lhs_id);
        if (it != grammar.prod_ids_by_lhs.end()) {
            for (size_t i = 0; i < it->second.size(); ++i) {
                index_tsv += std::to_string(it->second[i]);
                if (i + 1 < it->second.size()) {
                    index_tsv += ",";
                }
            }
        }
        index_tsv += "\n";
    }
    write_text_file(raw_dir / "prod_index_by_lhs.tsv", index_tsv);

    // 6) raw/first_sets.tsv
    std::string first_tsv = "symbol_id\tsymbol_name\tkind\tfirst_set\n";
    for (const auto& sym : grammar.symbols) {
        first_tsv += std::to_string(sym.id) + "\t" + sym.name + "\t" + symbol_kind_to_text(sym.kind) + "\t";
        if (sym.id >= 0 && sym.id < static_cast<int>(first_result.first_sets_by_symbol_id.size())) {
            const auto& first_set = first_result.first_sets_by_symbol_id[sym.id];
            bool first_item = true;
            for (int member_id : first_set) {
                if (!first_item) {
                    first_tsv += ",";
                }
                first_tsv += grammar.symbols[member_id].name;
                first_item = false;
            }
        }
        first_tsv += "\n";
    }
    write_text_file(raw_dir / "first_sets.tsv", first_tsv);

    // 7) raw/first_sequence_examples.txt
    std::ostringstream sequence_examples;
    sequence_examples << "examples_for_first_of_sequence\n";
    if (!grammar.productions.empty()) {
        const auto& p0 = grammar.productions.front();
        sequence_examples << "prod#0_rhs:";
        for (int sid : p0.rhs_symbol_ids) {
            sequence_examples << " " << grammar.symbols[sid].name;
        }
        sequence_examples << "\nFIRST(prod#0_rhs)={";
        const std::set<int> first_p0_rhs = compute_first_of_sequence(grammar, first_result, p0.rhs_symbol_ids, 0);
        bool first_item = true;
        for (int sid : first_p0_rhs) {
            if (!first_item) {
                sequence_examples << ", ";
            }
            sequence_examples << grammar.symbols[sid].name;
            first_item = false;
        }
        sequence_examples << "}\n";
    }
    write_text_file(raw_dir / "first_sequence_examples.txt", sequence_examples.str());

    // 8) raw/user_subroutines.c
    write_text_file(raw_dir / "user_subroutines.c", grammar.user_subroutines_raw);

    // 9) analysis/report.txt
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "preprocess_errors_count=" + std::to_string(preprocess_report.errors.size()) + "\n";
    report += "preprocess_warnings_count=" + std::to_string(preprocess_report.warnings.size()) + "\n";
    report += "first_converged=" + std::string(first_result.converged ? "true" : "false") + "\n";
    report += "first_iterations=" + std::to_string(first_result.iteration_count) + "\n";
    report += "nullable_nonterminals_count=" +
              std::to_string(first_result.nullable_nonterminal_ids.size()) + "\n";
    if (!first_result.nullable_nonterminal_ids.empty()) {
        report += "nullable_nonterminals:\n";
        for (int sid : first_result.nullable_nonterminal_ids) {
            report += "- " + grammar.symbols[sid].name + " (id=" + std::to_string(sid) + ")\n";
        }
    }
    report += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    report += "first_validation_errors_count=" + std::to_string(first_validation.errors.size()) + "\n";
    if (!first_validation.errors.empty()) {
        report += "first_validation_errors:\n";
        for (const auto& e : first_validation.errors) {
            report += "- " + e + "\n";
        }
    }
    report += "first_validation_warnings_count=" + std::to_string(first_validation.warnings.size()) + "\n";
    if (!first_validation.warnings.empty()) {
        report += "first_validation_warnings:\n";
        for (const auto& w : first_validation.warnings) {
            report += "- " + w + "\n";
        }
    }
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";

    write_text_file(analysis_dir / "report.txt", report);
}

void export_step6_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_result,
    const LR1Step6ValidationReport& lr1_validation, const std::string& output_dir) {
    // 复用 step5 导出作为基础，再追加第 6 步文件，保证目录结构稳定。
    export_step5_report(
        grammar, analysis, preprocess_report, first_result, first_validation, output_dir);

    const std::filesystem::path root(output_dir);
    const std::filesystem::path raw_dir = root / "raw";
    const std::filesystem::path analysis_dir = root / "analysis";

    std::filesystem::create_directories(raw_dir);
    std::filesystem::create_directories(analysis_dir);

    // 1) summary.txt：覆盖为 step6 信息。
    std::string summary;
    summary += "source=" + grammar.source_path + "\n";
    summary += "step=6\n";
    summary += "symbols=" + std::to_string(grammar.symbols.size()) + "\n";
    summary += "terminals=" + std::to_string(grammar.terminal_ids.size()) + "\n";
    summary += "nonterminals=" + std::to_string(grammar.nonterminal_ids.size()) + "\n";
    summary += "productions_with_augmented=" + std::to_string(grammar.productions.size()) + "\n";
    summary += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    summary += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    summary += "i0_kernel_items=" + std::to_string(lr1_result.i0_kernel_items.size()) + "\n";
    summary += "i0_closure_items=" + std::to_string(lr1_result.i0_closure_items.size()) + "\n";
    summary += "i0_goto_edges=" + std::to_string(lr1_result.goto_symbol_ids.size()) + "\n";
    summary += "lr1_step6_validation_passed=" + std::string(lr1_validation.passed ? "true" : "false") + "\n";
    write_text_file(root / "summary.txt", summary);

    // 2) raw/lr1_i0_items.txt
    std::ostringstream i0_items;
    i0_items << "[I0 kernel]\n";
    for (const auto& item : lr1_result.i0_kernel_items) {
        i0_items << format_lr1_item(grammar, item) << "\n";
    }
    i0_items << "\n[I0 closure]\n";
    for (const auto& item : lr1_result.i0_closure_items) {
        i0_items << format_lr1_item(grammar, item) << "\n";
    }
    write_text_file(raw_dir / "lr1_i0_items.txt", i0_items.str());

    // 3) raw/lr1_i0_goto.tsv
    std::string goto_tsv = "symbol_id\tsymbol_name\titem_count\n";
    for (std::size_t i = 0; i < lr1_result.goto_symbol_ids.size(); ++i) {
        const int sid = lr1_result.goto_symbol_ids[i];
        const std::string name =
            (sid >= 0 && sid < static_cast<int>(grammar.symbols.size())) ? grammar.symbols[sid].name
                                                                          : "<invalid>";
        goto_tsv += std::to_string(sid) + "\t" + name + "\t" +
                    std::to_string(lr1_result.goto_item_sets[i].size()) + "\n";
    }
    write_text_file(raw_dir / "lr1_i0_goto.tsv", goto_tsv);

    // 4) raw/lr1_i0_goto_items.txt
    std::ostringstream goto_items_text;
    for (std::size_t i = 0; i < lr1_result.goto_symbol_ids.size(); ++i) {
        const int sid = lr1_result.goto_symbol_ids[i];
        const std::string name =
            (sid >= 0 && sid < static_cast<int>(grammar.symbols.size())) ? grammar.symbols[sid].name
                                                                          : "<invalid>";
        goto_items_text << "[goto(I0, " << name << ") items]\n";
        for (const auto& item : lr1_result.goto_item_sets[i]) {
            goto_items_text << format_lr1_item(grammar, item) << "\n";
        }
        goto_items_text << "\n";
    }
    write_text_file(raw_dir / "lr1_i0_goto_items.txt", goto_items_text.str());

    // 5) raw/lr1_lookahead_derivation.txt
    std::ostringstream lookahead_notes;
    lookahead_notes << "lookahead_derivation_notes_count="
                    << lr1_result.lookahead_derivation_notes.size() << "\n";
    for (const auto& line : lr1_result.lookahead_derivation_notes) {
        lookahead_notes << "- " << line << "\n";
    }
    write_text_file(raw_dir / "lr1_lookahead_derivation.txt", lookahead_notes.str());

    // 6) analysis/report.txt：覆盖为 step6 诊断结果。
    std::string report;
    report += "preprocess_passed=" + std::string(preprocess_report.passed ? "true" : "false") + "\n";
    report += "first_validation_passed=" + std::string(first_validation.passed ? "true" : "false") + "\n";
    report += "lr1_i0_kernel_items=" + std::to_string(lr1_result.i0_kernel_items.size()) + "\n";
    report += "lr1_i0_closure_items=" + std::to_string(lr1_result.i0_closure_items.size()) + "\n";
    report += "lr1_i0_goto_edges=" + std::to_string(lr1_result.goto_symbol_ids.size()) + "\n";
    report += "lr1_lookahead_derivation_notes_count=" +
              std::to_string(lr1_result.lookahead_derivation_notes.size()) + "\n";
    report += "lr1_step6_validation_passed=" + std::string(lr1_validation.passed ? "true" : "false") + "\n";
    report += "lr1_step6_errors_count=" + std::to_string(lr1_validation.errors.size()) + "\n";
    if (!lr1_validation.errors.empty()) {
        report += "lr1_step6_errors:\n";
        for (const auto& e : lr1_validation.errors) {
            report += "- " + e + "\n";
        }
    }
    report += "lr1_step6_warnings_count=" + std::to_string(lr1_validation.warnings.size()) + "\n";
    if (!lr1_validation.warnings.empty()) {
        report += "lr1_step6_warnings:\n";
        for (const auto& w : lr1_validation.warnings) {
            report += "- " + w + "\n";
        }
    }
    report += "productions_with_actions=" + std::to_string(analysis.productions_with_actions) + "\n";
    report += "unused_terminals_count=" + std::to_string(analysis.unused_terminal_ids.size()) + "\n";
    report += "unreachable_nonterminals_count=" +
              std::to_string(analysis.unreachable_nonterminal_ids.size()) + "\n";

    write_text_file(analysis_dir / "report.txt", report);
}

}  // namespace seu::yacc
