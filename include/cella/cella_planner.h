// cella_planner.h —— 执行计划生成接口：通过语义检查的 AST -> 执行计划树（任务书 3.4 节）。
// 输出为缩进树文本的节点结构；计划阶段错误记为 PLN-4xx。
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "cella_ast.h"
#include "cella_catalog.h"
#include "cella_common.h"

namespace cella
{

    // 计划节点：op=算子名，detail=同行附加信息（可空），extra=子行信息，children=子算子
    // pred/onExpr：Filter 谓词与 Join ON 条件的表达式副本（供优化器做常量折叠/布尔化简）
    struct CELLA_PlanNode
    {
        std::string op;
        std::string detail;
        std::vector<std::string> extra;
        int line = 0, col = 0;
        std::unique_ptr<CELLA_Expr> pred;   // Filter 谓词（可空）
        std::unique_ptr<CELLA_Expr> onExpr; // Join ON 条件（可空）
        std::string joinKind;               // Join 方向：middle/left/right（Join 节点）
        std::vector<std::unique_ptr<CELLA_PlanNode>> children;
    };

    // 只为 stmtOk 为真的语句生成计划；错误追加到 errors
    // insertCols: 与语句对齐，INSERT 的实际目标列（省略列清单时已由语义阶段展开为全部列）
    std::vector<std::unique_ptr<CELLA_PlanNode>> cella_plan(const CELLA_Program &program,
                                                            const std::vector<bool> &stmtOk,
                                                            const std::vector<std::vector<std::string>> &insertCols,
                                                            std::vector<CELLA_Error> &errors);

} // namespace cella
