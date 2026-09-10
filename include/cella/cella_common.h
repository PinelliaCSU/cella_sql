// cella_common.h —— 公共定义：统一错误结构与字符串工具。
// 约定：各阶段错误收集到 std::vector<CELLA_Error> 后由 CLI 统一输出，不使用异常。
#pragma once

#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace cella
{

    // 错误阶段（决定诊断行前缀）
    enum class CELLA_Phase
    {
        LEX,
        SYN,
        SEM,
        PLN
    };

    inline const char *cella_phaseText(CELLA_Phase p)
    {
        switch (p)
        {
        case CELLA_Phase::LEX:
            return "词法";
        case CELLA_Phase::SYN:
            return "语法";
        case CELLA_Phase::SEM:
            return "语义";
        case CELLA_Phase::PLN:
            return "计划";
        }
        return "?";
    }

    // 统一错误：阶段 + 错误码 + 位置(行:列，1 起) + 说明
    struct CELLA_Error
    {
        CELLA_Phase phase = CELLA_Phase::LEX;
        std::string code;
        int line = 0;
        int col = 0;
        std::string message;
    };

    // 按任务书第 4 节契约格式化为一行
    inline std::string cella_errorText(const CELLA_Error &e)
    {
        std::ostringstream os;
        os << "[" << cella_phaseText(e.phase) << "] " << e.code << " @"
           << e.line << ":" << e.col << " " << e.message;
        return os.str();
    }

    inline CELLA_Error cella_makeError(CELLA_Phase phase, const std::string &code, int line, int col,
                                       const std::string &message)
    {
        CELLA_Error e;
        e.phase = phase;
        e.code = code;
        e.line = line;
        e.col = col;
        e.message = message;
        return e;
    }

    // 大写化（用于关键字与目录的大小写不敏感比较）
    inline std::string cella_toUpper(const std::string &s)
    {
        std::string r = s;
        for (auto &c : r)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return r;
    }

    // 是否形如 YYYY-MM-DD（日期字面量识别）
    inline bool cella_isDateText(const std::string &s)
    {
        if (s.size() != 10)
            return false;
        if (s[4] != '-' || s[7] != '-')
            return false;
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            if (i == 4 || i == 7)
                continue;
            if (!std::isdigit(static_cast<unsigned char>(s[i])))
                return false;
        }
        return true;
    }

} // namespace cella
