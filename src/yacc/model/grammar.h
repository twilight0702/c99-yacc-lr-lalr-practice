/*
 * grammar.h — 文法数据结构定义
 *
 * 本文件定义了 YACC 文法在内存中的核心数据结构，包括：
 *   - Symbol:     文法符号（终结符、非终结符、特殊符号）
 *   - Production: 产生式（左部 → 右部符号列表 + 语义动作）
 *   - Grammar:    完整文法描述，包含符号表、产生式表、开始符号等
 *
 * 这些结构是所有后续步骤（First 集、LR(1) 项目集规范族、分析表构造等）
 * 的基础数据载体，贯穿整个 YACC 后端流水线。
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace seu::yacc {

/*
 * SymbolKind — 文法符号的类别枚举
 * Terminal:    终结符（包括 token 和字面字符）
 * Nonterminal: 非终结符（产生式左部）
 * Special:     特殊符号（如 $ 表示文件结束、epsilon 表示空串、S' 表示增广开始符号）
 */
enum class SymbolKind {
    Terminal,
    Nonterminal,
    Special
};

/*
 * ActionBlock — 语义动作占位符
 * 第 3 步（解析阶段）只保存原始动作代码文本，不解析也不执行。
 * present:  产生式是否包含 { } 中的语义动作
 * raw_code: 原始的动作代码字符串
 */
struct ActionBlock {
    bool present = false;
    std::string raw_code;
};

/*
 * Symbol — 文法符号的完整描述
 * id:                符号在 symbols 向量中的唯一索引
 * name:              符号的显示名称（如 "+"、"expression"、"$"）
 * kind:              符号类别（Terminal / Nonterminal / Special）
 * is_literal_char:   是否为文法中直接写的字符常量（如 '+'、'-'），区别于命名的 token
 */
struct Symbol {
    int id = -1;
    std::string name;
    SymbolKind kind = SymbolKind::Special;
    bool is_literal_char = false;
};

/*
 * Production — 产生式的完整描述
 * id:              产生式在 productions 向量中的唯一索引
 * lhs_symbol_id:   产生式左部（被定义的）非终结符 ID
 * rhs_symbol_ids:  产生式右部符号 ID 序列（可能为空，表示 epsilon）
 * action:          该产生式的语义动作代码
 * source_line:     在原始 YACC 文件中所在的源行号（用于错误定位）
 */
struct Production {
    int id = -1;
    int lhs_symbol_id = -1;
    std::vector<int> rhs_symbol_ids;
    ActionBlock action;
    int source_line = 0;
};

/*
 * Grammar — 文法的完整描述
 *
 * source_path:               原始 YACC 输入文件路径
 * start_symbol_id:           用户指定的开始符号 ID
 * augmented_start_symbol_id: 增广文法的开始符号 S' 的 ID
 * eof_symbol_id:             文件结束符 $ 的符号 ID
 * epsilon_symbol_id:         空串 ε 的符号 ID
 *
 * symbols:          所有符号的列表（按 ID 索引）
 * terminal_ids:     所有终结符的 ID 列表
 * nonterminal_ids:  所有非终结符的 ID 列表
 * productions:      所有产生式的列表（按 ID 索引）
 *
 * symbol_id_by_name:  从符号名称到符号 ID 的快速查找表
 * prod_ids_by_lhs:    从非终结符 ID 到其所有产生式 ID 列表的索引
 *
 * user_subroutines_raw: 用户自定义子例程的原始代码文本
 */
struct Grammar {
    std::string source_path;

    int start_symbol_id = -1;
    int augmented_start_symbol_id = -1;
    int eof_symbol_id = -1;
    int epsilon_symbol_id = -1;

    std::vector<Symbol> symbols;
    std::vector<int> terminal_ids;
    std::vector<int> nonterminal_ids;
    std::vector<Production> productions;

    std::unordered_map<std::string, int> symbol_id_by_name;
    std::unordered_map<int, std::vector<int>> prod_ids_by_lhs;

    std::string user_subroutines_raw;
};

/* 常用特殊符号的内部名称常量 */
inline constexpr const char* kEofSymbolName = "$";              /* 文件结束符 */
inline constexpr const char* kEpsilonSymbolName = "epsilon";    /* 空串 ε */
inline constexpr const char* kAugmentedStartName = "S'";        /* 增广开始符号 */

}  // namespace seu::yacc
