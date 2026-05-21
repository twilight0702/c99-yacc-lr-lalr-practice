# YACC 可视化子系统设计与使用说明

## 1. 目标

将可视化能力与核心编译算法代码解耦：

1. `/src` 只负责算法计算与原始产物导出（`artifacts/yacc/stepX/...`）。
2. `/visualizer` 独立负责展示、交互、导出图片/JSON。
3. 通过版本化数据协议连接，避免强依赖绑定。

## 2. 目录结构

```text
contracts/
  yacc-artifact-schema/v1/
    *.schema.json
scripts/
  yacc_visualizer_prepare.py
visualizer/
  src/
  public/data/v1/<case_id>/
```

## 3. 数据流

1. 运行解析工具，生成 `artifacts/yacc/step3~stepN/<case>/...`。
2. 运行适配脚本，将原始文本产物转换为协议 JSON：
   - `manifest.json`
   - `step3/data.json` ... `step6/data.json`
3. 可视化前端按 `manifest` 动态加载步骤数据。

## 4. 当前支持范围

已接入 step3~step6：

1. Step3：符号表、产生式。
2. Step4：增广文法、lhs 到产生式索引。
3. Step5：First 集。
4. Step6：LR(1) I0 kernel/closure、goto(I0, X)、lookahead 推导记录。

## 5. 运行方式

在仓库根目录：

```bash
python3 scripts/yacc_visualizer_prepare.py --case c99
```

启动前端：

```bash
cd visualizer
npm install
npm run dev
```

打开：

```text
http://localhost:5174/?case=c99
```

## 6. 可维护性约束

1. 新增步骤时优先扩展协议（`contracts/.../v1`）和适配脚本，不直接把 `/src` 内部对象传给前端。
2. 字段语义破坏性变更时，升到 `v2`，并保留旧版适配逻辑。
3. 所有可视化页面仅依赖协议 JSON，保证后续 LR0/LR1/LALR 扩展时最小改动。
