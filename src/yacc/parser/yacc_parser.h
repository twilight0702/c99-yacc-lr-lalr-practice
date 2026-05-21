/*
 * yacc_parser.h — YACC 输入文件解析器
 *
 * 第 3 步核心模块：读取 .y 格式的 YACC 输入文件，将其解析为内存中的
 * Grammar 数据结构。处理的语法内容包括：
 *   - %token / %union / %type 等声明区
 *   - %% ... %% 之间的文法规则（产生式 + 语义动作）
 *   - 第二个 %% 之后的用户自定义代码区
 *
 * 同时定义了解析错误类型 ParseError，携带行列信息方便定位输入文件中
 * 的语法问题。
 */

#pragma once

#include <stdexcept>
#include <string>

#include "yacc/model/grammar.h"

namespace seu::yacc {

/*
 * ParseError — YACC 解析阶段异常
 *
 * 继承自 std::runtime_error，额外携带行列信息。
 * 在解析 .y 文件遇到语法错误时抛出，上层可捕获并格式化输出。
 */
class ParseError : public std::runtime_error {
public:
    /*
     * 构造函数
     * line:    错误所在行号
     * column:  错误所在列号
     * message: 错误描述信息
     */
    ParseError(int line, int column, const std::string& message);

    /* 返回错误行号 */
    int line() const noexcept { return line_; }
    /* 返回错误列号 */
    int column() const noexcept { return column_; }

private:
    int line_;
    int column_;
};

/*
 * parse_yacc_file — 解析 YACC 输入文件
 *
 * 参数:
 *   path: .y 文件的路径
 *
 * 返回:
 *   解析完成的 Grammar 结构，其中包含所有符号、产生式、动作代码等。
 *
 * 抛出:
 *   若文件无法打开或语法错误，抛出 ParseError。
 */
Grammar parse_yacc_file(const std::string& path);

}  // namespace seu::yacc
