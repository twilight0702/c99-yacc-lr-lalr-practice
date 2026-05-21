/*
 * lr_parser.h — LR 解析器运行时
 *
 * 第 9 步核心模块：基于构造好的 Action/Goto 分析表，对输入 token 序列
 * 执行 LR 解析。实现了标准 LR 总控程序（driver）的查表移进-归约循环。
 *
 * 核心算法：
 *   1. 初始化状态栈（仅含 0）、符号栈为空、输入指针指向第一个 token
 *   2. 取栈顶状态 s 和当前前瞻 token a
 *   3. 查 Action[s][a]：
 *      - Shift t:     将 a 和 t 压栈，输入指针前进
 *      - Reduce A→β:  弹出 |β| 对（状态+符号），查 Goto[新栈顶][A]，
 *                      将 A 和新状态压栈
 *      - Accept:       解析成功
 *      - (空):         解析错误
 *   4. 重复直到接受或出错或达到最大步数限制
 *
 * 同时支持输入 token 文件的加载和解析过程的 trace 记录。
 */

#pragma once

#include <string>
#include <vector>

#include "yacc/model/grammar.h"
#include "yacc/table/parse_table.h"

namespace seu::yacc {

/*
 * RuntimeToken — 第 9 步输入 token
 *
 * symbol_id:   符号在文法中的 ID（-1 表示未识别/未知）
 * symbol_name: 符号名称（便于诊断和导出）
 * lexeme:      原始词素文本
 * line:        词素在源文件中的行号
 * column:      词素在源文件中的列号
 */
struct RuntimeToken {
    int symbol_id = -1;
    std::string symbol_name;
    std::string lexeme;
    int line = 0;
    int column = 0;
};

/*
 * LRParseTraceRow — 解析过程的单步执行快照
 *
 * step_no:                当前步骤序号（从 1 开始）
 * state_id:               当前栈顶状态
 * lookahead_symbol_id:    当前前瞻符号
 * lookahead_symbol_name:  前瞻符号名称
 * action_text:            执行动作的文本描述（如 "s23"、"r45"、"acc"）
 * production_id:          Reduce 时使用的产生式 ID（非 Reduce 时为 -1）
 * state_stack_text:       当前状态栈的文本表示
 * symbol_stack_text:      当前符号栈的文本表示
 * input_index:            当前输入流的读取位置
 */
struct LRParseTraceRow {
    int step_no = 0;
    int state_id = -1;
    int lookahead_symbol_id = -1;
    std::string lookahead_symbol_name;
    std::string action_text;
    int production_id = -1;
    std::string state_stack_text;
    std::string symbol_stack_text;
    int input_index = 0;
};

/*
 * LRParseErrorInfo — 解析错误详情
 *
 * has_error:                是否发生错误
 * step_no:                  错误发生的步骤号
 * input_index:              错误发生时输入流的位置
 * state_id:                 错误发生时的栈顶状态
 * lookahead_symbol_id:      触发错误的前瞻符号
 * lookahead_symbol_name:    前瞻符号名称
 * message:                  错误描述信息
 * expected_terminal_ids:    当前状态下可接受的所有终结符集合（用于错误恢复提示）
 */
struct LRParseErrorInfo {
    bool has_error = false;
    int step_no = 0;
    int input_index = 0;
    int state_id = -1;
    int lookahead_symbol_id = -1;
    std::string lookahead_symbol_name;
    std::string message;
    std::vector<int> expected_terminal_ids;
};

/*
 * LRParseRunResult — 第 9 步解析运行结果
 *
 * accepted:                   是否成功接受输入
 * consumed_tokens:            成功消费的 token 数量（不计自动补的 EOF）
 * total_steps:                执行的总步数
 * reduction_production_ids:   归约产生的产生式 ID 序列（对应语法树构造顺序）
 * trace_rows:                 完整的步骤 trace 记录
 * error:                      错误信息（若未发生错误则 has_error == false）
 */
struct LRParseRunResult {
    bool accepted = false;
    int consumed_tokens = 0;
    int total_steps = 0;
    std::vector<int> reduction_production_ids;
    std::vector<LRParseTraceRow> trace_rows;
    LRParseErrorInfo error;
};

/*
 * load_runtime_tokens_from_file — 从文本文件加载 token 序列
 *
 * 文件格式（每行一个 token，支持以下三种格式）：
 *   TOKEN                       仅 token 名（词素默认为 token 名）
 *   TOKEN LEXEME                token 名 + 词素
 *   TOKEN LEXEME LINE COLUMN    token 名 + 词素 + 位置信息
 *
 * 支持以 # 开头的注释行和空行。若输入 token 序列末尾不含 EOF($)，
 * 自动追加一个 $ token。
 *
 * 参数:
 *   grammar:         文法结构（用于 token 名到 ID 的映射）
 *   token_file_path: token 文件路径
 *
 * 返回:
 *   解析后的 token 序列向量。
 *
 * 抛出:
 *   若 token 名无法识别，抛出 ParseError 异常。
 */
std::vector<RuntimeToken> load_runtime_tokens_from_file(
    const Grammar& grammar, const std::string& token_file_path);

/*
 * run_step9_lr_parse — 运行 LR(1) 总控程序
 *
 * 执行标准查表移进-归约循环，直到：
 *   1. 遇到 Accept 动作（解析成功）
 *   2. 遇到空动作/冲突（解析错误）
 *   3. 达到 max_steps 上限（防止死循环）
 *
 * 参数:
 *   grammar:      文法结构
 *   step8_result: 包含 Action/Goto 表的第 8 步结果
 *   input_tokens: 待解析的输入 token 序列
 *   max_steps:    最大执行步数限制
 *
 * 返回:
 *   包含接受状态、归约序列和 trace 记录的运行结果。
 */
LRParseRunResult run_step9_lr_parse(const Grammar& grammar, const LR1Step8Result& step8_result,
    const std::vector<RuntimeToken>& input_tokens, int max_steps);

}  // namespace seu::yacc
