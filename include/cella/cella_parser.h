// cella_parser.h —— 语法分析接口：Token 流 -> AST（任务书 2.4 节完整文法）。
// 递归下降 + 预测分析；语法错误记为 SYN-2xx，并同步恢复到下一个 ';' 继续解析。
#pragma once

#include <memory>
#include <vector>

#include "cella_ast.h"
#include "cella_common.h"
#include "cella_token.h"

namespace cella
{

    // 返回 Program（即使出错也尽量返回已解析部分）；错误追加到 errors
    std::unique_ptr<CELLA_Program> cella_parse(const std::vector<CELLA_Token> &tokens,
                                               std::vector<CELLA_Error> &errors);

} // namespace cella
