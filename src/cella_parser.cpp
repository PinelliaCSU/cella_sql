// cella_parser.cpp —— 语法分析：递归下降 + 预测分析，实现任务书 2.4 节完整文法。
// 错误记为 SYN-201（含期望/实际）与 SYN-202（缺分号），并同步恢复到下一个 ';'，
// 保证多语句输入一次能报告多条语法错误。
#include "cella/cella_parser.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cella
{
    namespace
    {

        std::unique_ptr<CELLA_Expr> makeBinary(CELLA_Expr::BinOp op, const CELLA_Token &t,
                                               std::unique_ptr<CELLA_Expr> l,
                                               std::unique_ptr<CELLA_Expr> r)
        {
            auto e = std::make_unique<CELLA_Expr>();
            e->kind = CELLA_Expr::Kind::BINARY;
            e->line = t.line;
            e->col = t.col;
            e->bop = op;
            e->left = std::move(l);
            e->right = std::move(r);
            return e;
        }

        std::unique_ptr<CELLA_Expr> makeUnary(CELLA_Expr::UnOp op, const CELLA_Token &t,
                                              std::unique_ptr<CELLA_Expr> child)
        {
            auto e = std::make_unique<CELLA_Expr>();
            e->kind = CELLA_Expr::Kind::UNARY;
            e->line = t.line;
            e->col = t.col;
            e->uop = op;
            e->child = std::move(child);
            return e;
        }

        class CELLA_ParserImpl
        {
        public:
            CELLA_ParserImpl(const std::vector<CELLA_Token> &toks, std::vector<CELLA_Error> &errs)
                : toks(toks), errors(errs) {}

            std::unique_ptr<CELLA_Program> parseProgram()
            {
                auto program = std::make_unique<CELLA_Program>();
                while (!atEnd())
                {
                    auto stmt = parseStatement();
                    if (stmt)
                    {
                        program->statements.push_back(std::move(stmt));
                    }
                    else
                    {
                        syncToSemicolon();
                    }
                }
                return program;
            }

        private:
            const std::vector<CELLA_Token> &toks;
            std::vector<CELLA_Error> &errors;
            size_t pos = 0;

            // ---------------- Token 游标 ----------------

            const CELLA_Token &peek(size_t off = 0) const
            {
                size_t p = pos + off;
                return p < toks.size() ? toks[p] : toks.back();
            }
            const CELLA_Token &advance()
            {
                const CELLA_Token &t = toks[pos];
                if (pos + 1 < toks.size())
                    pos++;
                return t;
            }
            bool atEnd() const { return peek().type == CELLA_TokenType::EOF_T; }

            bool matchKw(CELLA_Keyword kw)
            {
                if (peek().type == CELLA_TokenType::KEYWORD && peek().keyword == kw)
                {
                    advance();
                    return true;
                }
                return false;
            }
            bool matchOp(const std::string &op)
            {
                if (peek().type == CELLA_TokenType::OPERATOR && peek().lexeme == op)
                {
                    advance();
                    return true;
                }
                return false;
            }
            bool matchDelim(const std::string &d)
            {
                if (peek().type == CELLA_TokenType::DELIMITER && peek().lexeme == d)
                {
                    advance();
                    return true;
                }
                return false;
            }

            // ---------------- 错误与恢复 ----------------

            static std::string tokenDesc(const CELLA_Token &t)
            {
                switch (t.type)
                {
                case CELLA_TokenType::EOF_T:
                    return "文件结束";
                case CELLA_TokenType::KEYWORD:
                    return "关键字 " + cella_keywordText(t.keyword);
                case CELLA_TokenType::IDENTIFIER:
                    return "标识符 " + t.lexeme;
                case CELLA_TokenType::CONST:
                    return "常量 " + t.lexeme;
                case CELLA_TokenType::OPERATOR:
                    return "操作符 " + t.lexeme;
                case CELLA_TokenType::DELIMITER:
                    return "分隔符 " + t.lexeme;
                default:
                    return "非法 Token \"" + t.lexeme + "\"";
                }
            }

            void synError(const CELLA_Token &t, const std::string &expected)
            {
                errors.push_back(cella_makeError(CELLA_Phase::SYN, "SYN-201", t.line, t.col,
                                                 "期望 " + expected + "，实际为 " + tokenDesc(t)));
            }

            void synMissingSemicolon(const CELLA_Token &t)
            {
                errors.push_back(cella_makeError(CELLA_Phase::SYN, "SYN-202", t.line, t.col,
                                                 "语句缺少分号 ';' 结尾"));
            }

            void syncToSemicolon()
            {
                while (!atEnd())
                {
                    const CELLA_Token &t = peek();
                    if (t.type == CELLA_TokenType::DELIMITER && t.lexeme == ";")
                    {
                        advance();
                        return;
                    }
                    advance();
                }
            }

            // ---------------- 基础 expect ----------------

            bool expectKw(CELLA_Keyword kw)
            {
                if (matchKw(kw))
                    return true;
                synError(peek(), "关键字 " + cella_keywordText(kw));
                return false;
            }
            bool expectOp(const std::string &op)
            {
                if (matchOp(op))
                    return true;
                synError(peek(), "操作符 '" + op + "'");
                return false;
            }
            bool expectDelim(const std::string &d)
            {
                if (matchDelim(d))
                    return true;
                synError(peek(), "分隔符 '" + d + "'");
                return false;
            }
            bool expectIdent(std::string &out)
            {
                const CELLA_Token &t = peek();
                if (t.type == CELLA_TokenType::IDENTIFIER)
                {
                    out = t.lexeme;
                    advance();
                    return true;
                }
                synError(t, "标识符");
                return false;
            }
            bool expectSemicolon()
            {
                const CELLA_Token &t = peek();
                if (t.type == CELLA_TokenType::DELIMITER && t.lexeme == ";")
                {
                    advance();
                    return true;
                }
                synMissingSemicolon(t);
                return false;
            }

            static std::unique_ptr<CELLA_Stmt> makeStmt(CELLA_Stmt::Kind k, const CELLA_Token &t)
            {
                auto s = std::make_unique<CELLA_Stmt>();
                s->kind = k;
                s->line = t.line;
                s->col = t.col;
                return s;
            }

            // ---------------- 语句 ----------------

            std::unique_ptr<CELLA_Stmt> parseStatement()
            {
                const CELLA_Token &t = peek();
                if (t.type == CELLA_TokenType::KEYWORD)
                {
                    switch (t.keyword)
                    {
                    case CELLA_Keyword::CREATE:
                        return parseCreateTable();
                    case CELLA_Keyword::INSERT:
                        return parseInsert();
                    case CELLA_Keyword::GET:
                        return parseGet();
                    case CELLA_Keyword::DELETE:
                        return parseDelete();
                    case CELLA_Keyword::UPDATE:
                        return parseUpdate();
                    case CELLA_Keyword::DROP:
                        return parseDropTable();
                    default:
                        break;
                    }
                }
                synError(t, "CREATE/INSERT/GET/DELETE/UPDATE/DROP");
                return nullptr;
            }

            // CREATE TABLE name '(' column_def { ',' column_def } ')' ';'
            std::unique_ptr<CELLA_Stmt> parseCreateTable()
            {
                const CELLA_Token &t = advance(); // CREATE
                auto st = makeStmt(CELLA_Stmt::Kind::CREATE_TABLE, t);
                if (!expectKw(CELLA_Keyword::TABLE))
                    return nullptr;
                if (!expectIdent(st->tableName))
                    return nullptr;
                if (!expectDelim("("))
                    return nullptr;
                for (;;)
                {
                    CELLA_ColumnDef cd;
                    const CELLA_Token &idTok = peek();
                    if (!expectIdent(cd.name))
                        return nullptr;
                    cd.line = idTok.line;
                    cd.col = idTok.col;
                    if (!parseDataType(cd.type))
                        return nullptr;
                    // 仅 CHAR/VARCHAR 允许 '(n)'
                    if ((cd.type == CELLA_DataType::CHAR || cd.type == CELLA_DataType::VARCHAR) &&
                        matchDelim("("))
                    {
                        const CELLA_Token &lenTok = peek();
                        if (lenTok.type == CELLA_TokenType::CONST &&
                            lenTok.valueType == CELLA_TokenValueType::NUMBER &&
                            lenTok.lexeme.find('.') == std::string::npos)
                        {
                            advance();
                            cd.hasLen = true;
                            cd.len = static_cast<int>(lenTok.numValue);
                        }
                        else
                        {
                            synError(lenTok, "非负整数");
                            return nullptr;
                        }
                        if (!expectDelim(")"))
                            return nullptr;
                    }
                    if (matchKw(CELLA_Keyword::NOT))
                    {
                        if (!expectKw(CELLA_KW_NULL))
                            return nullptr;
                        cd.notNull = true;
                    }
                    st->columns.push_back(std::move(cd));
                    if (matchDelim(","))
                        continue;
                    break;
                }
                if (!expectDelim(")"))
                    return nullptr;
                if (!expectSemicolon())
                    return nullptr;
                return st;
            }

            bool parseDataType(CELLA_DataType &out)
            {
                const CELLA_Token &t = peek();
                if (t.type != CELLA_TokenType::KEYWORD)
                {
                    synError(t, "数据类型 INT/FLOAT/DOUBLE/CHAR/VARCHAR/TEXT/DATE/TIME/DATETIME");
                    return false;
                }
                switch (t.keyword)
                {
                case CELLA_Keyword::INT:
                case CELLA_Keyword::INTEGER:
                    out = CELLA_DataType::INT;
                    break;
                case CELLA_Keyword::FLOAT:
                    out = CELLA_DataType::FLOAT;
                    break;
                case CELLA_Keyword::DOUBLE:
                    out = CELLA_DataType::DOUBLE;
                    break;
                case CELLA_Keyword::CHAR:
                    out = CELLA_DataType::CHAR;
                    break;
                case CELLA_Keyword::VARCHAR:
                    out = CELLA_DataType::VARCHAR;
                    break;
                case CELLA_Keyword::TEXT:
                    out = CELLA_DataType::TEXT;
                    break;
                case CELLA_Keyword::DATE:
                    out = CELLA_DataType::DATE;
                    break;
                case CELLA_Keyword::TIME:
                    out = CELLA_DataType::TIME;
                    break;
                case CELLA_Keyword::DATETIME:
                    out = CELLA_DataType::DATETIME;
                    break;
                default:
                    synError(t, "数据类型 INT/FLOAT/DOUBLE/CHAR/VARCHAR/TEXT/DATE/TIME/DATETIME");
                    return false;
                }
                advance();
                return true;
            }

            // INSERT INTO name [ '(' cols ')' ] VALUES row { ',' row } ';'
            std::unique_ptr<CELLA_Stmt> parseInsert()
            {
                const CELLA_Token &t = advance(); // INSERT
                auto st = makeStmt(CELLA_Stmt::Kind::INSERT, t);
                if (!expectKw(CELLA_Keyword::INTO))
                    return nullptr;
                if (!expectIdent(st->tableName))
                    return nullptr;
                if (matchDelim("("))
                {
                    for (;;)
                    {
                        std::string c;
                        if (!expectIdent(c))
                            return nullptr;
                        st->insertColumns.push_back(c);
                        if (matchDelim(","))
                            continue;
                        break;
                    }
                    if (!expectDelim(")"))
                        return nullptr;
                }
                if (!expectKw(CELLA_Keyword::VALUES))
                    return nullptr;
                for (;;)
                {
                    if (!expectDelim("("))
                        return nullptr;
                    std::vector<std::unique_ptr<CELLA_Expr>> row;
                    for (;;)
                    {
                        auto e = parseLiteral();
                        if (!e)
                            return nullptr;
                        row.push_back(std::move(e));
                        if (matchDelim(","))
                            continue;
                        break;
                    }
                    if (!expectDelim(")"))
                        return nullptr;
                    st->rows.push_back(std::move(row));
                    if (matchDelim(","))
                        continue;
                    break;
                }
                if (!expectSemicolon())
                    return nullptr;
                return st;
            }

            // get [ distinct ] select_list in table_ref { join_clause }
            //     [ limit expr ] [ grouped cols ] [ having expr ]
            //     [ ordered items ] [ among uint ] [ union get_stmt ] ';'
            std::unique_ptr<CELLA_Stmt> parseGet()
            {
                const CELLA_Token &t = advance(); // GET
                auto st = makeStmt(CELLA_Stmt::Kind::GET, t);
                if (!parseGetBody(*st))
                    return nullptr;
                if (!expectSemicolon())
                    return nullptr;
                return st;
            }

            bool parseGetBody(CELLA_Stmt &st)
            {
                if (matchKw(CELLA_Keyword::DISTINCT))
                    st.distinct = true;

                // select_list
                if (matchOp("*"))
                {
                    st.star = true;
                }
                else
                {
                    for (;;)
                    {
                        CELLA_SelectItem item;
                        item.expr = parseExpr();
                        if (!item.expr)
                            return false;
                        if (matchKw(CELLA_Keyword::AS))
                        {
                            if (!expectIdent(item.alias))
                                return false;
                        }
                        else if (peek().type == CELLA_TokenType::IDENTIFIER)
                        {
                            item.alias = peek().lexeme;
                            advance();
                        }
                        st.selectItems.push_back(std::move(item));
                        if (matchDelim(","))
                            continue;
                        break;
                    }
                }

                if (!expectKw(CELLA_Keyword::IN))
                    return false;
                if (!parseTableRef(st.from))
                    return false;

                // join_clause { join_clause }
                for (;;)
                {
                    CELLA_JoinKind kind = CELLA_JoinKind::MIDDLE;
                    if (matchKw(CELLA_Keyword::LEFT))
                        kind = CELLA_JoinKind::LEFT;
                    else if (matchKw(CELLA_Keyword::RIGHT))
                        kind = CELLA_JoinKind::RIGHT;
                    else if (matchKw(CELLA_Keyword::MIDDLE))
                        kind = CELLA_JoinKind::MIDDLE;
                    else if (!(peek().type == CELLA_TokenType::KEYWORD &&
                               peek().keyword == CELLA_Keyword::JOIN))
                        break;
                    if (!expectKw(CELLA_Keyword::JOIN))
                        return false;
                    CELLA_JoinClause jc;
                    jc.kind = kind;
                    if (!parseTableRef(jc.ref))
                        return false;
                    if (!expectKw(CELLA_Keyword::ON))
                        return false;
                    jc.on = parseExpr();
                    if (!jc.on)
                        return false;
                    st.joins.push_back(std::move(jc));
                }

                if (matchKw(CELLA_Keyword::LIMIT))
                {
                    st.limit = parseExpr();
                    if (!st.limit)
                        return false;
                }
                if (matchKw(CELLA_Keyword::GROUPED))
                {
                    for (;;)
                    {
                        CELLA_ColName cn;
                        if (!parseColName(cn))
                            return false;
                        st.grouped.push_back(cn);
                        if (matchDelim(","))
                            continue;
                        break;
                    }
                }
                if (matchKw(CELLA_Keyword::HAVING))
                {
                    st.having = parseExpr();
                    if (!st.having)
                        return false;
                }
                if (matchKw(CELLA_Keyword::ORDERED))
                {
                    for (;;)
                    {
                        CELLA_OrderItem oi;
                        if (!parseColName(oi.col))
                            return false;
                        if (matchKw(CELLA_Keyword::ASC))
                            oi.asc = true;
                        else if (matchKw(CELLA_Keyword::DESC))
                            oi.asc = false;
                        st.ordered.push_back(oi);
                        if (matchDelim(","))
                            continue;
                        break;
                    }
                }
                if (matchKw(CELLA_Keyword::AMONG))
                {
                    const CELLA_Token &t = peek();
                    if (t.type == CELLA_TokenType::CONST && t.valueType == CELLA_TokenValueType::NUMBER &&
                        t.lexeme.find('.') == std::string::npos)
                    {
                        st.among = static_cast<long long>(t.numValue);
                        advance();
                    }
                    else
                    {
                        synError(t, "非负整数");
                        return false;
                    }
                }
                // 分页：page 页码 [ ',' 每页行数 ]（页码 1 起，每页行数默认 10）
                if (matchKw(CELLA_Keyword::PAGE))
                {
                    const CELLA_Token &t1 = peek();
                    if (t1.type == CELLA_TokenType::CONST && t1.valueType == CELLA_TokenValueType::NUMBER &&
                        t1.lexeme.find('.') == std::string::npos)
                    {
                        st.hasPage = true;
                        st.pageNo = static_cast<long long>(t1.numValue);
                        advance();
                    }
                    else
                    {
                        synError(t1, "正整数（页码）");
                        return false;
                    }
                    if (matchDelim(","))
                    {
                        const CELLA_Token &t2 = peek();
                        if (t2.type == CELLA_TokenType::CONST && t2.valueType == CELLA_TokenValueType::NUMBER &&
                            t2.lexeme.find('.') == std::string::npos)
                        {
                            st.pageSize = static_cast<long long>(t2.numValue);
                            advance();
                        }
                        else
                        {
                            synError(t2, "正整数（每页行数）");
                            return false;
                        }
                    }
                }
                if (matchKw(CELLA_Keyword::UNION))
                {
                    const CELLA_Token &t = toks[pos - 1];
                    auto arm = std::make_unique<CELLA_Stmt>();
                    arm->kind = CELLA_Stmt::Kind::GET;
                    arm->line = t.line;
                    arm->col = t.col;
                    // 文法: union get_stmt —— 右臂本身是完整 get 语句，须以 get 开头
                    if (!expectKw(CELLA_Keyword::GET))
                        return false;
                    if (!parseGetBody(*arm))
                        return false;
                    st.unionQuery = std::move(arm);
                }
                return true;
            }

            bool parseTableRef(CELLA_TableRef &ref)
            {
                const CELLA_Token &t = peek();
                if (t.type != CELLA_TokenType::IDENTIFIER)
                {
                    synError(t, "表名");
                    return false;
                }
                advance();
                ref.name = t.lexeme;
                ref.line = t.line;
                ref.col = t.col;
                if (matchKw(CELLA_Keyword::AS))
                {
                    if (!expectIdent(ref.alias))
                        return false;
                }
                else if (peek().type == CELLA_TokenType::IDENTIFIER)
                {
                    ref.alias = peek().lexeme;
                    advance();
                }
                return true;
            }

            bool parseColName(CELLA_ColName &cn)
            {
                const CELLA_Token &t = peek();
                if (t.type != CELLA_TokenType::IDENTIFIER)
                {
                    synError(t, "列名");
                    return false;
                }
                advance();
                cn.column = t.lexeme;
                cn.line = t.line;
                cn.col = t.col;
                if (matchOp("."))
                {
                    cn.table = cn.column;
                    if (!expectIdent(cn.column))
                        return false;
                }
                return true;
            }

            // DELETE in table_name [ LIMIT expr ] ';'（FROM → in，WHERE → limit）
            std::unique_ptr<CELLA_Stmt> parseDelete()
            {
                const CELLA_Token &t = advance(); // DELETE
                auto st = makeStmt(CELLA_Stmt::Kind::DELETE, t);
                if (!expectKw(CELLA_Keyword::IN))
                    return nullptr;
                if (!expectIdent(st->tableName))
                    return nullptr;
                if (matchKw(CELLA_Keyword::LIMIT))
                {
                    st->where = parseExpr();
                    if (!st->where)
                        return nullptr;
                }
                if (!expectSemicolon())
                    return nullptr;
                return st;
            }

            // UPDATE name SET col '=' expr { ',' col '=' expr } [ LIMIT expr ] ';'
            // 已彻底移除 WHERE，条件用 limit
            std::unique_ptr<CELLA_Stmt> parseUpdate()
            {
                const CELLA_Token &t = advance(); // UPDATE
                auto st = makeStmt(CELLA_Stmt::Kind::UPDATE, t);
                if (!expectIdent(st->tableName))
                    return nullptr;
                if (!expectKw(CELLA_Keyword::SET))
                    return nullptr;
                for (;;)
                {
                    CELLA_ColName cn;
                    if (!parseColName(cn))
                        return nullptr;
                    if (!expectOp("="))
                        return nullptr;
                    auto e = parseExpr();
                    if (!e)
                        return nullptr;
                    st->sets.push_back({cn.column, std::move(e)});
                    if (matchDelim(","))
                        continue;
                    break;
                }
                if (matchKw(CELLA_Keyword::LIMIT))
                {
                    st->where = parseExpr();
                    if (!st->where)
                        return nullptr;
                }
                if (!expectSemicolon())
                    return nullptr;
                return st;
            }

            // DROP TABLE name ';'
            std::unique_ptr<CELLA_Stmt> parseDropTable()
            {
                const CELLA_Token &t = advance(); // DROP
                auto st = makeStmt(CELLA_Stmt::Kind::DROP_TABLE, t);
                if (!expectKw(CELLA_Keyword::TABLE))
                    return nullptr;
                if (!expectIdent(st->tableName))
                    return nullptr;
                if (!expectSemicolon())
                    return nullptr;
                return st;
            }

            // ---------------- 表达式（任务书 2.4 节） ----------------

            std::unique_ptr<CELLA_Expr> parseExpr() { return parseOr(); }

            std::unique_ptr<CELLA_Expr> parseOr()
            {
                auto l = parseAnd();
                if (!l)
                    return nullptr;
                while (matchKw(CELLA_Keyword::OR))
                {
                    const CELLA_Token &op = toks[pos - 1];
                    auto r = parseAnd();
                    if (!r)
                        return nullptr;
                    l = makeBinary(CELLA_Expr::BinOp::OR, op, std::move(l), std::move(r));
                }
                return l;
            }

            std::unique_ptr<CELLA_Expr> parseAnd()
            {
                auto l = parseNot();
                if (!l)
                    return nullptr;
                while (matchKw(CELLA_Keyword::AND))
                {
                    const CELLA_Token &op = toks[pos - 1];
                    auto r = parseNot();
                    if (!r)
                        return nullptr;
                    l = makeBinary(CELLA_Expr::BinOp::AND, op, std::move(l), std::move(r));
                }
                return l;
            }

            std::unique_ptr<CELLA_Expr> parseNot()
            {
                if (matchKw(CELLA_Keyword::NOT))
                {
                    const CELLA_Token &op = toks[pos - 1];
                    auto c = parseNot();
                    if (!c)
                        return nullptr;
                    return makeUnary(CELLA_Expr::UnOp::NOT, op, std::move(c));
                }
                return parseComparison();
            }

            std::unique_ptr<CELLA_Expr> parseComparison()
            {
                auto l = parseAdd();
                if (!l)
                    return nullptr;
                const CELLA_Token &t = peek();
                CELLA_Expr::BinOp op = CELLA_Expr::BinOp::EQ;
                bool hasOp = false;
                if (t.type == CELLA_TokenType::OPERATOR)
                {
                    if (t.lexeme == "=" || t.lexeme == "==")
                    {
                        op = CELLA_Expr::BinOp::EQ;
                        hasOp = true;
                    }
                    else if (t.lexeme == "!=" || t.lexeme == "<>")
                    {
                        op = CELLA_Expr::BinOp::NE;
                        hasOp = true;
                    }
                    else if (t.lexeme == "<")
                    {
                        op = CELLA_Expr::BinOp::LT;
                        hasOp = true;
                    }
                    else if (t.lexeme == "<=")
                    {
                        op = CELLA_Expr::BinOp::LE;
                        hasOp = true;
                    }
                    else if (t.lexeme == ">")
                    {
                        op = CELLA_Expr::BinOp::GT;
                        hasOp = true;
                    }
                    else if (t.lexeme == ">=")
                    {
                        op = CELLA_Expr::BinOp::GE;
                        hasOp = true;
                    }
                }
                if (!hasOp)
                    return l;
                advance();
                auto r = parseAdd();
                if (!r)
                    return nullptr;
                return makeBinary(op, t, std::move(l), std::move(r));
            }

            std::unique_ptr<CELLA_Expr> parseAdd()
            {
                auto l = parseMul();
                if (!l)
                    return nullptr;
                for (;;)
                {
                    const CELLA_Token &t = peek();
                    CELLA_Expr::BinOp op;
                    if (t.type == CELLA_TokenType::OPERATOR && t.lexeme == "+")
                        op = CELLA_Expr::BinOp::PLUS;
                    else if (t.type == CELLA_TokenType::OPERATOR && t.lexeme == "-")
                        op = CELLA_Expr::BinOp::MINUS;
                    else
                        return l;
                    advance();
                    auto r = parseMul();
                    if (!r)
                        return nullptr;
                    l = makeBinary(op, t, std::move(l), std::move(r));
                }
            }

            std::unique_ptr<CELLA_Expr> parseMul()
            {
                auto l = parseUnary();
                if (!l)
                    return nullptr;
                for (;;)
                {
                    const CELLA_Token &t = peek();
                    CELLA_Expr::BinOp op;
                    if (t.type == CELLA_TokenType::OPERATOR && t.lexeme == "*")
                        op = CELLA_Expr::BinOp::MUL;
                    else if (t.type == CELLA_TokenType::OPERATOR && t.lexeme == "/")
                        op = CELLA_Expr::BinOp::DIV;
                    else
                        return l;
                    advance();
                    auto r = parseUnary();
                    if (!r)
                        return nullptr;
                    l = makeBinary(op, t, std::move(l), std::move(r));
                }
            }

            std::unique_ptr<CELLA_Expr> parseUnary()
            {
                if (matchOp("-"))
                {
                    const CELLA_Token &op = toks[pos - 1];
                    auto c = parseUnary();
                    if (!c)
                        return nullptr;
                    return makeUnary(CELLA_Expr::UnOp::NEG, op, std::move(c));
                }
                return parsePrimary();
            }

            std::unique_ptr<CELLA_Expr> parsePrimary()
            {
                const CELLA_Token &t = peek();
                if (t.state != CELLA_TokenState::VALID)
                {
                    synError(t, "合法 Token（前一词法错误导致）");
                    return nullptr;
                }
                if (t.type == CELLA_TokenType::CONST ||
                    (t.type == CELLA_TokenType::KEYWORD &&
                     (t.keyword == CELLA_KW_NULL || t.keyword == CELLA_Keyword::TRUE ||
                      t.keyword == CELLA_Keyword::FALSE)))
                {
                    return parseLiteral();
                }
                if (t.type == CELLA_TokenType::IDENTIFIER)
                {
                    auto e = std::make_unique<CELLA_Expr>();
                    e->kind = CELLA_Expr::Kind::COLUMN_REF;
                    e->line = t.line;
                    e->col = t.col;
                    e->column = t.lexeme;
                    advance();
                    if (matchOp("."))
                    {
                        e->table = e->column;
                        if (!expectIdent(e->column))
                            return nullptr;
                    }
                    return e;
                }
                if (t.type == CELLA_TokenType::DELIMITER && t.lexeme == "(")
                {
                    advance();
                    auto e = parseExpr();
                    if (!e)
                        return nullptr;
                    if (!expectDelim(")"))
                        return nullptr;
                    return e;
                }
                synError(t, "表达式");
                return nullptr;
            }

            // 常量：NUMBER | STRING | DATE | NULL | TRUE | FALSE
            std::unique_ptr<CELLA_Expr> parseLiteral()
            {
                const CELLA_Token &t = peek();
                if (t.state != CELLA_TokenState::VALID)
                {
                    synError(t, "合法常量（前一词法错误导致）");
                    return nullptr;
                }
                auto e = std::make_unique<CELLA_Expr>();
                e->kind = CELLA_Expr::Kind::LITERAL;
                e->line = t.line;
                e->col = t.col;
                if (t.type == CELLA_TokenType::CONST)
                {
                    switch (t.valueType)
                    {
                    case CELLA_TokenValueType::NUMBER:
                        e->lit = CELLA_LiteralKind::NUMBER;
                        e->text = t.textValue;
                        e->num = t.numValue;
                        break;
                    case CELLA_TokenValueType::DATE:
                        e->lit = CELLA_LiteralKind::DATE;
                        e->text = t.textValue;
                        break;
                    case CELLA_TokenValueType::STRING:
                        e->lit = CELLA_LiteralKind::STRING;
                        e->text = t.textValue;
                        break;
                    default:
                        synError(t, "常量");
                        return nullptr;
                    }
                    advance();
                    return e;
                }
                if (t.type == CELLA_TokenType::KEYWORD)
                {
                    if (t.keyword == CELLA_KW_NULL)
                    {
                        e->lit = CELLA_LiteralKind::NULL_LIT;
                        advance();
                        return e;
                    }
                    if (t.keyword == CELLA_Keyword::TRUE)
                    {
                        e->lit = CELLA_LiteralKind::BOOL_LIT;
                        e->boolVal = true;
                        advance();
                        return e;
                    }
                    if (t.keyword == CELLA_Keyword::FALSE)
                    {
                        e->lit = CELLA_LiteralKind::BOOL_LIT;
                        e->boolVal = false;
                        advance();
                        return e;
                    }
                }
                synError(t, "常量（数字/字符串/日期/NULL/TRUE/FALSE）");
                return nullptr;
            }
        };

    } // namespace

    std::unique_ptr<CELLA_Program> cella_parse(const std::vector<CELLA_Token> &tokens,
                                               std::vector<CELLA_Error> &errors)
    {
        CELLA_ParserImpl parser(tokens, errors);
        return parser.parseProgram();
    }

} // namespace cella
