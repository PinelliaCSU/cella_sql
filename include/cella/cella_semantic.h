// cella_semantic.h —— 语义分析接口：AST + 内存 Catalog -> 检查结论与 Catalog 更新（任务书 3.3 节）。
// 每条语句独立检查：失败语句输出 SEM-3xx 诊断、不影响后续语句；CREATE TABLE 成功会更新 Catalog。
#pragma once

#include <string>
#include <vector>

#include "cella_ast.h"
#include "cella_catalog.h"
#include "cella_common.h"

namespace cella
{

    struct CELLA_SemanticResult
    {
        std::vector<CELLA_Error> errors;                     // SEM-3xx 诊断
        std::vector<std::string> okMessages;                 // 每条通过语句的 [语义] OK 行
        std::vector<bool> stmtOk;                            // 与 program.statements 对齐：是否通过
        std::vector<std::vector<std::string>> insertColumns; // 与语句对齐：INSERT 实际目标列
                                                             // （省略列清单时展开为全部列，供计划阶段使用）
    };

    CELLA_SemanticResult cella_analyze(const CELLA_Program &program, CELLA_Catalog &catalog);

} // namespace cella
