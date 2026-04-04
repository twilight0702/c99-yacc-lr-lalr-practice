#include "yacc/report/report_exporter.h"

#include <filesystem>
#include <fstream>
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

}  // namespace seu::yacc
