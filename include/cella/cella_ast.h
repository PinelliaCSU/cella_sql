// cella_ast.h —— AST 定义：语句与表达式节点（任务书 3.2 节）。
// 采用 tagged 结构 + unique_ptr 拥有子节点；每个节点携带源位置(行:列，1 起)。
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cella_catalog.h"

namespace cella
{

    // ---------------- 表达式 ----------------

    enum class CELLA_LiteralKind
    {
        NUMBER,
        STRING,
        DATE,
        NULL_LIT,
        BOOL_LIT
    };

    struct CELLA_Expr
    {
        enum class Kind
        {
            LITERAL,
            COLUMN_REF,
            UNARY,
            BINARY
        };
        enum class UnOp
        {
            NEG,
            NOT
        };
        enum class BinOp
        {
            EQ,
            NE,
            LT,
            LE,
            GT,
            GE,
            PLUS,
            MINUS,
            MUL,
            DIV,
            AND,
            OR
        };

        Kind kind = Kind::LITERAL;
        int line = 0, col = 0;

        // LITERAL
        CELLA_LiteralKind lit = CELLA_LiteralKind::NULL_LIT;
        std::string text;     // 数字规范化文本 / 字符串内容 / 日期文本
        double num = 0;       // NUMBER 数值
        bool boolVal = false; // BOOL_LIT

        // COLUMN_REF
        std::string table; // 可空；非空表示 表名.列名
        std::string column;

        // UNARY / BINARY
        UnOp uop = UnOp::NEG;
        BinOp bop = BinOp::EQ;
        std::unique_ptr<CELLA_Expr> left;  // BINARY 左子树
        std::unique_ptr<CELLA_Expr> right; // BINARY 右子树
        std::unique_ptr<CELLA_Expr> child; // UNARY 子树
    };

    // ---------------- 结构 ----------------

    struct CELLA_ColumnDef
    {
        std::string name;
        CELLA_DataType type = CELLA_DataType::INT;
        int len = 0;
        bool hasLen = false;
        bool notNull = false;
        int line = 0, col = 0;
    };

    struct CELLA_TableRef
    {
        std::string name;
        std::string alias; // 可空
        int line = 0, col = 0;
    };

    enum class CELLA_JoinKind
    {
        MIDDLE,
        LEFT,
        RIGHT
    };

    struct CELLA_JoinClause
    {
        CELLA_JoinKind kind = CELLA_JoinKind::MIDDLE;
        CELLA_TableRef ref;
        std::unique_ptr<CELLA_Expr> on;
    };

    struct CELLA_SelectItem
    {
        std::unique_ptr<CELLA_Expr> expr;
        std::string alias; // 可空
    };

    struct CELLA_ColName
    {
        std::string table; // 可空
        std::string column;
        int line = 0, col = 0;
    };

    struct CELLA_OrderItem
    {
        CELLA_ColName col;
        bool asc = true;
    };

    // 表达式深拷贝（计划/优化阶段需要持有表达式副本）
    inline std::unique_ptr<CELLA_Expr> cella_cloneExpr(const CELLA_Expr &e)
    {
        auto n = std::make_unique<CELLA_Expr>();
        n->kind = e.kind;
        n->line = e.line;
        n->col = e.col;
        n->lit = e.lit;
        n->text = e.text;
        n->num = e.num;
        n->boolVal = e.boolVal;
        n->table = e.table;
        n->column = e.column;
        n->uop = e.uop;
        n->bop = e.bop;
        if (e.left)
            n->left = cella_cloneExpr(*e.left);
        if (e.right)
            n->right = cella_cloneExpr(*e.right);
        if (e.child)
            n->child = cella_cloneExpr(*e.child);
        return n;
    }

    // ---------------- 语句 ----------------

    struct CELLA_Stmt
    {
        enum class Kind
        {
            CREATE_TABLE,
            INSERT,
            GET,
            DELETE,
            UPDATE,
            DROP_TABLE
        };

        Kind kind = Kind::CREATE_TABLE;
        int line = 0, col = 0;

        // CREATE TABLE / DELETE / UPDATE / DROP TABLE 共用
        std::string tableName;

        // CREATE TABLE
        std::vector<CELLA_ColumnDef> columns;

        // INSERT
        std::vector<std::string> insertColumns; // 空 = 省略列清单
        std::vector<std::vector<std::unique_ptr<CELLA_Expr>>> rows;

        // GET
        bool distinct = false;
        bool star = false;
        std::vector<CELLA_SelectItem> selectItems;
        CELLA_TableRef from;
        std::vector<CELLA_JoinClause> joins;
        std::unique_ptr<CELLA_Expr> limit;      // = WHERE
        std::vector<CELLA_ColName> grouped;     // = GROUP BY
        std::unique_ptr<CELLA_Expr> having;     // = HAVING
        std::vector<CELLA_OrderItem> ordered;   // = ORDER BY
        long long among = -1;                   // = LIMIT 行数，-1 表示未指定
        bool hasPage = false;                   // = 分页（页码 1 起，每页行数默认 10）
        long long pageNo = 0;                   // 页码
        long long pageSize = 10;                // 每页行数
        std::unique_ptr<CELLA_Stmt> unionQuery; // UNION 右臂

        // UPDATE
        std::vector<std::pair<std::string, std::unique_ptr<CELLA_Expr>>> sets;

        // DELETE / UPDATE 共用
        std::unique_ptr<CELLA_Expr> where;
    };

    struct CELLA_Program
    {
        std::vector<std::unique_ptr<CELLA_Stmt>> statements;
    };

} // namespace cella
