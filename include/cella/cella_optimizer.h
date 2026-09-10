// cella_optimizer.h —— 计划优化接口（任务书 v2 14.2 / guide 7）。
// 规则：常量折叠、布尔化简、恒真 Filter 消除；不改动输入计划树（深拷贝后变换）。
#pragma once

#include <memory>
#include <vector>

#include "cella_planner.h"

namespace cella
{

    struct CELLA_OptResult
    {
        std::vector<std::unique_ptr<CELLA_PlanNode>> plans; // 优化后的计划树
        int ruleHits = 0;                                   // 规则触发次数
    };

    // 对多条语句的计划树逐一优化
    CELLA_OptResult cella_optimizePlans(const std::vector<std::unique_ptr<CELLA_PlanNode>> &plans);

} // namespace cella
