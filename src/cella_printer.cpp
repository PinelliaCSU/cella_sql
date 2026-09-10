// cella_printer.cpp —— 文本输出：Token 流 / AST / 执行计划（任务书 5/7 节格式）。
// 同一输入输出稳定，供 golden 回归。
#include "cella/cella_printer.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace cella
{
    namespace
    {

        // ---------------- 表达式文本 ----------------

        std::string opText(CELLA_Expr::BinOp op)
        {
            switch (op)
            {
            case CELLA_Expr::BinOp::EQ:
                return "=";
            case CELLA_Expr::BinOp::NE:
                return "!=";
            case CELLA_Expr::BinOp::LT:
                return "<";
            case CELLA_Expr::BinOp::LE:
                return "<=";
            case CELLA_Expr::BinOp::GT:
                return ">";
            case CELLA_Expr::BinOp::GE:
                return ">=";
            case CELLA_Expr::BinOp::PLUS:
                return "+";
            case CELLA_Expr::BinOp::MINUS:
                return "-";
            case CELLA_Expr::BinOp::MUL:
                return "*";
            case CELLA_Expr::BinOp::DIV:
                return "/";
            case CELLA_Expr::BinOp::AND:
                return "AND";
            case CELLA_Expr::BinOp::OR:
                return "OR";
            }
            return "?";
        }

        int exprPrec(const CELLA_Expr &e)
        {
            switch (e.kind)
            {
            case CELLA_Expr::Kind::LITERAL:
            case CELLA_Expr::Kind::COLUMN_REF:
                return 7;
            case CELLA_Expr::Kind::UNARY:
                return 6;
            case CELLA_Expr::Kind::BINARY:
                switch (e.bop)
                {
                case CELLA_Expr::BinOp::OR:
                    return 1;
                case CELLA_Expr::BinOp::AND:
                    return 2;
                case CELLA_Expr::BinOp::EQ:
                case CELLA_Expr::BinOp::NE:
                case CELLA_Expr::BinOp::LT:
                case CELLA_Expr::BinOp::LE:
                case CELLA_Expr::BinOp::GT:
                case CELLA_Expr::BinOp::GE:
                    return 3;
                case CELLA_Expr::BinOp::PLUS:
                case CELLA_Expr::BinOp::MINUS:
                    return 4;
                case CELLA_Expr::BinOp::MUL:
                case CELLA_Expr::BinOp::DIV:
                    return 5;
                }
                break;
            }
            return 7;
        }

        std::string quoteEscape(const std::string &s)
        {
            std::string r;
            for (char c : s)
            {
                r += c;
                if (c == '\'')
                    r += '\''; // '' 转义还原
            }
            return r;
        }

        std::string exprToStringMin(const CELLA_Expr &e, int minPrec)
        {
            std::string s;
            switch (e.kind)
            {
            case CELLA_Expr::Kind::LITERAL:
                switch (e.lit)
                {
                case CELLA_LiteralKind::NUMBER:
                    s = e.text;
                    break;
                case CELLA_LiteralKind::STRING:
                    s = "'" + quoteEscape(e.text) + "'";
                    break;
                case CELLA_LiteralKind::DATE:
                    s = "'" + e.text + "'";
                    break;
                case CELLA_LiteralKind::NULL_LIT:
                    s = "NULL";
                    break;
                case CELLA_LiteralKind::BOOL_LIT:
                    s = e.boolVal ? "TRUE" : "FALSE";
                    break;
                }
                break;
            case CELLA_Expr::Kind::COLUMN_REF:
                s = e.table.empty() ? e.column : e.table + "." + e.column;
                break;
            case CELLA_Expr::Kind::UNARY:
            {
                std::string inner = exprToStringMin(*e.child, 6);
                s = (e.uop == CELLA_Expr::UnOp::NEG ? "-" : "NOT ") + inner;
                break;
            }
            case CELLA_Expr::Kind::BINARY:
            {
                int p = exprPrec(e);
                s = exprToStringMin(*e.left, p) + " " + opText(e.bop) + " " +
                    exprToStringMin(*e.right, p + 1);
                break;
            }
            }
            if (exprPrec(e) < minPrec)
                s = "(" + s + ")";
            return s;
        }

        // ---------------- 公共工具 ----------------

        std::string indent(int level) { return std::string(static_cast<size_t>(level) * 2, ' '); }

        std::string columnTypeText(const CELLA_ColumnDef &cd)
        {
            std::string s = cella_typeName(cd.type);
            if (cd.type == CELLA_DataType::CHAR || cd.type == CELLA_DataType::VARCHAR)
            {
                s += "(" + std::to_string(cd.hasLen ? cd.len : 255) + ")";
            }
            if (cd.notNull)
                s += " NOT NULL";
            return s;
        }

        std::string colNameText(const CELLA_ColName &cn)
        {
            return cn.table.empty() ? cn.column : cn.table + "." + cn.column;
        }

        // ---------------- AST 打印 ----------------

        void printStmt(const CELLA_Stmt &st, std::ostream &os, int level);

        void printStmt(const CELLA_Stmt &st, std::ostream &os, int level)
        {
            switch (st.kind)
            {
            case CELLA_Stmt::Kind::CREATE_TABLE:
                os << indent(level) << "CreateTableStmt @" << st.line << ":" << st.col
                   << "  name=" << st.tableName << "\n";
                os << indent(level + 1) << "columns:\n";
                for (const auto &cd : st.columns)
                {
                    os << indent(level + 2) << cd.name << "  " << columnTypeText(cd) << "\n";
                }
                break;
            case CELLA_Stmt::Kind::INSERT:
                os << indent(level) << "InsertStmt @" << st.line << ":" << st.col
                   << "  table=" << st.tableName;
                if (!st.insertColumns.empty())
                {
                    os << "  columns=[";
                    for (size_t i = 0; i < st.insertColumns.size(); i++)
                    {
                        if (i)
                            os << ",";
                        os << st.insertColumns[i];
                    }
                    os << "]";
                }
                os << "\n";
                os << indent(level + 1) << "rows:\n";
                for (const auto &row : st.rows)
                {
                    os << indent(level + 2) << "(";
                    for (size_t i = 0; i < row.size(); i++)
                    {
                        if (i)
                            os << ",";
                        os << cella_exprToString(*row[i]);
                    }
                    os << ")\n";
                }
                break;
            case CELLA_Stmt::Kind::GET:
            {
                os << indent(level) << "GetStmt @" << st.line << ":" << st.col << "  select=[";
                if (st.star)
                {
                    if (st.distinct)
                        os << "DISTINCT ";
                    os << "*";
                }
                else
                {
                    if (st.distinct)
                        os << "DISTINCT ";
                    for (size_t i = 0; i < st.selectItems.size(); i++)
                    {
                        if (i)
                            os << ",";
                        os << cella_exprToString(*st.selectItems[i].expr);
                        if (!st.selectItems[i].alias.empty())
                        {
                            os << " as " << st.selectItems[i].alias;
                        }
                    }
                }
                os << "] from=[" << st.from.name;
                if (!st.from.alias.empty())
                    os << " " << st.from.alias;
                os << "]\n";
                for (const auto &jc : st.joins)
                {
                    std::string kindText = jc.kind == CELLA_JoinKind::MIDDLE
                                               ? "middle"
                                               : (jc.kind == CELLA_JoinKind::LEFT ? "left" : "right");
                    os << indent(level + 1) << "join: " << kindText << " " << jc.ref.name;
                    if (!jc.ref.alias.empty())
                        os << " " << jc.ref.alias;
                    os << " on " << cella_exprToString(*jc.on) << "\n";
                }
                if (st.limit)
                {
                    os << indent(level + 1) << "limit: " << cella_exprToString(*st.limit) << "\n";
                }
                if (!st.grouped.empty())
                {
                    os << indent(level + 1) << "grouped: ";
                    for (size_t i = 0; i < st.grouped.size(); i++)
                    {
                        if (i)
                            os << ", ";
                        os << colNameText(st.grouped[i]);
                    }
                    os << "\n";
                }
                if (st.having)
                {
                    os << indent(level + 1) << "having: " << cella_exprToString(*st.having) << "\n";
                }
                if (!st.ordered.empty())
                {
                    os << indent(level + 1) << "ordered: ";
                    for (size_t i = 0; i < st.ordered.size(); i++)
                    {
                        if (i)
                            os << ", ";
                        os << colNameText(st.ordered[i].col)
                           << (st.ordered[i].asc ? " ASC" : " DESC");
                    }
                    os << "\n";
                }
                if (st.among >= 0)
                {
                    os << indent(level + 1) << "among: " << st.among << "\n";
                }
                if (st.hasPage)
                {
                    os << indent(level + 1) << "page: " << st.pageNo << ", " << st.pageSize << "\n";
                }
                if (st.unionQuery)
                {
                    os << indent(level + 1) << "union:\n";
                    printStmt(*st.unionQuery, os, level + 2);
                }
                break;
            }
            case CELLA_Stmt::Kind::DELETE:
                os << indent(level) << "DeleteStmt @" << st.line << ":" << st.col
                   << "  table=" << st.tableName << "\n";
                if (st.where)
                {
                    os << indent(level + 1) << "limit: " << cella_exprToString(*st.where) << "\n";
                }
                break;
            case CELLA_Stmt::Kind::UPDATE:
                os << indent(level) << "UpdateStmt @" << st.line << ":" << st.col
                   << "  table=" << st.tableName << "\n";
                for (const auto &s : st.sets)
                {
                    os << indent(level + 1) << "set: " << s.first << " = "
                       << cella_exprToString(*s.second) << "\n";
                }
                if (st.where)
                {
                    os << indent(level + 1) << "limit: " << cella_exprToString(*st.where) << "\n";
                }
                break;
            case CELLA_Stmt::Kind::DROP_TABLE:
                os << indent(level) << "DropTableStmt @" << st.line << ":" << st.col
                   << "  table=" << st.tableName << "\n";
                break;
            }
        }

        void printPlanNode(const CELLA_PlanNode &n, std::ostream &os, int level)
        {
            os << indent(level) << n.op;
            if (n.op == "Filter" && n.pred)
                os << " (" << cella_exprToString(*n.pred) << ")";
            else if (n.op == "Join" && n.onExpr)
                os << " (" << n.joinKind << " on " << cella_exprToString(*n.onExpr) << ")";
            else if (!n.detail.empty())
                os << " " << n.detail;
            os << "\n";
            for (const auto &line : n.extra)
                os << indent(level + 1) << line << "\n";
            for (const auto &c : n.children)
                printPlanNode(*c, os, level + 1);
        }

    } // namespace

    std::string cella_exprToString(const CELLA_Expr &e) { return exprToStringMin(e, 0); }

    void cella_printTokens(const std::vector<CELLA_Token> &tokens, std::ostream &os)
    {
        for (const auto &t : tokens)
        {
            if (t.type == CELLA_TokenType::EOF_T)
            {
                os << "EOF\n";
                continue;
            }
            std::string typeText;
            switch (t.type)
            {
            case CELLA_TokenType::UNKNOWN:
                typeText = "UNKNOWN";
                break;
            case CELLA_TokenType::KEYWORD:
                typeText = "KEYWORD";
                break;
            case CELLA_TokenType::IDENTIFIER:
                typeText = "IDENTIFIER";
                break;
            case CELLA_TokenType::CONST:
                typeText = "CONST(" + cella_valueTypeText(t.valueType) + ")";
                break;
            case CELLA_TokenType::OPERATOR:
                typeText = "OPERATOR";
                break;
            case CELLA_TokenType::DELIMITER:
                typeText = "DELIMITER";
                break;
            default:
                typeText = "EOF";
                break;
            }
            std::string lexemeText =
                (t.type == CELLA_TokenType::KEYWORD) ? cella_keywordText(t.keyword) : t.lexeme;
            os << t.line << ":" << t.col << "  " << std::left << std::setw(14) << typeText << " "
               << lexemeText << "\n";
        }
    }

    void cella_printAst(const CELLA_Program &program, std::ostream &os)
    {
        os << "Program\n";
        for (const auto &st : program.statements)
            printStmt(*st, os, 1);
    }

    void cella_printPlan(const std::vector<std::unique_ptr<CELLA_PlanNode>> &plans, std::ostream &os)
    {
        bool first = true;
        for (const auto &p : plans)
        {
            if (!first)
                os << "\n";
            first = false;
            printPlanNode(*p, os, 0);
        }
    }

} // namespace cella
