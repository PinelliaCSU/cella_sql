// cella_printer.h —— 输出接口：Token 流、AST、执行计划的缩进树文本（任务书 5/7 节格式）。
// 同一输入输出必须稳定，供 golden 回归。
#pragma once

#include <iosfwd>
#include <memory>
#include <vector>

#include "cella_ast.h"
#include "cella_planner.h"
#include "cella_token.h"

namespace cella
{

    // 表达式中缀文本（用于 AST/计划打印），按优先级加括号
    std::string cella_exprToString(const CELLA_Expr &e);

    void cella_printTokens(const std::vector<CELLA_Token> &tokens, std::ostream &os);
    void cella_printAst(const CELLA_Program &program, std::ostream &os);
    void cella_printPlan(const std::vector<std::unique_ptr<CELLA_PlanNode>> &plans, std::ostream &os);

} // namespace cella
