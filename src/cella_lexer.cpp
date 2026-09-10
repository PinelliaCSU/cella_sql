// cella_lexer.cpp —— 词法分析：源文本 -> Token 流（任务书 2.3 / 3.1 节）。
// 识别关键字/标识符/数字/字符串/日期常量/运算符/分隔符；跳过空白与注释；
// 词法错误（LEX-101~LEX-104）标记 Token 为 INVALID 后继续扫描，一次运行可暴露多个词法错误。
#include "cella/cella_lexer.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace cella
{
    namespace
    {

        // 游标：跟踪行/列（1 起）
        struct CELLA_Cursor
        {
            const std::string &src;
            size_t pos = 0;
            int line = 1;
            int col = 1;

            explicit CELLA_Cursor(const std::string &s) : src(s) {}

            bool eof() const { return pos >= src.size(); }
            char ch(size_t off = 0) const { return pos + off < src.size() ? src[pos + off] : '\0'; }

            void advance()
            {
                if (eof())
                    return;
                char c = src[pos++];
                if (c == '\r')
                {
                    // \r\n 视为一个换行
                    if (!eof() && src[pos] == '\n')
                        pos++;
                    line++;
                    col = 1;
                }
                else if (c == '\n')
                {
                    line++;
                    col = 1;
                }
                else
                {
                    col++;
                }
            }
        };

        // 非法字符的可显示文本
        std::string charText(char c)
        {
            if (static_cast<unsigned char>(c) >= 32)
                return std::string(1, c);
            char buf[8];
            std::snprintf(buf, sizeof(buf), "0x%02X", static_cast<unsigned char>(c));
            return buf;
        }

    } // namespace

    std::vector<CELLA_Token> cella_tokenize(const std::string &src, std::vector<CELLA_Error> &errors)
    {
        std::vector<CELLA_Token> tokens;
        CELLA_Cursor cur(src);

        auto lexError = [&](const std::string &code, int line, int col, const std::string &msg)
        {
            errors.push_back(cella_makeError(CELLA_Phase::LEX, code, line, col, msg));
        };

        // 跳过文件开头的 UTF-8 BOM（EF BB BF）
        if (src.size() >= 3 && static_cast<unsigned char>(src[0]) == 0xEF &&
            static_cast<unsigned char>(src[1]) == 0xBB && static_cast<unsigned char>(src[2]) == 0xBF)
        {
            cur.advance();
            cur.advance();
            cur.advance();
        }

        while (!cur.eof())
        {
            char c = cur.ch();

            // 空白
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
                cur.advance();
                continue;
            }

            int tLine = cur.line;
            int tCol = cur.col;
            size_t startPos = cur.pos;

            // -- 行注释（注意：\r\n 在 advance() 中一次性消费，故同时判断 \r 与 \n）
            if (c == '-' && cur.ch(1) == '-')
            {
                while (!cur.eof() && cur.ch() != '\n' && cur.ch() != '\r')
                    cur.advance();
                continue;
            }

            // /* 块注释 */（可跨行，不嵌套）
            if (c == '/' && cur.ch(1) == '*')
            {
                cur.advance();
                cur.advance();
                bool closed = false;
                while (!cur.eof())
                {
                    if (cur.ch() == '*' && cur.ch(1) == '/')
                    {
                        cur.advance();
                        cur.advance();
                        closed = true;
                        break;
                    }
                    cur.advance();
                }
                if (!closed)
                    lexError("LEX-104", tLine, tCol, "未闭合的块注释");
                continue;
            }

            CELLA_Token tk;
            tk.line = tLine;
            tk.col = tCol;

            // 标识符 / 关键字
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
            {
                while (!cur.eof() &&
                       (std::isalnum(static_cast<unsigned char>(cur.ch())) || cur.ch() == '_'))
                {
                    cur.advance();
                }
                tk.lexeme = src.substr(startPos, cur.pos - startPos);
                tk.keyword = cella_keywordFromString(tk.lexeme);
                tk.type = (tk.keyword != CELLA_Keyword::NONE) ? CELLA_TokenType::KEYWORD
                                                              : CELLA_TokenType::IDENTIFIER;
                tokens.push_back(tk);
                continue;
            }

            // 数字常量（贪婪扫描含 '.'，多个 '.' 视为格式错误）
            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                while (!cur.eof() &&
                       (std::isdigit(static_cast<unsigned char>(cur.ch())) || cur.ch() == '.'))
                {
                    cur.advance();
                }
                tk.lexeme = src.substr(startPos, cur.pos - startPos);
                int dots = 0;
                for (char ch : tk.lexeme)
                {
                    if (ch == '.')
                        dots++;
                }
                if (dots > 1)
                {
                    tk.type = CELLA_TokenType::UNKNOWN;
                    tk.state = CELLA_TokenState::INVALID;
                    lexError("LEX-103", tLine, tCol, "数字格式错误 \"" + tk.lexeme + "\"");
                }
                else
                {
                    tk.type = CELLA_TokenType::CONST;
                    tk.valueType = CELLA_TokenValueType::NUMBER;
                    tk.numValue = std::strtod(tk.lexeme.c_str(), nullptr);
                    tk.textValue = tk.lexeme;
                    if (!tk.textValue.empty() && tk.textValue.back() == '.')
                        tk.textValue.pop_back();
                }
                tokens.push_back(tk);
                continue;
            }

            // 字符串常量：'...'，内部 '' 转义；内容匹配 YYYY-MM-DD 时值类别为 DATE
            if (c == '\'')
            {
                cur.advance();
                std::string content;
                bool closed = false;
                while (!cur.eof())
                {
                    char c2 = cur.ch();
                    if (c2 == '\'')
                    {
                        if (cur.ch(1) == '\'')
                        { // '' 转义
                            content += '\'';
                            cur.advance();
                            cur.advance();
                            continue;
                        }
                        cur.advance();
                        closed = true;
                        break;
                    }
                    if (c2 == '\n' || c2 == '\r')
                        break; // 字符串不允许跨行
                    content += c2;
                    cur.advance();
                }
                tk.lexeme = src.substr(startPos, cur.pos - startPos);
                tk.type = CELLA_TokenType::CONST;
                tk.textValue = content;
                if (!closed)
                {
                    tk.state = CELLA_TokenState::INVALID;
                    lexError("LEX-102", tLine, tCol, "未闭合的字符串字面量");
                }
                else
                {
                    tk.valueType = cella_isDateText(content) ? CELLA_TokenValueType::DATE
                                                             : CELLA_TokenValueType::STRING;
                }
                tokens.push_back(tk);
                continue;
            }

            // 运算符（先匹配两字符）
            static const char *twoCharOps[] = {"!=", "<>", "<=", ">=", "=="};
            bool matched = false;
            for (const char *op : twoCharOps)
            {
                if (c == op[0] && cur.ch(1) == op[1])
                {
                    cur.advance();
                    cur.advance();
                    tk.lexeme = op;
                    tk.type = CELLA_TokenType::OPERATOR;
                    tokens.push_back(tk);
                    matched = true;
                    break;
                }
            }
            if (matched)
                continue;

            // 单字符运算符：= < > + - * / .（. 用于 表名.列名）
            if (std::string("=<>+-*/.").find(c) != std::string::npos)
            {
                cur.advance();
                tk.lexeme = std::string(1, c);
                tk.type = CELLA_TokenType::OPERATOR;
                tokens.push_back(tk);
                continue;
            }

            // 分隔符
            if (c == '(' || c == ')' || c == ',' || c == ';')
            {
                cur.advance();
                tk.lexeme = std::string(1, c);
                tk.type = CELLA_TokenType::DELIMITER;
                tokens.push_back(tk);
                continue;
            }

            // 非法字符：标记 INVALID 后继续扫描
            tk.type = CELLA_TokenType::UNKNOWN;
            tk.state = CELLA_TokenState::INVALID;
            tk.lexeme = std::string(1, c);
            lexError("LEX-101", tLine, tCol, "非法字符 '" + charText(c) + "' 不在任何 Token 类别中");
            cur.advance();
            tokens.push_back(tk);
        }

        // EOF Token
        CELLA_Token eofTk;
        eofTk.type = CELLA_TokenType::EOF_T;
        eofTk.line = cur.line;
        eofTk.col = cur.col;
        tokens.push_back(eofTk);

        return tokens;
    }

} // namespace cella
