// cella_lexer.h —— 词法分析接口：源文本 -> Token 流（含 EOF），错误收集到 errors。
// 输入: 整个源文本（单条或多条语句，';' 分隔）
// 输出: Token 流（始终以 EOF Token 结尾）；词法错误记为 LEX-1xx 并尽量恢复继续扫描。
#pragma once

#include <string>
#include <vector>

#include "cella_common.h"
#include "cella_token.h"

namespace cella
{

    // 单遍扫描：返回 Token 流；错误（LEX-101~LEX-104）追加到 errors
    std::vector<CELLA_Token> cella_tokenize(const std::string &src, std::vector<CELLA_Error> &errors);

} // namespace cella
