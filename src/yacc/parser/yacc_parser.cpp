/**
 * 文件说明：实现 .y 文法文件解析器。
 * 负责读取 definitions/rules/user code 三段内容，
 * 解析符号与产生式并构建内部 Grammar 结构。
 */

#include "yacc/parser/yacc_parser.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace seu::yacc {
namespace {

struct SectionRanges {
    std::string definitions;
    std::string rules;
    std::string user_subroutines;
    int definitions_start_line = 1;
    int rules_start_line = 1;
    int user_start_line = 1;
};

struct ProductionDraft {
    std::string lhs_name;
    std::vector<std::string> rhs_symbol_names;
    std::vector<bool> rhs_is_literal_char;
    ActionBlock action;
    std::string precedence_override_symbol_name;
    int source_line = 0;
};

struct DefinitionParseResult {
    std::unordered_set<std::string> terminal_names;
    std::unordered_map<std::string, std::string> symbol_type_tag_by_name;
    std::unordered_map<std::string, PrecedenceDecl> precedence_by_symbol_name;
    std::string union_block_raw;
};

std::string read_text_file(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("无法打开文件: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool is_identifier_start(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

bool is_identifier_char(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

std::string trim(const std::string& s) {
    size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])) != 0) {
        ++begin;
    }

    size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }
    return s.substr(begin, end - begin);
}

std::string strip_comments_for_directives(const std::string& line) {
    std::string out;
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;

    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        const char next = (i + 1 < line.size()) ? line[i + 1] : '\0';

        if (in_string) {
            out.push_back(ch);
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (in_char) {
            out.push_back(ch);
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '\'') {
                in_char = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
            out.push_back(ch);
            continue;
        }
        if (ch == '\'') {
            in_char = true;
            out.push_back(ch);
            continue;
        }
        if (ch == '/' && next == '/') {
            break;
        }
        if (ch == '/' && next == '*') {
            i += 2;
            while (i < line.size()) {
                if (line[i] == '*' && i + 1 < line.size() && line[i + 1] == '/') {
                    ++i;
                    break;
                }
                ++i;
            }
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

std::vector<std::string> split_lines_keep_newline(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char ch : text) {
        current.push_back(ch);
        if (ch == '\n') {
            lines.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

SectionRanges split_sections(const std::string& text) {
    const std::vector<std::string> lines = split_lines_keep_newline(text);

    int first_marker_line = -1;
    int second_marker_line = -1;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const std::string normalized = trim(lines[i]);
        if (normalized == "%%") {
            if (first_marker_line < 0) {
                first_marker_line = i + 1;
            } else if (second_marker_line < 0) {
                second_marker_line = i + 1;
            }
        }
    }

    if (first_marker_line < 0) {
        throw ParseError(1, 1, "Yacc 文件至少需要一个 `%%` 分隔符");
    }

    SectionRanges sections;
    sections.definitions_start_line = 1;
    sections.rules_start_line = first_marker_line + 1;
    sections.user_start_line = (second_marker_line > 0) ? (second_marker_line + 1) : (static_cast<int>(lines.size()) + 1);

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const int line_no = i + 1;
        if (line_no < first_marker_line) {
            sections.definitions += lines[i];
        } else if (line_no > first_marker_line && (second_marker_line < 0 || line_no < second_marker_line)) {
            sections.rules += lines[i];
        } else if (second_marker_line > 0 && line_no > second_marker_line) {
            sections.user_subroutines += lines[i];
        }
    }

    return sections;
}

void register_symbol_if_absent(
    Grammar& grammar, const std::string& name, SymbolKind kind, bool is_literal_char = false) {
    if (grammar.symbol_id_by_name.find(name) != grammar.symbol_id_by_name.end()) {
        return;
    }

    Symbol symbol;
    symbol.id = static_cast<int>(grammar.symbols.size());
    symbol.name = name;
    symbol.kind = kind;
    symbol.is_literal_char = is_literal_char;

    grammar.symbols.push_back(symbol);
    grammar.symbol_id_by_name.emplace(name, symbol.id);

    if (kind == SymbolKind::Terminal) {
        grammar.terminal_ids.push_back(symbol.id);
    } else if (kind == SymbolKind::Nonterminal) {
        grammar.nonterminal_ids.push_back(symbol.id);
    }
}

std::vector<std::string> tokenize_definition_tail(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    bool in_angle = false;
    bool in_char = false;
    bool escaped = false;

    for (char ch : text) {
        if (in_char) {
            cur.push_back(ch);
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '\'') {
                in_char = false;
            }
            continue;
        }
        if (in_angle) {
            cur.push_back(ch);
            if (ch == '>') {
                in_angle = false;
                out.push_back(cur);
                cur.clear();
            }
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            continue;
        }
        if (ch == '<') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            cur.push_back(ch);
            in_angle = true;
            continue;
        }
        if (ch == '\'') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            cur.push_back(ch);
            in_char = true;
            continue;
        }
        cur.push_back(ch);
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

Associativity assoc_from_directive(const std::string& directive) {
    if (directive == "%left") {
        return Associativity::Left;
    }
    if (directive == "%right") {
        return Associativity::Right;
    }
    if (directive == "%nonassoc") {
        return Associativity::Nonassoc;
    }
    return Associativity::None;
}

std::string parse_type_tag_token(const std::string& token, int line) {
    if (token.size() < 3 || token.front() != '<' || token.back() != '>') {
        throw ParseError(line, 1, "类型标签格式非法，期望 <tag>");
    }
    return token.substr(1, token.size() - 2);
}

void parse_definitions(const std::string& definitions, int base_line, DefinitionParseResult& result,
    std::string& start_symbol_name) {
    const std::vector<std::string> lines = split_lines_keep_newline(definitions);

    bool in_code_block = false;
    int precedence_level = 0;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const int current_line = base_line + i;
        std::string line = lines[i];
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        const std::string normalized = trim(strip_comments_for_directives(line));

        if (normalized.empty()) {
            continue;
        }

        if (normalized == "%{") {
            in_code_block = true;
            continue;
        }
        if (normalized == "%}") {
            in_code_block = false;
            continue;
        }
        if (in_code_block) {
            continue;
        }

        if (normalized.rfind("%union", 0) == 0) {
            // 保留 %union 原文并做基本括号配平。
            std::string block;
            int brace_depth = 0;
            bool started = false;
            int j = i;
            for (; j < static_cast<int>(lines.size()); ++j) {
                std::string chunk = lines[j];
                block += chunk;
                for (char ch : chunk) {
                    if (ch == '{') {
                        ++brace_depth;
                        started = true;
                    } else if (ch == '}') {
                        --brace_depth;
                    }
                }
                if (started && brace_depth == 0) {
                    break;
                }
            }
            if (!started || brace_depth != 0) {
                throw ParseError(current_line, 1, "%union 代码块未正确闭合");
            }
            result.union_block_raw = block;
            i = j;
            continue;
        }

        if (normalized.rfind("%start", 0) == 0) {
            std::istringstream iss(normalized.substr(6));
            std::string start;
            if (!(iss >> start)) {
                throw ParseError(current_line, 1, "%start 后未找到开始符号");
            }
            if (!start_symbol_name.empty() && start_symbol_name != start) {
                throw ParseError(current_line, 1, "检测到多个不同的 %start 声明");
            }
            start_symbol_name = start;
            continue;
        }

        if (normalized.rfind("%token", 0) == 0 || normalized.rfind("%left", 0) == 0 ||
            normalized.rfind("%right", 0) == 0 || normalized.rfind("%nonassoc", 0) == 0 ||
            normalized.rfind("%type", 0) == 0) {
            const std::size_t first_space = normalized.find_first_of(" \t");
            const std::string directive =
                (first_space == std::string::npos) ? normalized : normalized.substr(0, first_space);
            const std::string tail =
                (first_space == std::string::npos) ? "" : trim(normalized.substr(first_space));
            const std::vector<std::string> items = tokenize_definition_tail(tail);

            std::string explicit_type_tag;
            std::size_t pos = 0;
            if (!items.empty() && items[0].size() >= 3 && items[0].front() == '<' && items[0].back() == '>') {
                explicit_type_tag = parse_type_tag_token(items[0], current_line);
                pos = 1;
            }

            if (pos >= items.size()) {
                throw ParseError(current_line, 1, directive + " 后未找到符号列表");
            }

            if (directive == "%left" || directive == "%right" || directive == "%nonassoc") {
                ++precedence_level;
                PrecedenceDecl pd;
                pd.level = precedence_level;
                pd.assoc = assoc_from_directive(directive);
                for (; pos < items.size(); ++pos) {
                    const std::string& name = items[pos];
                    result.terminal_names.insert(name);
                    result.precedence_by_symbol_name[name] = pd;
                }
                continue;
            }

            for (; pos < items.size(); ++pos) {
                const std::string& name = items[pos];
                if (directive == "%token") {
                    result.terminal_names.insert(name);
                }
                if (!explicit_type_tag.empty()) {
                    result.symbol_type_tag_by_name[name] = explicit_type_tag;
                }
            }
            continue;
        }

        // 其余 definitions 指令保留兼容，不阻断第一版与扩展版输入。
    }
}

struct RuleCursor {
    const std::string& text;
    size_t pos = 0;
    int line = 1;
    int column = 1;
};

bool is_eof(const RuleCursor& c) {
    return c.pos >= c.text.size();
}

char peek(const RuleCursor& c) {
    return is_eof(c) ? '\0' : c.text[c.pos];
}

char advance(RuleCursor& c) {
    if (is_eof(c)) {
        return '\0';
    }
    char ch = c.text[c.pos++];
    if (ch == '\n') {
        ++c.line;
        c.column = 1;
    } else {
        ++c.column;
    }
    return ch;
}

void skip_spaces(RuleCursor& c) {
    while (!is_eof(c)) {
        char ch = peek(c);
        if (std::isspace(static_cast<unsigned char>(ch)) == 0) {
            break;
        }
        advance(c);
    }
}

void skip_spaces_and_comments(RuleCursor& c, int base_line) {
    while (!is_eof(c)) {
        const char ch = peek(c);
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            advance(c);
            continue;
        }
        if (ch == '/' && c.pos + 1 < c.text.size()) {
            const char next = c.text[c.pos + 1];
            if (next == '/') {
                advance(c);
                advance(c);
                while (!is_eof(c) && peek(c) != '\n') {
                    advance(c);
                }
                continue;
            }
            if (next == '*') {
                advance(c);
                advance(c);
                bool closed = false;
                while (!is_eof(c)) {
                    const char cur = advance(c);
                    if (cur == '*' && !is_eof(c) && peek(c) == '/') {
                        advance(c);
                        closed = true;
                        break;
                    }
                }
                if (!closed) {
                    throw ParseError(base_line + c.line - 1, c.column, "块注释未闭合");
                }
                continue;
            }
        }
        break;
    }
}

[[noreturn]] void fail_here(const RuleCursor& c, const std::string& message, int base_line) {
    throw ParseError(base_line + c.line - 1, c.column, message);
}

std::string parse_identifier(RuleCursor& c, int base_line) {
    if (!is_identifier_start(peek(c))) {
        fail_here(c, "期望标识符", base_line);
    }
    std::string out;
    out.push_back(advance(c));
    while (!is_eof(c) && is_identifier_char(peek(c))) {
        out.push_back(advance(c));
    }
    return out;
}

std::string parse_char_literal(RuleCursor& c, int base_line) {
    if (peek(c) != '\'') {
        fail_here(c, "期望字符字面量", base_line);
    }
    std::string out;
    out.push_back(advance(c));
    bool escaped = false;
    while (!is_eof(c)) {
        char ch = advance(c);
        out.push_back(ch);
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '\'') {
            return out;
        }
    }
    fail_here(c, "字符字面量未闭合", base_line);
}

std::string parse_action_block(RuleCursor& c, int base_line) {
    if (peek(c) != '{') {
        fail_here(c, "期望动作块起始 `{`", base_line);
    }
    std::string raw;
    int brace_depth = 0;
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;
    bool in_line_comment = false;
    bool in_block_comment = false;

    while (!is_eof(c)) {
        const char ch = advance(c);
        raw.push_back(ch);

        if (in_line_comment) {
            if (ch == '\n') {
                in_line_comment = false;
            }
            continue;
        }
        if (in_block_comment) {
            if (ch == '*' && !is_eof(c) && peek(c) == '/') {
                raw.push_back(advance(c));
                in_block_comment = false;
            }
            continue;
        }
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (in_char) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '\'') {
                in_char = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch == '\'') {
            in_char = true;
            continue;
        }
        if (ch == '/' && !is_eof(c) && peek(c) == '/') {
            raw.push_back(advance(c));
            in_line_comment = true;
            continue;
        }
        if (ch == '/' && !is_eof(c) && peek(c) == '*') {
            raw.push_back(advance(c));
            in_block_comment = true;
            continue;
        }
        if (ch == '{') {
            ++brace_depth;
            continue;
        }
        if (ch == '}') {
            --brace_depth;
            if (brace_depth == 0) {
                return raw;
            }
        }
    }
    fail_here(c, "动作块未闭合", base_line);
}

void append_production_from_buffer(const std::string& lhs_name, const std::vector<std::string>& rhs_names,
    const std::vector<bool>& rhs_is_literal, const ActionBlock& action,
    const std::string& precedence_override_symbol_name, int source_line,
    std::vector<ProductionDraft>& out_drafts) {
    ProductionDraft draft;
    draft.lhs_name = lhs_name;
    draft.rhs_symbol_names = rhs_names;
    draft.rhs_is_literal_char = rhs_is_literal;
    draft.action = action;
    draft.precedence_override_symbol_name = precedence_override_symbol_name;
    draft.source_line = source_line;
    out_drafts.push_back(std::move(draft));
}

void parse_rules(const std::string& rules, int base_line, std::vector<ProductionDraft>& drafts,
    std::unordered_set<std::string>& lhs_names) {
    RuleCursor c{rules, 0, 1, 1};

    while (!is_eof(c)) {
        skip_spaces_and_comments(c, base_line);
        if (is_eof(c)) {
            break;
        }

        const int rule_line = base_line + c.line - 1;
        const std::string lhs_name = parse_identifier(c, base_line);
        lhs_names.insert(lhs_name);

        skip_spaces_and_comments(c, base_line);
        if (peek(c) != ':') {
            fail_here(c, "产生式左部后缺少 `:`", base_line);
        }
        advance(c);

        std::vector<std::string> rhs_buffer;
        std::vector<bool> rhs_literal_buffer;
        ActionBlock action_buffer;
        std::string precedence_override_symbol_name;

        while (!is_eof(c)) {
            skip_spaces_and_comments(c, base_line);
            if (is_eof(c)) {
                fail_here(c, "规则未正常结束，缺少 `;`", base_line);
            }

            const char ch = peek(c);
            if (ch == '|') {
                append_production_from_buffer(lhs_name, rhs_buffer, rhs_literal_buffer, action_buffer,
                    precedence_override_symbol_name, rule_line, drafts);
                rhs_buffer.clear();
                rhs_literal_buffer.clear();
                action_buffer = ActionBlock{};
                precedence_override_symbol_name.clear();
                advance(c);
                continue;
            }
            if (ch == ';') {
                append_production_from_buffer(lhs_name, rhs_buffer, rhs_literal_buffer, action_buffer,
                    precedence_override_symbol_name, rule_line, drafts);
                advance(c);
                break;
            }
            if (ch == '{') {
                if (action_buffer.present) {
                    fail_here(c, "同一候选式中不允许多个动作块", base_line);
                }
                action_buffer.present = true;
                action_buffer.raw_code = parse_action_block(c, base_line);
                continue;
            }
            if (ch == '\'') {
                const std::string lit = parse_char_literal(c, base_line);
                rhs_buffer.push_back(lit);
                rhs_literal_buffer.push_back(true);
                continue;
            }
            if (ch == '%') {
                advance(c);
                const std::string directive = parse_identifier(c, base_line);
                if (directive != "prec") {
                    fail_here(c, "规则段仅支持 %prec 指令", base_line);
                }
                skip_spaces_and_comments(c, base_line);
                if (is_eof(c)) {
                    fail_here(c, "%prec 后缺少符号", base_line);
                }
                if (!precedence_override_symbol_name.empty()) {
                    fail_here(c, "同一候选式中不允许多个 %prec", base_line);
                }
                if (peek(c) == '\'') {
                    precedence_override_symbol_name = parse_char_literal(c, base_line);
                } else {
                    precedence_override_symbol_name = parse_identifier(c, base_line);
                }
                continue;
            }
            if (ch == '<') {
                fail_here(c, "规则段不支持类型标签", base_line);
            }
            if (is_identifier_start(ch)) {
                rhs_buffer.push_back(parse_identifier(c, base_line));
                rhs_literal_buffer.push_back(false);
                continue;
            }

            fail_here(c, std::string("无法识别的规则符号: `") + ch + "`", base_line);
        }
    }
}

int compute_default_precedence_symbol_id(const Grammar& grammar, const Production& p) {
    for (int i = static_cast<int>(p.rhs_symbol_ids.size()) - 1; i >= 0; --i) {
        const int sid = p.rhs_symbol_ids[i];
        if (sid < 0 || sid >= static_cast<int>(grammar.symbols.size())) {
            continue;
        }
        if (grammar.symbols[sid].kind == SymbolKind::Terminal) {
            return sid;
        }
    }
    return -1;
}

void finalize_grammar(Grammar& grammar, const DefinitionParseResult& def_result,
    const std::unordered_set<std::string>& lhs_names, const std::vector<ProductionDraft>& drafts,
    const std::string& start_symbol_name_raw) {
    for (const auto& terminal : def_result.terminal_names) {
        const bool is_literal = terminal.size() >= 2 && terminal.front() == '\'' && terminal.back() == '\'';
        register_symbol_if_absent(grammar, terminal, SymbolKind::Terminal, is_literal);
    }

    for (const auto& lhs : lhs_names) {
        register_symbol_if_absent(grammar, lhs, SymbolKind::Nonterminal, false);
    }

    register_symbol_if_absent(grammar, kEofSymbolName, SymbolKind::Special, false);
    register_symbol_if_absent(grammar, kEpsilonSymbolName, SymbolKind::Special, false);
    register_symbol_if_absent(grammar, kAugmentedStartName, SymbolKind::Special, false);
    grammar.eof_symbol_id = grammar.symbol_id_by_name.at(kEofSymbolName);
    grammar.epsilon_symbol_id = grammar.symbol_id_by_name.at(kEpsilonSymbolName);
    grammar.augmented_start_symbol_id = grammar.symbol_id_by_name.at(kAugmentedStartName);

    std::string start_symbol_name = start_symbol_name_raw;
    if (start_symbol_name.empty()) {
        if (drafts.empty()) {
            throw ParseError(1, 1, "缺少规则，无法推断开始符号");
        }
        start_symbol_name = drafts.front().lhs_name;
    }
    auto start_it = grammar.symbol_id_by_name.find(start_symbol_name);
    if (start_it == grammar.symbol_id_by_name.end()) {
        throw ParseError(1, 1, "开始符号未在规则左部出现: " + start_symbol_name);
    }
    grammar.start_symbol_id = start_it->second;

    Production augmented;
    augmented.id = 0;
    augmented.lhs_symbol_id = grammar.augmented_start_symbol_id;
    augmented.rhs_symbol_ids = {grammar.start_symbol_id};
    augmented.source_line = 1;
    grammar.productions.push_back(augmented);

    int next_prod_id = 1;
    for (const auto& draft : drafts) {
        Production p;
        p.id = next_prod_id++;
        p.source_line = draft.source_line;
        p.action = draft.action;

        auto lhs_it = grammar.symbol_id_by_name.find(draft.lhs_name);
        if (lhs_it == grammar.symbol_id_by_name.end()) {
            throw ParseError(draft.source_line, 1, "未知的产生式左部: " + draft.lhs_name);
        }
        p.lhs_symbol_id = lhs_it->second;

        for (size_t i = 0; i < draft.rhs_symbol_names.size(); ++i) {
            const std::string& name = draft.rhs_symbol_names[i];
            const bool is_literal = draft.rhs_is_literal_char[i];

            if (is_literal) {
                register_symbol_if_absent(grammar, name, SymbolKind::Terminal, true);
            } else {
                if (def_result.terminal_names.find(name) != def_result.terminal_names.end()) {
                    register_symbol_if_absent(grammar, name, SymbolKind::Terminal, false);
                } else if (lhs_names.find(name) != lhs_names.end()) {
                    register_symbol_if_absent(grammar, name, SymbolKind::Nonterminal, false);
                } else {
                    throw ParseError(draft.source_line, 1, "规则右部出现未定义符号: " + name);
                }
            }
            p.rhs_symbol_ids.push_back(grammar.symbol_id_by_name.at(name));
        }

        if (!draft.precedence_override_symbol_name.empty()) {
            auto it = grammar.symbol_id_by_name.find(draft.precedence_override_symbol_name);
            if (it == grammar.symbol_id_by_name.end()) {
                throw ParseError(draft.source_line, 1,
                    "%prec 引用未定义终结符: " + draft.precedence_override_symbol_name);
            }
            p.precedence_symbol_id = it->second;
        } else {
            p.precedence_symbol_id = compute_default_precedence_symbol_id(grammar, p);
        }

        grammar.productions.push_back(std::move(p));
    }

    for (const auto& kv : def_result.symbol_type_tag_by_name) {
        const auto it = grammar.symbol_id_by_name.find(kv.first);
        if (it != grammar.symbol_id_by_name.end()) {
            grammar.symbol_type_tag_by_id[it->second] = kv.second;
        }
    }

    for (const auto& kv : def_result.precedence_by_symbol_name) {
        const auto it = grammar.symbol_id_by_name.find(kv.first);
        if (it == grammar.symbol_id_by_name.end()) {
            throw ParseError(1, 1, "优先级声明引用未定义终结符: " + kv.first);
        }
        if (grammar.symbols[it->second].kind != SymbolKind::Terminal) {
            throw ParseError(1, 1, "优先级声明只能作用于终结符: " + kv.first);
        }
        grammar.precedence_by_symbol_id[it->second] = kv.second;
    }

    grammar.union_block_raw = def_result.union_block_raw;

    for (const auto& prod : grammar.productions) {
        grammar.prod_ids_by_lhs[prod.lhs_symbol_id].push_back(prod.id);
    }
}

}  // namespace

ParseError::ParseError(int line, int column, const std::string& message)
    : std::runtime_error(
          "ParseError at line " + std::to_string(line) + ", column " + std::to_string(column) +
          ": " + message),
      line_(line),
      column_(column) {}

Grammar parse_yacc_file(const std::string& path) {
    const std::string content = read_text_file(path);
    const SectionRanges sections = split_sections(content);

    DefinitionParseResult def_result;
    std::string start_symbol_name;
    parse_definitions(sections.definitions, sections.definitions_start_line, def_result, start_symbol_name);

    std::vector<ProductionDraft> drafts;
    std::unordered_set<std::string> lhs_names;
    parse_rules(sections.rules, sections.rules_start_line, drafts, lhs_names);

    Grammar grammar;
    grammar.source_path = path;
    grammar.user_subroutines_raw = sections.user_subroutines;
    finalize_grammar(grammar, def_result, lhs_names, drafts, start_symbol_name);
    return grammar;
}

}  // namespace seu::yacc
