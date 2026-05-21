# YACC 部分详细任务拆解与实施步骤

## 1. 文档目的

本文根据以下三份材料整理：

- `c99.y`：当前作为 Yacc 输入定义的 C99 语法文件。
- `编译原理专题实践：全周期详细进度计划.md`：已有的粗粒度进度安排。
- `编译原理课程实践 2026.pptx`：课程要求、参考进度、提交物和答辩要求的最完整说明。

目标是把 **YACC 部分从“要做什么”拆成“按什么顺序做、每一步做到什么程度才算完成”**，形成可以直接执行和汇报的工作文档。

---

## 2. YACC 部分的总目标

YACC 部分不是只“把 `c99.y` 读进来”，而是要完整实现一套语法分析程序生成器（SeuYacc）的核心链路：

1. 解析 Yacc 输入文件。
2. 建立文法、产生式、终结符、非终结符等内部表示。
3. 计算 First 集。
4. 构造 LR(1) 项目、闭包、Goto/状态转移图。
5. 由 LR(1) 自动机构造 Action/Goto 分析表。
6. 实现 LR 总控程序（查表驱动的移进-归约分析器）。
7. 在需要时将 LR(1) 状态合并成 LALR(1)。
8. 与词法分析器对接，输出规约过程、产生式序列、语法树或语义动作触发点。
9. 完成测试、文档、报告与答辩材料。

---

## 3. 结合 `c99.y` 得出的工作范围

当前 `c99.y` 已经提供了一个较完整的 C99 文法骨架，包含：

- `%token` 声明
- `%start translation_unit`
- 大量产生式规则
- 尾部用户代码区（如 `yyerror`）

但从课程要求看，YACC 部分至少还需要你自己完成以下“工具链能力”：

- 识别并解析 Yacc 输入文件的各个 section。
- 把文法规则转成内部数据结构，而不是手写死在程序里。
- 自动求 First 集、构造 LR(1) 项目集规范族、构造分析表。
- 编写自己的 LR/LALR 总控程序，而不是直接调用现成 bison/yacc。
- 为后续语义分析/AST 构造预留动作位置或产生式序列输出能力。

也就是说，**`c99.y` 是输入，不是最终成果**。

---

## 4. 推荐的分步实施方案

下面按“先设计输入解析，再做文法算法，再做分析器执行，再做联调与文档”的顺序拆分。

### 第 1 步：明确 Yacc 输入范围与最终支持子集

**要做什么**

- 通读 `c99.y`，确认本项目的输入格式至少要支持：
  - Definitions 区
  - Rules 区
  - User subroutines 区
- 明确第一版是否完整支持 `c99.y` 中出现的所有写法。
- 明确是否暂时不支持复杂语义动作，只先支持“空动作/占位动作/动作字符串原样保存”。

**做到什么程度算完成**

- 给出一份“Yacc 输入格式说明”。
- 写清楚：
  - 支持哪些指令，如 `%token`、`%start`
  - 规则部分如何表示
  - 动作是否执行、是否仅保存
  - 暂不支持哪些高级特性
- 能明确回答“当前版本能不能正确读取 `c99.y`”。

**产出**

- 输入格式说明文档
- 功能边界清单

---

### 第 2 步：设计 Yacc 输入文件的内部数据结构

**要做什么**

- 设计用于表示以下信息的数据结构：
  - 终结符表
  - 非终结符表
  - 产生式表
  - 开始符号
  - 文法符号编号映射
  - 产生式右部符号序列
  - 动作代码或动作占位信息
- 设计解析完成后的统一文法对象 `Grammar`。

**建议至少包含的字段**

- `Symbol`
  - 名字
  - 编号
  - 类型（终结符/非终结符/特殊符号）
- `Production`
  - 产生式编号
  - 左部非终结符
  - 右部符号列表
  - 动作信息
- `Grammar`
  - token 集
  - nonterminal 集
  - start symbol
  - productions
  - augmented start production

**做到什么程度算完成**

- 所有 Yacc 处理阶段都能复用这套结构。
- 能支持 `c99.y` 中几十到上百条产生式的存储。
- 能方便后续 First、Closure、Goto、Action/Goto 表构造。

**产出**

- 数据结构设计说明
- 关键类图/结构图
- 伪代码

---

### 第 3 步：实现 Yacc 输入文件解析器

**要做什么**

- 按 `%%` 分割 Yacc 文件。
- 解析 Definitions 区：
  - `%token`
  - `%start`
  - 其他需要支持的声明
- 解析 Rules 区：
  - 识别非终结符左部
  - 识别 `:`、`|`、`;`
  - 拆分多个候选式
  - 处理右部符号序列
  - 处理空产生式
  - 保存语义动作或动作占位
- User subroutines 区可以先原样保存，不参与分析表生成。

**做到什么程度算完成**

- 对 `c99.y` 可以稳定解析，得到完整的 productions 列表。
- 能打印或导出：
  - token 列表
  - 非终结符列表
  - 产生式编号及内容
- 遇到格式错误时，能报告行号、错误类型、附近文本。

**最低验收标准**

- `c99.y` 中所有规则均被正确读入。
- 不出现漏规则、串规则、左右部分错位。

**产出**

- Yacc 输入解析模块源码
- 文法导出结果
- 错误处理说明

---

### 第 4 步：文法预处理与增广文法构造

**要做什么**

- 自动补充增广开始符号，例如 `S' -> translation_unit`。
- 区分：
  - 普通终结符
  - 非终结符
  - 输入结束符 `$`
  - epsilon（如果内部需要）
- 预处理产生式索引，建立“某非终结符对应哪些产生式”的快速访问结构。

**做到什么程度算完成**

- 后续 Closure/Goto 过程不需要重复全表扫描。
- 增广文法唯一、稳定、可打印。

**产出**

- 增广文法
- 非终结符到产生式列表的索引表

---

### 第 5 步：实现 First 集计算

**要做什么**

- 依据文法结构计算所有非终结符的 First 集。
- 若项目设计需要，也可以一并支持符号串的 First 集计算。
- 正确处理：
  - 终结符 First
  - 非终结符递推
  - epsilon 传播

**做到什么程度算完成**

- 对每个非终结符都能输出稳定正确的 First 集。
- 能支撑 LR(1) 闭包中的 lookahead 计算。
- 多次运行结果一致。

**最低验收标准**

- 对若干典型非终结符做人工校验。
- 有单元测试或样例验证 First 集结果。

**产出**

- First 集算法伪代码
- First 集输出样例
- 单元测试/验证记录

**当前实现状态（已完成）**

- 已新增模块：
  - `src/yacc/first/first_set.h`
  - `src/yacc/first/first_set.cpp`
- 已实现能力：
  - `compute_first_sets(const Grammar&)`：计算全部符号的 First 集，支持 epsilon 传播与不动点收敛。
  - `compute_first_of_sequence(...)`：计算符号串 First(β)，可直接用于后续 LR(1) 闭包 lookahead 传播。
  - `validate_first_sets(...)`：执行 First 集不变量校验（终结符 First、自空产生式传播、首符号传播）。
- 已接入命令行主流程：`src/tools/yacc_parse_main.cpp`
  - 在第 4 步后自动执行第 5 步。
  - 校验失败返回码 `5`。
- 已接入结构化导出：
  - 默认目录：`artifacts/yacc/step5/<input_stem>/`
  - 导出文件包含：
    - `summary.txt`
    - `raw/first_sets.tsv`
    - `raw/first_sequence_examples.txt`
    - `analysis/report.txt`
    - 以及 step4 继承的 `symbols/productions/augmented/index/user_subroutines`

**第 5 步完成判定**

- 对 `c99.y` 运行可得到稳定 First 集输出；
- `analysis/report.txt` 中 `first_converged=true` 且 `first_validation_passed=true`；
- 可用 `raw/first_sequence_examples.txt` 快速核对符号串 First 结果。

---

### 第 6 步：实现 LR(1) 项及闭包算法

**要做什么**

- 设计 LR(1) 项结构：
  - 产生式编号
  - 点的位置
  - 展望符 lookahead
- 实现闭包 `closure(I)`：
  - 根据点后非终结符扩展新项目
  - 计算预测符
  - 去重
- 实现 `goto(I, X)`。

**这是 PPT 中明确强调的关键点**

- item 的扩展
- 预测符的计算
- Item 闭包构造

**做到什么程度算完成**

- 能输出一个状态中的全部 LR(1) 项。
- 对起始状态 `I0` 的闭包结果可视化或文本打印。
- Closure/Goto 结果可重复、无遗漏、无重复爆炸。

**最低验收标准**

- 至少对小文法和 `c99.y` 子集都能正确生成项目集。
- 能解释任一 lookahead 的来源。

**产出**

- LR(1) 项结构设计
- Closure/Goto 算法实现
- 样例状态输出

**当前实现状态（已完成）**

- 已新增模块：`src/yacc/lr1/lr1_items.h`、`src/yacc/lr1/lr1_items.cpp`
  - `LR1Item`：`production_id`、`dot_pos`、`lookahead_symbol_id`
  - `build_step6_lr1_items(...)`：构造 `I0 kernel`、`closure(I0)`、`goto(I0, X)`
  - `validate_step6_lr1_items(...)`：项合法性、闭包完备性、去重一致性校验
  - `format_lr1_item(...)`：可读化输出
- 已接入命令行主流程：`src/tools/yacc_parse_main.cpp`
  - 在第 5 步后自动执行第 6 步
  - 校验失败返回码 `6`
  - 终端输出 `I0` 项数、`goto` 边数、lookahead 来源说明条目
- 已接入结构化导出：
  - 默认目录：`artifacts/yacc/step6/<input_stem>/`
  - 新增导出文件：
    - `raw/lr1_i0_items.txt`（`I0 kernel + closure(I0)`）
    - `raw/lr1_i0_goto.tsv`（`goto(I0, X)` 边摘要）
    - `raw/lr1_i0_goto_items.txt`（每条 `goto` 目标项集）
    - `raw/lr1_lookahead_derivation.txt`（lookahead 来源说明）
  - 保留 step5 继承文件（`first_sets`、`productions` 等）

**第 6 步完成判定**

- 对 `c99.y` 运行可稳定生成 `I0` 闭包与 `goto(I0, X)`；
- `analysis/report.txt` 中 `lr1_step6_validation_passed=true`；
- 可通过 `raw/lr1_lookahead_derivation.txt` 追溯 lookahead 来源；
- 多次运行输出结构一致、无重复项和缺失项错误。

---

### 第 7 步：构造 LR(1) 项目集规范族与状态转移图

**要做什么**

- 从初始项目集 `I0` 出发，按所有可转移符号反复做 `goto`。
- 建立状态编号。
- 去重判等，形成完整的 LR(1) 项目集规范族。
- 记录状态转移边：
  - `state --symbol--> state`

**做到什么程度算完成**

- 能生成全部 LR(1) 状态。
- 能打印状态图或边列表。
- 对每个状态知道它由哪个状态、经哪个符号转移而来。

**最低验收标准**

- 小文法测试通过。
- `c99.y` 或其子集可以生成完整状态集合，程序可在可接受时间内完成。

**产出**

- 状态集列表
- 状态转移图
- 状态去重策略说明

**当前实现状态（已完成）**

- 已新增第 7 步能力（基于第 6 步公共 closure/goto 算法扩展）：
  - `src/yacc/lr1/lr1_items.h`
  - `src/yacc/lr1/lr1_items.cpp`
  - `LR1State`、`LR1Transition`、`LR1Step7Result`、`LR1Step7ValidationReport`
  - `build_step7_lr1_canonical_collection(...)`：从 `I0` 出发构造完整 LR(1) 项目集规范族
  - `validate_step7_lr1_canonical_collection(...)`：状态合法性、去重一致性、`goto` 与边一致性校验
- 已接入命令行主流程：`src/tools/yacc_parse_main.cpp`
  - 在第 6 步后自动执行第 7 步
  - 第 7 步校验失败返回码 `7`
  - 终端输出状态总数、转移边总数、前若干条状态转移边
- 已接入结构化导出：
  - 默认目录：`artifacts/yacc/step7/<input_stem>/`
  - 导出文件新增：
    - `raw/lr1_states.tsv`（状态编号与项数）
    - `raw/lr1_state_items.txt`（每个状态完整 LR(1) 项）
    - `raw/lr1_transitions.tsv`（`from --symbol--> to` 边列表）
    - `raw/lr1_predecessors.tsv`（每个状态的来源边摘要）
  - 保留 step6 及其之前的继承文件（`first_sets`、`lr1_i0_items` 等）

**第 7 步完成判定**

- 对 `c99.y` 运行可稳定生成完整 LR(1) 状态集合与转移边；
- `analysis/report.txt` 中 `lr1_step7_validation_passed=true`；
- 可通过 `raw/lr1_transitions.tsv` 与 `raw/lr1_predecessors.tsv` 追踪任意状态来源；
- 多次运行状态数与边数一致，且无重复状态或非法边错误。

---

### 第 8 步：构造 Action / Goto 分析表

**要做什么**

- 根据 LR(1) 状态和项目构造：
  - `Action[state, terminal]`
  - `Goto[state, nonterminal]`
- 处理 4 类动作：
  - shift
  - reduce
  - accept
  - error
- 建立冲突检测机制：
  - shift/reduce
  - reduce/reduce

**做到什么程度算完成**

- 表项可导出、可查询。
- 出现冲突时能定位到：
  - 状态号
  - 输入符号
  - 涉及项目
- 对无冲突文法能稳定生成完整表。

**最低验收标准**

- 至少对课程样例文法表构造正确。
- 对 `c99.y` 若出现冲突，能够输出冲突信息而不是直接崩溃。

**产出**

- Action/Goto 表
- 冲突报告
- 表构造算法说明

**当前实现状态（已完成）**

- 已新增模块：
  - `src/yacc/table/parse_table.h`
  - `src/yacc/table/parse_table.cpp`
- 已实现能力：
  - `build_step8_lr1_parsing_table(...)`：基于第 7 步 LR(1) 规范族构造 `Action[state, terminal]` 与 `Goto[state, nonterminal]`。
  - `format_parse_action_entry(...)`：统一渲染 `sN / rK / acc` 动作文本。
  - `validate_step8_lr1_parsing_table(...)`：校验表项合法性、与状态转移图一致性、与归约项一致性，并输出冲突告警。
  - 冲突消解策略（确定性）：
    - `shift/reduce`：优先 `shift`（与经典 Yacc/Bison 默认一致，可正确处理 dangling-else）。
    - `reduce/reduce`：优先产生式编号更小者（保证结果稳定可复现）。
    - `accept/*`：优先 `accept`（仅应在 `$` 列出现，异常场景也可稳定处理）。
  - 冲突检测已覆盖：
    - `shift/reduce`
    - `reduce/reduce`
    - 其他异常动作组合（`accept/*`、`shift/shift`）也会记录。
- 已接入命令行主流程：`src/tools/yacc_parse_main.cpp`
  - 在第 7 步后自动执行第 8 步。
  - 第 8 步校验失败返回码 `8`。
  - 终端输出 Action 表项数、Goto 表项数、冲突数。
- 已接入结构化导出：
  - 默认目录：`artifacts/yacc/step8/<input_stem>/`
  - 新增导出文件：
    - `raw/action_table.tsv`
    - `raw/goto_table.tsv`
    - `raw/parse_table_conflicts.tsv`
    - `raw/parse_table_conflict_resolution.tsv`（每条冲突的最终选取动作与原因）
  - 并保留 step7 及之前的继承文件（`lr1_states`、`lr1_transitions`、`first_sets` 等）。

**第 8 步完成判定**

- 对 `c99.y` 可稳定生成 Action/Goto 表；
- `analysis/report.txt` 中 `lr1_step8_validation_passed=true`；
- 若存在冲突，可在 `raw/parse_table_conflicts.tsv` 精确定位状态号、符号和冲突类型；
- 冲突的最终处理策略可在 `raw/parse_table_conflict_resolution.tsv` 完整追踪；
- 无冲突时该文件仅保留表头（或空记录），流程不崩溃。

---

### 第 9 步：实现 LR 总控程序（移进-归约分析器）

**要做什么**

- 编写查表驱动的语法分析主循环：
  - 状态栈
  - 符号栈
  - 输入 token 流
- 按 Action/Goto 表执行：
  - 移进
  - 归约
  - 接受
  - 报错
- 归约时输出：
  - 归约使用的产生式编号
  - 或句柄序列
  - 或语法树构造动作入口

**做到什么程度算完成**

- 能接受 Lex 提供的 token 序列并完成语法分析。
- 能输出完整的移进/归约日志。
- 能在错误输入下给出尽量明确的报错位置。

**最低验收标准**

- 对若干合法输入成功 accept。
- 对若干非法输入成功报错。
- 能把规约序列传给后续语义模块，或至少能打印出来。

**产出**

- Parser 总控程序
- 规约日志
- 报错示例

**当前实现状态（已完成）**

- 已新增模块：
  - `src/yacc/runtime/lr_parser.h`
  - `src/yacc/runtime/lr_parser.cpp`
- 已实现能力：
  - `load_runtime_tokens_from_file(...)`：读取 token 文件，支持注释/空行，支持 `TOKEN`、`TOKEN LEXEME`、`TOKEN LEXEME LINE COLUMN` 三种格式，末尾自动补 `$`。
  - `run_step9_lr_parse(...)`：查表驱动移进-归约主循环，维护状态栈/符号栈，执行 `shift / reduce / accept / error`。
  - 错误信息包含：`step_no`、`input_index`、`state_id`、`lookahead`、`expected terminals`。
  - 解析过程 trace 全量记录：每一步动作、栈快照、lookahead、输入指针、归约产生式。
- 已接入命令行主流程：`src/tools/yacc_parse_main.cpp`
  - 新增参数：
    - `--parse-tokens <file>`：启用第 9 步并读取 token 序列；
    - `--max-parse-steps <n>`：限制解析循环最大步数（防死循环）。
  - 未提供 `--parse-tokens` 时第 9 步默认跳过。
- 已接入结构化导出（不散落）：
  - 默认目录：`artifacts/yacc/step9/<input_stem>/`
  - 新增导出文件：
    - `raw/parse_input_tokens.tsv`
    - `raw/parse_trace.tsv`
    - `raw/parse_reductions.txt`
    - `raw/parse_error.txt`
  - `summary.txt`/`analysis/report.txt` 增加第 9 步指标：
    - `parse_accepted`
    - `parse_total_steps`
    - `parse_consumed_tokens`
    - `parse_reductions`
- 已提供样例 token 输入：
  - `contracts/yacc/tokens/c99_decl_int.tokens`（合法：`int;`）
  - `contracts/yacc/tokens/c99_invalid_if.tokens`（非法样例）
  - `contracts/yacc/tokens/c99_func_return_const.tokens`（复杂合法：`int main(){ return 0; }`）
- 已提供自动验收脚本：
  - `scripts/yacc_step9_validate.py`
  - 可执行：
    - `python3 scripts/yacc_step9_validate.py --tokens contracts/yacc/tokens/c99_func_return_const.tokens --expect accept --export-dir artifacts/yacc/step9/validate_func_return`
    - `python3 scripts/yacc_step9_validate.py --tokens contracts/yacc/tokens/c99_invalid_if.tokens --expect reject --export-dir artifacts/yacc/step9/validate_invalid_if`

**第 9 步完成判定**

- 对合法 token 序列可得到 `accept`，并导出完整移进/归约 trace；
- 对非法 token 序列可稳定报错，且给出状态号、lookahead、期待终结符；
- 规约序列可导出并用于后续第 12 步语义接口或 AST 构造映射；
- 输出目录统一落在 `artifacts/yacc/step9/...`，不与前置步骤混乱。

---

### 第 10 步：实现 LR(1) 到 LALR(1) 的转换

**要做什么**

- 按 PPT 要求，把 **LR(1) 状态合并为 LALR(1)**：
  - 核相同的状态进行合并
  - 合并 lookahead
- 重新构造 Action/Goto 表。
- 对比 LR(1) 与 LALR(1) 的状态数和冲突情况。

**做到什么程度算完成**

- 可切换输出 LR(1) 或 LALR(1) 状态机。
- 能说明哪些状态被合并了。
- 若冲突增加，能给出分析。

**最低验收标准**

- 至少完成一版正确的状态合并。
- 文档中能说明实现方法，而不是只给结论。

**产出**

- LALR 合并模块
- 合并前后状态数对比
- 冲突对比分析

**当前实现状态（已完成）**

- 已新增模块：
  - `src/yacc/lalr/lalr_builder.h`
  - `src/yacc/lalr/lalr_builder.cpp`
- 已实现能力：
  - `build_step10_lalr_from_lr1(...)`：
    - 以 **LR(0) 核（production_id + dot_pos）相同** 作为分组合并条件；
    - 对组内 lookahead 做并集，得到 LALR 状态项集；
    - 构造 `LR(1)状态 -> LALR状态` 映射、合并组明细、LALR 转移边；
    - 在合并后状态机上重新构造 LALR Action/Goto 表。
  - `validate_step10_lalr_from_lr1(...)`：
    - 校验映射完整性（每个 LR(1) 状态都有对应 LALR 状态）；
    - 校验转移确定性（同一 `from+symbol` 不出现多目标）；
    - 复用第 8 步校验逻辑检查 LALR 表合法性；
    - 输出冲突与状态压缩对比告警。
- 已接入命令行主流程：`src/tools/yacc_parse_main.cpp`
  - 在第 8 步后自动执行第 10 步；
  - 第 10 步校验失败返回码 `10`；
  - 终端输出：`LR(1)状态数 / LALR状态数 / 压缩数 / 冲突数对比`。
- 已接入结构化导出：
  - 默认目录：`artifacts/yacc/step10/<input_stem>/`
  - 新增导出文件：
    - `raw/lr1_to_lalr_state_map.tsv`
    - `raw/lalr_merge_groups.tsv`
    - `raw/lalr_state_items.txt`
    - `raw/lalr_transitions.tsv`
    - `raw/lalr_action_table.tsv`
    - `raw/lalr_goto_table.tsv`
    - `raw/lalr_parse_table_conflicts.tsv`
    - `raw/lalr_parse_table_conflict_resolution.tsv`
  - 并保留 step8 及之前的继承文件（便于横向对比 LR(1) 与 LALR(1)）。

**第 10 步完成判定**

- 对 `c99.y` 运行后可稳定得到 LALR 状态机与 LALR 分析表；
- `analysis/report.txt` 中 `lr1_step10_validation_passed=true`；
- 可在 `raw/lalr_merge_groups.tsv` 追溯每个 LALR 状态由哪些 LR(1) 状态合并而来；
- 可通过 `summary.txt`/`analysis/report.txt` 直接对比 `lr1_states vs lalr_states`、`lr1_conflicts vs lalr_conflicts`；
- 多次运行结果稳定可复现（同输入下分组与对比指标一致）。

---

### 第 11 步：与 Lex 对接

**要做什么**

- 统一 token 编号和 token 名称。
- Lex 输出给 Yacc 的接口格式 参考flex和bison的形式
- 测试从源程序到 token 流再到语法分析的完整链路。

**做到什么程度算完成**

- Yacc 不再依赖手写 token 序列。
- 能直接吃 Lex 模块产生的 token 流。
- 至少跑通一批简单 C99 样例。

**最低验收标准**

- 关键字、标识符、常量、分隔符等 token 能被正确识别并送入分析器。
- 对接后不会因 token 名称不一致导致大量伪错误。

**产出**

- Lex-Yacc 接口说明
- 联调日志
- 端到端样例

---

### 第 12 步：为语义分析/语法树构造预留接口

**要做什么**

- 明确 Yacc 归约时如何把信息交给语义部分，至少支持以下一种：
  - 输出产生式序列
  - 归约时构造语法树
  - 归约时触发动作回调
- 如果本阶段不真正执行语义动作，也要设计好接口。

**做到什么程度算完成**

- 语义模块能清楚知道“当前规约用了哪条产生式”。
- 文档中明确：
  - 哪些动作在语法分析阶段执行
  - 哪些动作在后续遍历阶段执行

**最低验收标准**

- 至少能够输出“句柄序列/产生式序列”。
- 最好能构造基础语法树节点。

**产出**

- 语义接口设计说明
- 归约到 AST/动作的映射方案

---

### 第 13 步：冲突分析、错误处理与调试工具

**要做什么**

- 为分析表冲突实现调试输出。
- 为 parser 报错实现更易读的信息：
  - 当前状态
  - 当前 token
  - 可能期待的终结符
- 添加辅助工具：
  - 打印 First 集
  - 打印状态集
  - 打印分析表
  - 打印规约过程

**做到什么程度算完成**

- 调试时能定位算法错误，不靠肉眼硬猜。
- 在答辩时可以演示中间结果，而不是只展示最终 accept/reject。

**产出**

- 调试工具集
- 错误信息样例
- 冲突分析记录

---

### 第 14 步：测试设计与测试执行

**要做什么**

- 设计 3 类测试：
  - 文法级算法测试：First、Closure、Goto、分析表
  - 语法分析测试：合法/非法 token 序列
  - 联调测试：真实 C99 子集程序
- 根据 PPT，重点覆盖：
  - 多层嵌套
  - `if/else`
  - `while/do/for`
  - 函数定义和调用
  - 声明语句
  - 复杂表达式

**做到什么程度算完成**

- 至少有一组“最小样例”、一组“典型样例”、一组“错误样例”。
- 每组都记录输入、预期结果、实际结果。
- 测试结果可写入实验报告。

**产出**

- 测试用例集
- 测试结果表
- 问题修复记录

---

### 第 15 步：详细设计文档与中期检查材料

**要做什么**

- 按 PPT 的中期检查要求整理 YACC 详细设计方案，至少包括：
  - 输入文件格式
  - 程序架构
  - 模块划分
  - 数据结构
  - 主要算法伪代码
  - 接口定义
  - 当前完成度与后续计划

**做到什么程度算完成**

- 可以单独提交给助教作为中期材料。
- 让别人不看代码也能知道你的实现思路。

**产出**

- YACC 详细设计文档
- 中期检查汇报材料

---

### 第 16 步：实验报告、答辩与 AI 使用记录

**要做什么**

- 按 PPT 的要求补齐报告内容：
  - 目标语言描述
  - Yacc 输入文件说明
  - 程序架构与接口
  - 数据结构
  - 主要算法伪代码
  - 测试报告
  - 实际效果与讨论
- 单独整理 AI 使用记录：
  - 使用了什么 AI 工具
  - 提示词
  - AI 返回了什么
  - 你如何验证和手工修正

**做到什么程度算完成**

- 报告能支撑答辩，不只是代码说明书。
- AI 使用记录完整，符合课程要求。

**产出**

- 实验报告的 YACC 章节
- AI 使用记录
- 答辩 PPT 中的 YACC 页面

---

## 5. 推荐的阶段划分

结合粗计划和 PPT，可把 YACC 部分压缩成以下阶段：

### 阶段 A：需求确认与设计

- 完成第 1-4 步。
- 结果：能正确解析 `c99.y`，并完成文法内部表示设计。

### 阶段 B：核心算法

- 完成第 5-8 步。
- 结果：能自动生成 First、LR(1) 项目集、Action/Goto 表。

### 阶段 C：分析器执行

- 完成第 9-10 步。
- 结果：能真正进行 LR/LALR 语法分析。

### 阶段 D：联调与扩展

- 完成第 11-13 步。
- 结果：能和 Lex/语义部分协作，并能调试冲突和错误。

### 阶段 E：测试与验收

- 完成第 14-16 步。
- 结果：有测试、有文档、有报告、有答辩材料。

---

## 6. 每一步的“完成标准”汇总

如果要快速判断 YACC 部分是否真的做完，可以用下面这份清单验收：

1. 能正确解析 `c99.y`，拿到终结符、非终结符、产生式。
2. 能输出增广文法。
3. 能正确计算 First 集。
4. 能构造 LR(1) 项闭包和 Goto。
5. 能生成 LR(1) 项目集规范族。
6. 能构造 Action/Goto 表。
7. 能检测并报告冲突。
8. 能实现移进-归约总控程序。
9. 能接受真实 token 流并完成语法分析。
10. 能输出规约序列、语法树或动作触发信息。
11. 能完成 LR(1) 到 LALR(1) 的转换。
12. 能跑测试用例并形成报告材料。

只做到前 3-4 项，只能算“完成了文法算法的一部分”；做到前 8 项，才算“YACC 核心完成”；做到全部，才算“课程实践中的 YACC 部分基本闭环”。

---

## 7. 最终建议的实际落地顺序

如果你现在要真正开始做，建议严格按下面顺序推进：

1. 先做 `c99.y` 的输入解析器和内部数据结构。
2. 再做增广文法与 First 集。
3. 再做 LR(1) 项、Closure、Goto。
4. 然后做项目集规范族和状态转移图。
5. 再做 Action/Goto 表和冲突报告。
6. 再做 LR 总控程序。
7. 再做 LALR 合并。
8. 最后做 Lex 对接、测试、报告和答辩材料。

原因很简单：**前面的结果是后面的输入，顺序乱了会频繁返工。**

---

## 8. 对你当前任务最直接的结论

基于当前材料，YACC 部分需要做的事情可以概括为四句话：

- 先把 `c99.y` 解析成机器内部可处理的文法。
- 再把文法自动转换成 LR(1)/LALR(1) 分析表。
- 再实现查表驱动的移进-归约分析器。
- 最后完成联调、测试、文档、报告和 AI 使用记录。

如果后续你需要，我可以在这份文档基础上继续往下做两件事中的任意一个：

- 把这 16 步进一步改成“按周执行计划”。
- 直接开始帮你设计 **Yacc 输入解析器的数据结构和代码骨架**。
