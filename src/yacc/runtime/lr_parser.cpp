/**
 * 文件说明：实现第9步 LR 运行时解析器。
 * 包括 token 文件加载、EOF 补齐、移进/归约主循环执行，
 * 以及 trace 与错误信息的生成。
 */


#include "yacc/runtime/lr_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
#include <unistd.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace seu::yacc {
namespace {
namespace fs = std::filesystem;

// 函数说明：去掉字符串首尾空白字符并返回副本。
std::string trim_copy(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

// 函数说明：根据符号 ID 获取符号名，越界时返回占位文本。
std::string symbol_name_of(const Grammar& grammar, int symbol_id) {
    if (symbol_id < 0 || symbol_id >= static_cast<int>(grammar.symbols.size())) {
        return "<invalid>";
    }
    return grammar.symbols[symbol_id].name;
}

int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

std::string canonical_char_literal_for_token_file(const std::string& raw) {
    if (raw.size() < 3 || raw.front() != '\'' || raw.back() != '\'') {
        return raw;
    }
    // token_dumper may serialize non-printable char tokens as '\xHH'
    if (raw.size() == 6 && raw[1] == '\\' && raw[2] == 'x') {
        const int hi = hex_value(raw[3]);
        const int lo = hex_value(raw[4]);
        if (hi >= 0 && lo >= 0) {
            const unsigned char code = static_cast<unsigned char>((hi << 4) | lo);
            switch (code) {
                case '\n':
                    return "'\\n'";
                case '\t':
                    return "'\\t'";
                case '\r':
                    return "'\\r'";
                case '\v':
                    return "'\\v'";
                case '\f':
                    return "'\\f'";
                case '\b':
                    return "'\\b'";
                case '\a':
                    return "'\\a'";
                case '\\':
                    return "'\\\\'";
                case '\'':
                    return "'\\''";
                default:
                    break;
            }
            if (std::isprint(code) != 0) {
                return std::string("'") + static_cast<char>(code) + "'";
            }
        }
    }
    return raw;
}

// 函数说明：将状态栈序列格式化为可打印字符串。
std::string join_state_stack(const std::vector<int>& state_stack) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < state_stack.size(); ++i) {
        if (i != 0) {
            oss << " ";
        }
        oss << state_stack[i];
    }
    return oss.str();
}

// 函数说明：将符号栈序列格式化为可打印字符串。
std::string join_symbol_stack(const Grammar& grammar, const std::vector<int>& symbol_stack) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < symbol_stack.size(); ++i) {
        if (i != 0) {
            oss << " ";
        }
        oss << symbol_name_of(grammar, symbol_stack[i]);
    }
    return oss.str();
}

struct AstNode {
    int id = -1;
    std::string type;
    std::string lexeme;
    int production_id = -1;
    int line = 0;
    int column = 0;
    std::vector<int> children;
};

std::string escape_json(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

// 函数说明：确保 token 序列以 EOF 结尾，缺失时自动追加。
void ensure_eof_token(const Grammar& grammar, std::vector<RuntimeToken>& tokens) {
    if (!tokens.empty() && tokens.back().symbol_id == grammar.eof_symbol_id) {
        return;
    }
    RuntimeToken eof;
    eof.symbol_id = grammar.eof_symbol_id;
    eof.symbol_name = symbol_name_of(grammar, grammar.eof_symbol_id);
    eof.lexeme = "$";
    tokens.push_back(std::move(eof));
}

struct CompiledActionExecutor {
    std::string bin_path;
};

std::unordered_map<std::string, CompiledActionExecutor> g_compiled_action_cache;

std::string sanitize_id(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
            out.push_back(ch);
        } else {
            out.push_back('_');
        }
    }
    return out;
}

std::string escape_field(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char ch : s) {
        switch (ch) {
            case '\\':
                out += "\\\\";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

std::string extract_union_body(const std::string& union_block_raw) {
    if (union_block_raw.empty()) {
        return "";
    }
    const std::size_t l = union_block_raw.find('{');
    const std::size_t r = union_block_raw.rfind('}');
    if (l == std::string::npos || r == std::string::npos || r <= l) {
        return "";
    }
    return union_block_raw.substr(l + 1, r - l - 1);
}

std::string replace_action_placeholders(const Grammar& grammar, const Production& p, const std::string& raw) {
    std::string out;
    out.reserve(raw.size() * 2);
    const bool has_union = !extract_union_body(grammar.union_block_raw).empty();
    auto lhs_tag_it = grammar.symbol_type_tag_by_id.find(p.lhs_symbol_id);
    const std::string lhs_tag = (lhs_tag_it == grammar.symbol_type_tag_by_id.end()) ? "" : lhs_tag_it->second;
    auto rhs_tag_for_index = [&](int one_based) -> std::string {
        const int idx = one_based - 1;
        if (idx < 0 || idx >= static_cast<int>(p.rhs_symbol_ids.size())) {
            return "";
        }
        const int sid = p.rhs_symbol_ids[idx];
        auto it = grammar.symbol_type_tag_by_id.find(sid);
        if (it == grammar.symbol_type_tag_by_id.end()) {
            return "";
        }
        return it->second;
    };
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const char ch = raw[i];
        if (in_string) {
            out.push_back(ch);
            if (!escaped && ch == '\\') {
                escaped = true;
            } else if (!escaped && ch == '"') {
                in_string = false;
            } else {
                escaped = false;
            }
            continue;
        }
        if (in_char) {
            out.push_back(ch);
            if (!escaped && ch == '\\') {
                escaped = true;
            } else if (!escaped && ch == '\'') {
                in_char = false;
            } else {
                escaped = false;
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
        if (raw[i] == '$' && i + 1 < raw.size() && raw[i + 1] == '$') {
            if (!has_union) {
                out += "yy_lhs_val";
            } else {
                out += lhs_tag.empty() ? "yy_lhs_val" : ("yy_lhs_val." + lhs_tag);
            }
            ++i;
            continue;
        }
        if (raw[i] == '@' && i + 1 < raw.size() && raw[i + 1] == '$') {
            out += "yy_lhs_loc";
            ++i;
            continue;
        }
        if ((raw[i] == '$' || raw[i] == '@') && i + 1 < raw.size() &&
            std::isdigit(static_cast<unsigned char>(raw[i + 1])) != 0) {
            std::size_t j = i + 1;
            while (j < raw.size() && std::isdigit(static_cast<unsigned char>(raw[j])) != 0) {
                ++j;
            }
            const std::string num = raw.substr(i + 1, j - (i + 1));
            if (raw[i] == '$') {
                const int n = std::stoi(num);
                const std::string tag = rhs_tag_for_index(n);
                if (!has_union) {
                    out += "YY_RHS(" + num + ")";
                } else {
                    out += tag.empty() ? ("YY_RHS(" + num + ")") : ("YY_RHS(" + num + ")." + tag);
                }
            } else {
                out += "YY_RHS_LOC(" + num + ")";
            }
            i = j - 1;
            continue;
        }
        out.push_back(raw[i]);
    }
    return out;
}

std::uint64_t fnv1a64(const std::string& s) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

std::string build_compiled_action_source(const Grammar& grammar) {
    std::ostringstream out;
    out << "#include <cstdio>\n";
    out << "#include <cstdlib>\n";
    out << "#include <cstdint>\n";
    out << "#include <cctype>\n";
    out << "#include <cstring>\n";
    out << "#include <iostream>\n";
    out << "#include <sstream>\n";
    out << "#include <string>\n";
    out << "#include <vector>\n\n";
    out << "typedef struct YYLTYPE { int first_line; int first_column; int last_line; int last_column; } YYLTYPE;\n";
    const std::string union_body = extract_union_body(grammar.union_block_raw);
    if (!union_body.empty()) {
        out << "typedef union YYSTYPE {\n" << union_body << "\n} YYSTYPE;\n";
        out << "static YYSTYPE default_val(const std::string& lex){ YYSTYPE v{}; char* end=nullptr; double d=std::strtod(lex.c_str(), &end); "
               "if(end!=nullptr && *end=='\\0'){ std::memcpy(&v, &d, (sizeof(YYSTYPE)<sizeof(double)?sizeof(YYSTYPE):sizeof(double))); } return v; }\n\n";
    } else {
        out << "typedef int YYSTYPE;\n";
        out << "static YYSTYPE default_val(const std::string& lex){ char* end=nullptr; long long v=std::strtoll(lex.c_str(), &end, 10); "
               "if(end!=nullptr && *end=='\\0') return static_cast<int>(v); return 0; }\n\n";
    }
    out << "static std::string unesc(const std::string& s){ std::string o; bool e=false; "
           "for(char c:s){ if(!e&&c=='\\\\'){e=true;continue;} if(e){ if(c=='n')o.push_back('\\n'); "
           "else if(c=='t')o.push_back('\\t'); else if(c=='r')o.push_back('\\r'); else o.push_back(c); e=false; continue;} o.push_back(c);} return o; }\n";
    // Do not inject user subroutines directly here; they may redefine main/yyparse.
    out << "static int yy_action_execute(int production_id, int rhs_count, std::vector<YYSTYPE>& vstk, std::vector<YYLTYPE>& lstk) {\n";
    out << "  std::vector<YYSTYPE> yy_rhs_vals; std::vector<YYLTYPE> yy_rhs_locs;\n";
    out << "  for(int i=rhs_count;i>=1;--i){ yy_rhs_vals.push_back(vstk[vstk.size()-i]); yy_rhs_locs.push_back(lstk[lstk.size()-i]); }\n";
    out << "  for(int i=0;i<rhs_count;++i){ vstk.pop_back(); lstk.pop_back(); }\n";
    out << "  YYSTYPE yy_lhs_val{}; YYLTYPE yy_lhs_loc{};\n";
    out << "  if(rhs_count>0){ yy_lhs_val=yy_rhs_vals[0]; yy_lhs_loc=yy_rhs_locs[0]; yy_lhs_loc.last_line=yy_rhs_locs[rhs_count-1].last_line; yy_lhs_loc.last_column=yy_rhs_locs[rhs_count-1].last_column; }\n";
    out << "  #define YY_RHS(N) (yy_rhs_vals[(N)-1])\n";
    out << "  #define YY_RHS_LOC(N) (yy_rhs_locs[(N)-1])\n";
    out << "  #define YY_NUM(N) (YY_RHS(N))\n";
    out << "  switch (production_id) {\n";
    for (const auto& p : grammar.productions) {
        if (!p.action.present || p.action.raw_code.empty()) {
            continue;
        }
        out << "    case " << p.id << ": {\n";
        out << replace_action_placeholders(grammar, p, p.action.raw_code) << "\n";
        out << "      break;\n";
        out << "    }\n";
    }
    out << "    default:\n";
    out << "      break;\n";
    out << "  }\n";
    out << "  vstk.push_back(yy_lhs_val); lstk.push_back(yy_lhs_loc);\n";
    out << "  return 0;\n";
    out << "}\n";
    out << "int main() {\n";
    out << "  std::vector<YYSTYPE> vstk; std::vector<YYLTYPE> lstk; std::string line;\n";
    out << "  while (std::getline(std::cin, line)) {\n";
    out << "    if(line.empty()) continue; std::istringstream iss(line); char op=0; iss>>op;\n";
    out << "    if(op=='Q') break;\n";
    out << "    if(op=='S'){ std::string sym,lex; int fl=0,fc=0,ll=0,lc=0; iss>>sym>>lex>>fl>>fc>>ll>>lc; (void)sym; "
           "YYSTYPE v=default_val(unesc(lex)); YYLTYPE l{fl,fc,ll,lc}; vstk.push_back(v); lstk.push_back(l); continue; }\n";
    out << "    if(op=='R'){ int pid=0,rhs=0; iss>>pid>>rhs; if(rhs<0||rhs>(int)vstk.size()) continue; "
           "yy_action_execute(pid,rhs,vstk,lstk); continue; }\n";
    out << "  }\n";
    out << "  return 0;\n";
    out << "}\n";
    return out.str();
}

CompiledActionExecutor& get_or_build_compiled_action_executor(const Grammar& grammar) {
    std::string signature = "gen_v5\n" + grammar.source_path + "\n" + grammar.user_subroutines_raw;
    for (const auto& p : grammar.productions) {
        signature += "\n#" + std::to_string(p.id) + ":" + (p.action.present ? "1" : "0") + ":" + p.action.raw_code;
    }
    const std::uint64_t sig_hash = fnv1a64(signature);
    const std::string key = sanitize_id(grammar.source_path) + "_" + std::to_string(sig_hash);

    auto it = g_compiled_action_cache.find(key);
    if (it != g_compiled_action_cache.end()) {
        return it->second;
    }

    const std::string uniq = std::to_string(static_cast<long long>(::getpid()));
    const fs::path build_dir = fs::path("/tmp") / ("yacc_actions_" + key + "_" + uniq);
    fs::create_directories(build_dir);
    const fs::path cpp_path = build_dir / "actions.cpp";
    const fs::path bin_path = build_dir / "actions_runner";

    {
        std::ofstream out(cpp_path);
        if (!out.is_open()) {
            throw std::runtime_error("无法写入动作编译源码: " + cpp_path.string());
        }
        out << build_compiled_action_source(grammar);
    }

    std::ostringstream cmd;
    cmd << "g++ -std=c++17 -O2 "
        << cpp_path.string() << " -o " << bin_path.string()
        << " >/tmp/yacc_action_build_stdout.log 2>/tmp/yacc_action_build_stderr.log";
    const int rc = std::system(cmd.str().c_str());
    if (rc != 0) {
        throw std::runtime_error("编译语义动作失败（与 bison 一致，动作代码必须是可编译 C/C++ 片段）。"
                                 " 可查看 /tmp/yacc_action_build_stderr.log");
    }
    CompiledActionExecutor exec;
    exec.bin_path = bin_path.string();
    auto inserted = g_compiled_action_cache.emplace(key, std::move(exec));
    return inserted.first->second;
}

}  // namespace

std::vector<RuntimeToken> load_runtime_tokens_from_stream(
    const Grammar& grammar, std::istream& in, const std::string& source_name) {
    std::vector<RuntimeToken> tokens;
    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::istringstream iss(trimmed);
        RuntimeToken token;
        if (!(iss >> token.symbol_name)) {
            continue;
        }
        std::string second;
        std::string third;
        std::string fourth;
        std::string fifth;
        const bool has2 = static_cast<bool>(iss >> second);
        const bool has3 = has2 && static_cast<bool>(iss >> third);
        const bool has4 = has3 && static_cast<bool>(iss >> fourth);
        const bool has5 = has4 && static_cast<bool>(iss >> fifth);

        // 新协议 v2：token_name lexeme line col [token_id]
        // 旧协议兼容：token_name [lexeme [line [col]]]
        if (has4) {
            token.lexeme = second;
            token.line = std::stoi(third);
            token.column = std::stoi(fourth);
        } else if (has2) {
            token.lexeme = second;
        }

        token.symbol_name = canonical_char_literal_for_token_file(token.symbol_name);
        auto id_it = grammar.symbol_id_by_name.find(token.symbol_name);
        if (id_it == grammar.symbol_id_by_name.end()) {
            throw std::runtime_error(source_name + " 第 " + std::to_string(line_no) +
                                     " 行使用了未知符号: " + token.symbol_name);
        }
        token.symbol_id = id_it->second;
        const SymbolKind kind = grammar.symbols[token.symbol_id].kind;
        if (kind == SymbolKind::Nonterminal) {
            throw std::runtime_error(source_name + " 第 " + std::to_string(line_no) +
                                     " 行使用了非终结符，不能作为输入 token: " + token.symbol_name);
        }
        if (token.lexeme.empty()) {
            token.lexeme = token.symbol_name;
        }
        if (token.line <= 0) {
            token.line = line_no;
        }
        if (token.column <= 0) {
            token.column = 1;
        }
        if (has5) {
            // 可选 token_id 目前仅保留兼容位，不参与解析逻辑。
        }
        tokens.push_back(std::move(token));
    }

    ensure_eof_token(grammar, tokens);
    return tokens;
}

// 函数说明：从 tokens 文本文件读取运行时输入序列并完成基本校验。
std::vector<RuntimeToken> load_runtime_tokens_from_file(
    const Grammar& grammar, const std::string& token_file_path) {
    std::ifstream in(token_file_path);
    if (!in.is_open()) {
        throw std::runtime_error("无法打开 token 文件: " + token_file_path);
    }
    return load_runtime_tokens_from_stream(grammar, in, "token 文件");
}

// 函数说明：执行 LR 运行时移进/归约循环并生成 trace 与错误信息。
LRParseRunResult run_step9_lr_parse(const Grammar& grammar, const LR1Step8Result& step8_result,
    const std::vector<RuntimeToken>& input_tokens, int max_steps, bool execute_semantic_compiled_actions) {
    LRParseRunResult result;
    if (step8_result.action_table.empty() || step8_result.goto_table.empty()) {
        result.error.has_error = true;
        result.error.message = "第8步分析表为空，无法执行第9步解析。";
        return result;
    }
    if (input_tokens.empty()) {
        result.error.has_error = true;
        result.error.message = "输入 token 序列为空。";
        return result;
    }
    if (max_steps <= 0) {
        max_steps = 1;
    }

    std::vector<int> state_stack;
    std::vector<int> symbol_stack;
    std::vector<int> ast_stack;
    std::vector<AstNode> ast_nodes;
    auto add_ast_leaf = [&](const RuntimeToken& tk) {
        AstNode n;
        n.id = static_cast<int>(ast_nodes.size());
        n.type = tk.symbol_name;
        n.lexeme = tk.lexeme;
        n.production_id = -1;
        n.line = tk.line;
        n.column = tk.column;
        ast_nodes.push_back(std::move(n));
        return static_cast<int>(ast_nodes.size() - 1);
    };
    auto add_ast_reduce = [&](const Production& p, int pop_count, const std::vector<int>& child_ids) {
        AstNode n;
        n.id = static_cast<int>(ast_nodes.size());
        n.type = symbol_name_of(grammar, p.lhs_symbol_id);
        n.production_id = p.id;
        n.children = child_ids;
        if (pop_count > 0 && !child_ids.empty()) {
            n.line = ast_nodes[child_ids.front()].line;
            n.column = ast_nodes[child_ids.front()].column;
        }
        ast_nodes.push_back(std::move(n));
        return static_cast<int>(ast_nodes.size() - 1);
    };
    state_stack.push_back(0);
    CompiledActionExecutor* compiled_executor = nullptr;
    FILE* compiled_action_pipe = nullptr;
    if (execute_semantic_compiled_actions) {
        compiled_executor = &get_or_build_compiled_action_executor(grammar);
        compiled_action_pipe = popen(compiled_executor->bin_path.c_str(), "w");
        if (compiled_action_pipe == nullptr) {
            throw std::runtime_error("启动语义动作执行器失败: " + compiled_executor->bin_path);
        }
    }

    int input_index = 0;
    int step_no = 0;
    while (step_no < max_steps) {
        ++step_no;
        if (input_index < 0 || input_index >= static_cast<int>(input_tokens.size())) {
            result.error.has_error = true;
            result.error.step_no = step_no;
            result.error.input_index = input_index;
            result.error.message = "读取 lookahead 越界。";
            break;
        }
        if (state_stack.empty()) {
            result.error.has_error = true;
            result.error.step_no = step_no;
            result.error.input_index = input_index;
            result.error.message = "状态栈为空。";
            break;
        }

        const int state_id = state_stack.back();
        if (state_id < 0 || state_id >= static_cast<int>(step8_result.action_table.size())) {
            result.error.has_error = true;
            result.error.step_no = step_no;
            result.error.input_index = input_index;
            result.error.state_id = state_id;
            result.error.message = "当前状态越界。";
            break;
        }

        const RuntimeToken& lookahead = input_tokens[input_index];
        LRParseTraceRow row;
        row.step_no = step_no;
        row.state_id = state_id;
        row.lookahead_symbol_id = lookahead.symbol_id;
        row.lookahead_symbol_name = lookahead.symbol_name;
        row.state_stack_text = join_state_stack(state_stack);
        row.symbol_stack_text = join_symbol_stack(grammar, symbol_stack);
        row.input_index = input_index;

        const auto& action_row = step8_result.action_table[state_id];
        auto action_it = action_row.find(lookahead.symbol_id);
        if (action_it == action_row.end()) {
            result.error.has_error = true;
            result.error.step_no = step_no;
            result.error.input_index = input_index;
            result.error.state_id = state_id;
            result.error.lookahead_symbol_id = lookahead.symbol_id;
            result.error.lookahead_symbol_name = lookahead.symbol_name;
            result.error.message = "Action 表无可用动作，语法错误。";

            std::set<int> expected;
            for (const auto& kv : action_row) {
                expected.insert(kv.first);
            }
            result.error.expected_terminal_ids.assign(expected.begin(), expected.end());
            row.action_text = "error";
            result.trace_rows.push_back(std::move(row));
            break;
        }

        const ParseActionEntry& action = action_it->second;
        row.action_text = format_parse_action_entry(action);

        if (action.type == ParseActionType::Shift) {
            if (action.target_state_id < 0 ||
                action.target_state_id >= static_cast<int>(step8_result.action_table.size())) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = state_id;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "Shift 目标状态越界。";
                result.trace_rows.push_back(std::move(row));
                break;
            }
            symbol_stack.push_back(lookahead.symbol_id);
            state_stack.push_back(action.target_state_id);
            ast_stack.push_back(add_ast_leaf(lookahead));
            if (execute_semantic_compiled_actions && compiled_action_pipe != nullptr) {
                const std::string sym = escape_field(lookahead.symbol_name);
                const std::string lex = escape_field(lookahead.lexeme);
                const int fl = lookahead.line;
                const int fc = lookahead.column;
                const int ll = lookahead.line;
                const int lc = std::max(fc, fc + static_cast<int>(lookahead.lexeme.size()) - 1);
                std::fprintf(compiled_action_pipe, "S %s %s %d %d %d %d\n",
                    sym.c_str(), lex.c_str(), fl, fc, ll, lc);
                std::fflush(compiled_action_pipe);
            }
            ++input_index;
            result.trace_rows.push_back(std::move(row));
            continue;
        }

        if (action.type == ParseActionType::Reduce) {
            const int production_id = action.reduce_production_id;
            if (production_id <= 0 || production_id >= static_cast<int>(grammar.productions.size())) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = state_id;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "Reduce 产生式编号非法。";
                row.production_id = production_id;
                result.trace_rows.push_back(std::move(row));
                break;
            }

            const Production& production = grammar.productions[production_id];
            const int pop_count = static_cast<int>(production.rhs_symbol_ids.size());
            if (static_cast<int>(state_stack.size()) < pop_count + 1 ||
                static_cast<int>(symbol_stack.size()) < pop_count) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = state_id;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "栈深不足，无法执行归约。";
                row.production_id = production_id;
                result.trace_rows.push_back(std::move(row));
                break;
            }

            for (int i = 0; i < pop_count; ++i) {
                state_stack.pop_back();
                symbol_stack.pop_back();
            }
            std::vector<int> popped_children;
            for (int i = 0; i < pop_count; ++i) {
                if (!ast_stack.empty()) {
                    popped_children.push_back(ast_stack.back());
                    ast_stack.pop_back();
                }
            }
            std::reverse(popped_children.begin(), popped_children.end());
            const int goto_from_state = state_stack.back();
            if (goto_from_state < 0 || goto_from_state >= static_cast<int>(step8_result.goto_table.size())) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = goto_from_state;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "Goto 起点状态越界。";
                row.production_id = production_id;
                result.trace_rows.push_back(std::move(row));
                break;
            }
            const auto& goto_row = step8_result.goto_table[goto_from_state];
            auto goto_it = goto_row.find(production.lhs_symbol_id);
            if (goto_it == goto_row.end()) {
                result.error.has_error = true;
                result.error.step_no = step_no;
                result.error.input_index = input_index;
                result.error.state_id = goto_from_state;
                result.error.lookahead_symbol_id = lookahead.symbol_id;
                result.error.lookahead_symbol_name = lookahead.symbol_name;
                result.error.message = "归约后 Goto 缺失。";
                row.production_id = production_id;
                result.trace_rows.push_back(std::move(row));
                break;
            }

            symbol_stack.push_back(production.lhs_symbol_id);
            state_stack.push_back(goto_it->second);
            ast_stack.push_back(add_ast_reduce(production, pop_count, popped_children));
            row.production_id = production_id;
            result.reduction_production_ids.push_back(production_id);
            if (execute_semantic_compiled_actions && compiled_action_pipe != nullptr) {
                std::fprintf(compiled_action_pipe, "R %d %d\n", production_id, pop_count);
                std::fflush(compiled_action_pipe);
            }
            result.trace_rows.push_back(std::move(row));
            continue;
        }

        // accept
        result.accepted = true;
        result.trace_rows.push_back(std::move(row));
        break;
    }

    if (!result.accepted && !result.error.has_error && step_no >= max_steps) {
        result.error.has_error = true;
        result.error.step_no = step_no;
        result.error.input_index = input_index;
        result.error.message = "达到最大步骤限制，已中止。";
    }

    result.total_steps = step_no;
    result.consumed_tokens = std::max(0, input_index);
    if (result.accepted && !ast_stack.empty()) {
        const int root = ast_stack.back();
        std::ostringstream js;
        js << "{\n  \"root\": " << root << ",\n  \"nodes\": [\n";
        for (std::size_t i = 0; i < ast_nodes.size(); ++i) {
            const auto& n = ast_nodes[i];
            js << "    {\"id\":" << n.id << ",\"type\":\"" << escape_json(n.type) << "\",\"lexeme\":\""
               << escape_json(n.lexeme) << "\",\"production_id\":" << n.production_id << ",\"line\":" << n.line
               << ",\"column\":" << n.column << ",\"children\":[";
            for (std::size_t j = 0; j < n.children.size(); ++j) {
                if (j) js << ",";
                js << n.children[j];
            }
            js << "]}";
            if (i + 1 != ast_nodes.size()) js << ",";
            js << "\n";
        }
        js << "  ]\n}\n";
        result.ast_json = js.str();

        std::ostringstream txt;
        std::function<void(int, int)> dfs = [&](int id, int d) {
            if (id < 0 || id >= static_cast<int>(ast_nodes.size())) return;
            const auto& n = ast_nodes[id];
            txt << std::string(static_cast<std::size_t>(d) * 2, ' ') << n.type;
            if (n.production_id >= 0) txt << " [p#" << n.production_id << "]";
            if (!n.lexeme.empty()) txt << " \"" << n.lexeme << "\"";
            txt << "\n";
            for (int c : n.children) dfs(c, d + 1);
        };
        dfs(root, 0);
        result.ast_text = txt.str();
    }
    if (compiled_action_pipe != nullptr) {
        const int child_rc = pclose(compiled_action_pipe);
        compiled_action_pipe = nullptr;
        if (child_rc != 0) {
            throw std::runtime_error("语义动作执行器返回非零退出码: " + std::to_string(child_rc));
        }
    }
    return result;
}

}  // namespace seu::yacc
