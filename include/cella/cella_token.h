// cella_token.h —— Token 定义：种别码、关键字总表、值类别与携带词素/位置的 Token 结构。
// 对应任务书 3.1 节；沿用既有 CELLA_ 前缀命名风格。
#pragma once

#include <string>
#include <unordered_map>

#include "cella_common.h"

// NULL 可能是宏（<cstddef> 等定义），先解除再使用枚举名
#ifdef NULL
#undef NULL
#endif

namespace cella
{

    enum class CELLA_TokenType
    {
        UNKNOWN,
        EOF_T,
        KEYWORD,
        IDENTIFIER,
        CONST,
        OPERATOR,
        DELIMITER
    };
    enum class CELLA_TokenValueType
    {
        NONE,
        NUMBER,
        DATE,
        STRING
    };
    enum class CELLA_TokenState
    {
        UNKNOWN,
        VALID,
        INVALID
    };

    // 保留字总表（任务书 2.2 节，大小写不敏感，全部不得用作标识符）
    enum class CELLA_Keyword
    {
        NONE,
        CREATE,
        TABLE,
        ALTER,
        DROP,
        TRUNCATE,
        RENAME,
        INSERT,
        INTO,
        VALUES,
        UPDATE,
        SET,
        DELETE,
        FROM,
        WHERE,
        GET,
        IN,
        LIMIT,
        GROUPED,
        HAVING,
        ORDERED,
        AMONG,
        PAGE,
        JOIN,
        ON,
        LEFT,
        RIGHT,
        MIDDLE,
        UNION,
        DISTINCT,
        AS,
        AND,
        OR,
        NOT,
        NULL,
        TRUE,
        FALSE,
        ASC,
        DESC,
        IS,
        INT,
        INTEGER,
        FLOAT,
        DOUBLE,
        CHAR,
        VARCHAR,
        TEXT,
        DATE,
        TIME,
        DATETIME
    };

    // 内部别名：防止后续头文件再次定义 NULL 宏导致使用处被展开
    static constexpr CELLA_Keyword CELLA_KW_NULL = CELLA_Keyword::NULL;

    // Token：种别码 + 原始词素 + 值类别 + 行:列(1 起) + 状态
    struct CELLA_Token
    {
        CELLA_TokenType type = CELLA_TokenType::UNKNOWN;
        std::string lexeme;                                          // 源代码中的原始词素
        CELLA_TokenValueType valueType = CELLA_TokenValueType::NONE; // 仅 CONST 有效
        std::string textValue;                                       // 字符串/日期内容或数字规范化文本
        double numValue = 0;                                         // NUMBER 数值
        int line = 0, col = 0;                                       // 1 起
        CELLA_TokenState state = CELLA_TokenState::VALID;
        CELLA_Keyword keyword = CELLA_Keyword::NONE; // KEYWORD 时有效
    };

    // 规范拼写（统一大写）
    inline std::string cella_keywordText(CELLA_Keyword kw)
    {
        switch (kw)
        {
        case CELLA_Keyword::NONE:
            return "";
        case CELLA_Keyword::CREATE:
            return "CREATE";
        case CELLA_Keyword::TABLE:
            return "TABLE";
        case CELLA_Keyword::ALTER:
            return "ALTER";
        case CELLA_Keyword::DROP:
            return "DROP";
        case CELLA_Keyword::TRUNCATE:
            return "TRUNCATE";
        case CELLA_Keyword::RENAME:
            return "RENAME";
        case CELLA_Keyword::INSERT:
            return "INSERT";
        case CELLA_Keyword::INTO:
            return "INTO";
        case CELLA_Keyword::VALUES:
            return "VALUES";
        case CELLA_Keyword::UPDATE:
            return "UPDATE";
        case CELLA_Keyword::SET:
            return "SET";
        case CELLA_Keyword::DELETE:
            return "DELETE";
        case CELLA_Keyword::FROM:
            return "FROM";
        case CELLA_Keyword::WHERE:
            return "WHERE";
        case CELLA_Keyword::GET:
            return "GET";
        case CELLA_Keyword::IN:
            return "IN";
        case CELLA_Keyword::LIMIT:
            return "LIMIT";
        case CELLA_Keyword::GROUPED:
            return "GROUPED";
        case CELLA_Keyword::HAVING:
            return "HAVING";
        case CELLA_Keyword::ORDERED:
            return "ORDERED";
        case CELLA_Keyword::AMONG:
            return "AMONG";
        case CELLA_Keyword::PAGE:
            return "PAGE";
        case CELLA_Keyword::JOIN:
            return "JOIN";
        case CELLA_Keyword::ON:
            return "ON";
        case CELLA_Keyword::LEFT:
            return "LEFT";
        case CELLA_Keyword::RIGHT:
            return "RIGHT";
        case CELLA_Keyword::MIDDLE:
            return "MIDDLE";
        case CELLA_Keyword::UNION:
            return "UNION";
        case CELLA_Keyword::DISTINCT:
            return "DISTINCT";
        case CELLA_Keyword::AS:
            return "AS";
        case CELLA_Keyword::AND:
            return "AND";
        case CELLA_Keyword::OR:
            return "OR";
        case CELLA_Keyword::NOT:
            return "NOT";
        case CELLA_Keyword::NULL:
            return "NULL";
        case CELLA_Keyword::TRUE:
            return "TRUE";
        case CELLA_Keyword::FALSE:
            return "FALSE";
        case CELLA_Keyword::ASC:
            return "ASC";
        case CELLA_Keyword::DESC:
            return "DESC";
        case CELLA_Keyword::IS:
            return "IS";
        case CELLA_Keyword::INT:
            return "INT";
        case CELLA_Keyword::INTEGER:
            return "INTEGER";
        case CELLA_Keyword::FLOAT:
            return "FLOAT";
        case CELLA_Keyword::DOUBLE:
            return "DOUBLE";
        case CELLA_Keyword::CHAR:
            return "CHAR";
        case CELLA_Keyword::VARCHAR:
            return "VARCHAR";
        case CELLA_Keyword::TEXT:
            return "TEXT";
        case CELLA_Keyword::DATE:
            return "DATE";
        case CELLA_Keyword::TIME:
            return "TIME";
        case CELLA_Keyword::DATETIME:
            return "DATETIME";
        }
        return "";
    }

    // 大小写不敏感的关键字查表（返回 NONE 表示非关键字）
    inline CELLA_Keyword cella_keywordFromString(const std::string &lexeme)
    {
        static const std::unordered_map<std::string, CELLA_Keyword> table = {
            {"CREATE", CELLA_Keyword::CREATE},
            {"TABLE", CELLA_Keyword::TABLE},
            {"ALTER", CELLA_Keyword::ALTER},
            {"DROP", CELLA_Keyword::DROP},
            {"TRUNCATE", CELLA_Keyword::TRUNCATE},
            {"RENAME", CELLA_Keyword::RENAME},
            {"INSERT", CELLA_Keyword::INSERT},
            {"INTO", CELLA_Keyword::INTO},
            {"VALUES", CELLA_Keyword::VALUES},
            {"UPDATE", CELLA_Keyword::UPDATE},
            {"SET", CELLA_Keyword::SET},
            {"DELETE", CELLA_Keyword::DELETE},
            {"FROM", CELLA_Keyword::FROM},
            {"WHERE", CELLA_Keyword::WHERE},
            {"GET", CELLA_Keyword::GET},
            {"IN", CELLA_Keyword::IN},
            {"LIMIT", CELLA_Keyword::LIMIT},
            {"GROUPED", CELLA_Keyword::GROUPED},
            {"HAVING", CELLA_Keyword::HAVING},
            {"ORDERED", CELLA_Keyword::ORDERED},
            {"AMONG", CELLA_Keyword::AMONG},
            {"PAGE", CELLA_Keyword::PAGE},
            {"JOIN", CELLA_Keyword::JOIN},
            {"ON", CELLA_Keyword::ON},
            {"LEFT", CELLA_Keyword::LEFT},
            {"RIGHT", CELLA_Keyword::RIGHT},
            {"MIDDLE", CELLA_Keyword::MIDDLE},
            {"UNION", CELLA_Keyword::UNION},
            {"DISTINCT", CELLA_Keyword::DISTINCT},
            {"AS", CELLA_Keyword::AS},
            {"AND", CELLA_Keyword::AND},
            {"OR", CELLA_Keyword::OR},
            {"NOT", CELLA_Keyword::NOT},
            {"NULL", CELLA_Keyword::NULL},
            {"TRUE", CELLA_Keyword::TRUE},
            {"FALSE", CELLA_Keyword::FALSE},
            {"ASC", CELLA_Keyword::ASC},
            {"DESC", CELLA_Keyword::DESC},
            {"IS", CELLA_Keyword::IS},
            {"INT", CELLA_Keyword::INT},
            {"INTEGER", CELLA_Keyword::INTEGER},
            {"FLOAT", CELLA_Keyword::FLOAT},
            {"DOUBLE", CELLA_Keyword::DOUBLE},
            {"CHAR", CELLA_Keyword::CHAR},
            {"VARCHAR", CELLA_Keyword::VARCHAR},
            {"TEXT", CELLA_Keyword::TEXT},
            {"DATE", CELLA_Keyword::DATE},
            {"TIME", CELLA_Keyword::TIME},
            {"DATETIME", CELLA_Keyword::DATETIME},
        };
        auto it = table.find(cella_toUpper(lexeme));
        return it == table.end() ? CELLA_Keyword::NONE : it->second;
    }

    inline std::string cella_valueTypeText(CELLA_TokenValueType v)
    {
        switch (v)
        {
        case CELLA_TokenValueType::NONE:
            return "NONE";
        case CELLA_TokenValueType::NUMBER:
            return "NUMBER";
        case CELLA_TokenValueType::DATE:
            return "DATE";
        case CELLA_TokenValueType::STRING:
            return "STRING";
        }
        return "?";
    }

} // namespace cella
