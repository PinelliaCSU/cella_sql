// cella_semantic.cpp —— 语义分析：基于内存 Catalog 检查表/列/类型一致性（任务书 3.3 节）。
// 每条语句独立检查：出错输出 SEM-3xx 诊断并标记该语句失败，继续处理后续语句；
// CREATE TABLE / DROP TABLE 成功时更新 Catalog。
#include "cella/cella_semantic.h"

#include <set>
#include <string>
#include <vector>

namespace cella
{
    namespace
    {

        // 查询作用域：from 表与各 join 表（支持别名）
        struct CELLA_ScopeEntry
        {
            std::string name;  // 表名（原始拼写）
            std::string alias; // 可空
        };

        const CELLA_Table *entryTable(const CELLA_Catalog &cat, const CELLA_ScopeEntry &e)
        {
            return cat.findTable(e.name);
        }

        bool tableHasColumn(const CELLA_Table &t, const std::string &name)
        {
            std::string key = cella_toUpper(name);
            for (const auto &c : t.columns)
            {
                if (cella_toUpper(c.name) == key)
                    return true;
            }
            return false;
        }

        // ---------------- 表达式类型系统（任务书 v2 14.1 / guide 5.3） ----------------

        // 表达式值类型分类
        enum class CELLA_ValueType
        {
            INT,
            FLOAT,
            DOUBLE,
            CHAR,
            VARCHAR,
            TEXT,
            DATE,
            TIME,
            DATETIME,
            BOOL,
            NULL_T,
            UNKNOWN
        };

        bool isNumericType(CELLA_ValueType t)
        {
            return t == CELLA_ValueType::INT || t == CELLA_ValueType::FLOAT || t == CELLA_ValueType::DOUBLE;
        }
        bool isStringType(CELLA_ValueType t)
        {
            return t == CELLA_ValueType::CHAR || t == CELLA_ValueType::VARCHAR || t == CELLA_ValueType::TEXT;
        }
        bool isDateTimeType(CELLA_ValueType t)
        {
            return t == CELLA_ValueType::DATE || t == CELLA_ValueType::TIME || t == CELLA_ValueType::DATETIME;
        }

        std::string valueTypeName(CELLA_ValueType t)
        {
            switch (t)
            {
            case CELLA_ValueType::INT:
                return "INT";
            case CELLA_ValueType::FLOAT:
                return "FLOAT";
            case CELLA_ValueType::DOUBLE:
                return "DOUBLE";
            case CELLA_ValueType::CHAR:
                return "CHAR";
            case CELLA_ValueType::VARCHAR:
                return "VARCHAR";
            case CELLA_ValueType::TEXT:
                return "TEXT";
            case CELLA_ValueType::DATE:
                return "DATE";
            case CELLA_ValueType::TIME:
                return "TIME";
            case CELLA_ValueType::DATETIME:
                return "DATETIME";
            case CELLA_ValueType::BOOL:
                return "BOOL";
            case CELLA_ValueType::NULL_T:
                return "NULL";
            case CELLA_ValueType::UNKNOWN:
                return "UNKNOWN";
            }
            return "?";
        }

        CELLA_ValueType dataTypeToValue(CELLA_DataType t)
        {
            switch (t)
            {
            case CELLA_DataType::INT:
                return CELLA_ValueType::INT;
            case CELLA_DataType::FLOAT:
                return CELLA_ValueType::FLOAT;
            case CELLA_DataType::DOUBLE:
                return CELLA_ValueType::DOUBLE;
            case CELLA_DataType::CHAR:
                return CELLA_ValueType::CHAR;
            case CELLA_DataType::VARCHAR:
                return CELLA_ValueType::VARCHAR;
            case CELLA_DataType::TEXT:
                return CELLA_ValueType::TEXT;
            case CELLA_DataType::DATE:
                return CELLA_ValueType::DATE;
            case CELLA_DataType::TIME:
                return CELLA_ValueType::TIME;
            case CELLA_DataType::DATETIME:
                return CELLA_ValueType::DATETIME;
            }
            return CELLA_ValueType::UNKNOWN;
        }

        CELLA_ValueType promoteNumeric(CELLA_ValueType a, CELLA_ValueType b)
        {
            if (a == CELLA_ValueType::DOUBLE || b == CELLA_ValueType::DOUBLE)
                return CELLA_ValueType::DOUBLE;
            if (a == CELLA_ValueType::FLOAT || b == CELLA_ValueType::FLOAT)
                return CELLA_ValueType::FLOAT;
            return CELLA_ValueType::INT;
        }

        std::string binOpText(CELLA_Expr::BinOp op)
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

        // 在作用域内查找列定义（用于取列类型；歧义已在 resolveColumn 中报错）
        const CELLA_Column *scopeFindColumn(const std::vector<CELLA_ScopeEntry> &scope,
                                            const CELLA_Catalog &cat, const std::string &table,
                                            const std::string &column)
        {
            if (!table.empty())
            {
                for (const auto &se : scope)
                {
                    if (cella_toUpper(se.alias) == cella_toUpper(table) ||
                        cella_toUpper(se.name) == cella_toUpper(table))
                    {
                        const CELLA_Table *t = entryTable(cat, se);
                        return t ? CELLA_Catalog::findColumn(*t, column) : nullptr;
                    }
                }
                return nullptr;
            }
            for (const auto &se : scope)
            {
                const CELLA_Table *t = entryTable(cat, se);
                if (!t)
                    continue;
                const CELLA_Column *c = CELLA_Catalog::findColumn(*t, column);
                if (c)
                    return c;
            }
            return nullptr;
        }

        // 解析（可能带限定的）列引用；line/col 用于错误定位
        bool resolveColumn(const std::string &table, const std::string &column, int line, int col,
                           const std::vector<CELLA_ScopeEntry> &scope, const CELLA_Catalog &cat,
                           std::vector<CELLA_Error> &errors)
        {
            if (table.empty())
            {
                const CELLA_Table *found = nullptr;
                for (const auto &se : scope)
                {
                    const CELLA_Table *t = entryTable(cat, se);
                    if (t && tableHasColumn(*t, column))
                    {
                        if (found && found != t)
                        {
                            errors.push_back(cella_makeError(
                                CELLA_Phase::SEM, "SEM-308", line, col,
                                "列 \"" + column + "\" 不明确（表 \"" + found->name + "\" 与表 \"" +
                                    t->name + "\" 均含此列），请用 表名.列名 限定"));
                            return false;
                        }
                        found = t;
                    }
                }
                if (!found)
                {
                    std::string msg;
                    if (scope.size() == 1)
                    {
                        msg = "列 \"" + column + "\" 不存在于表 \"" + scope[0].name + "\"";
                    }
                    else
                    {
                        msg = "列 \"" + column + "\" 不存在于查询涉及的表";
                    }
                    errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-303", line, col, msg));
                    return false;
                }
                return true;
            }

            const CELLA_Table *target = nullptr;
            for (const auto &se : scope)
            {
                if (cella_toUpper(se.alias) == cella_toUpper(table) ||
                    cella_toUpper(se.name) == cella_toUpper(table))
                {
                    target = entryTable(cat, se);
                    break;
                }
            }
            if (!target)
            {
                errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-303", line, col,
                                                 "列 \"" + table + "." + column + "\" 引用的表 \"" + table +
                                                     "\" 不在查询范围"));
                return false;
            }
            if (!tableHasColumn(*target, column))
            {
                errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-303", line, col,
                                                 "列 \"" + column + "\" 不存在于表 \"" + target->name + "\""));
                return false;
            }
            return true;
        }

        // 存在性 + 类型检查；成功时通过 outType 输出表达式类型
        bool resolveExpr(const CELLA_Expr &e, const std::vector<CELLA_ScopeEntry> &scope,
                         const CELLA_Catalog &cat, std::vector<CELLA_Error> &errors,
                         CELLA_ValueType *outType = nullptr)
        {
            CELLA_ValueType t = CELLA_ValueType::UNKNOWN;
            switch (e.kind)
            {
            case CELLA_Expr::Kind::LITERAL:
                switch (e.lit)
                {
                case CELLA_LiteralKind::NUMBER:
                    t = e.text.find('.') != std::string::npos ? CELLA_ValueType::FLOAT
                                                              : CELLA_ValueType::INT;
                    break;
                case CELLA_LiteralKind::STRING:
                    t = CELLA_ValueType::VARCHAR;
                    break;
                case CELLA_LiteralKind::DATE:
                    t = CELLA_ValueType::DATE;
                    break;
                case CELLA_LiteralKind::NULL_LIT:
                    t = CELLA_ValueType::NULL_T;
                    break;
                case CELLA_LiteralKind::BOOL_LIT:
                    t = CELLA_ValueType::BOOL;
                    break;
                }
                break;
            case CELLA_Expr::Kind::COLUMN_REF:
            {
                if (!resolveColumn(e.table, e.column, e.line, e.col, scope, cat, errors))
                    return false;
                const CELLA_Column *c = scopeFindColumn(scope, cat, e.table, e.column);
                t = c ? dataTypeToValue(c->type) : CELLA_ValueType::UNKNOWN;
                break;
            }
            case CELLA_Expr::Kind::UNARY:
            {
                CELLA_ValueType ct;
                if (!resolveExpr(*e.child, scope, cat, errors, &ct))
                    return false;
                if (e.uop == CELLA_Expr::UnOp::NEG)
                {
                    if (!isNumericType(ct))
                    {
                        errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-309", e.line, e.col,
                                                         "一元操作符 '-' 不能应用于 " + valueTypeName(ct)));
                        return false;
                    }
                    t = ct;
                }
                else // NOT
                {
                    if (ct != CELLA_ValueType::BOOL)
                    {
                        errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-309", e.line, e.col,
                                                         "操作符 'NOT' 需要 BOOL 操作数，实际为 " +
                                                             valueTypeName(ct)));
                        return false;
                    }
                    t = CELLA_ValueType::BOOL;
                }
                break;
            }
            case CELLA_Expr::Kind::BINARY:
            {
                CELLA_ValueType lt, rt;
                if (!resolveExpr(*e.left, scope, cat, errors, &lt) ||
                    !resolveExpr(*e.right, scope, cat, errors, &rt))
                    return false;
                switch (e.bop)
                {
                case CELLA_Expr::BinOp::AND:
                case CELLA_Expr::BinOp::OR:
                    if (lt != CELLA_ValueType::BOOL || rt != CELLA_ValueType::BOOL)
                    {
                        errors.push_back(cella_makeError(
                            CELLA_Phase::SEM, "SEM-309", e.line, e.col,
                            "操作符 '" + binOpText(e.bop) + "' 需要 BOOL 操作数，实际为 " +
                                valueTypeName(lt) + " 与 " + valueTypeName(rt)));
                        return false;
                    }
                    t = CELLA_ValueType::BOOL;
                    break;
                case CELLA_Expr::BinOp::PLUS:
                case CELLA_Expr::BinOp::MINUS:
                case CELLA_Expr::BinOp::MUL:
                case CELLA_Expr::BinOp::DIV:
                    if (!isNumericType(lt) || !isNumericType(rt))
                    {
                        errors.push_back(cella_makeError(
                            CELLA_Phase::SEM, "SEM-309", e.line, e.col,
                            "操作符 '" + binOpText(e.bop) + "' 不能应用于 " + valueTypeName(lt) +
                                " 与 " + valueTypeName(rt)));
                        return false;
                    }
                    t = promoteNumeric(lt, rt);
                    break;
                default: // 比较运算
                {
                    bool ok = (isNumericType(lt) && isNumericType(rt)) ||
                              (isStringType(lt) && isStringType(rt)) ||
                              (isDateTimeType(lt) && isDateTimeType(rt)) ||
                              (lt == CELLA_ValueType::BOOL && rt == CELLA_ValueType::BOOL) ||
                              lt == CELLA_ValueType::NULL_T || rt == CELLA_ValueType::NULL_T;
                    if (!ok)
                    {
                        errors.push_back(cella_makeError(
                            CELLA_Phase::SEM, "SEM-309", e.line, e.col,
                            "操作符 '" + binOpText(e.bop) + "' 不能应用于 " + valueTypeName(lt) +
                                " 与 " + valueTypeName(rt)));
                        return false;
                    }
                    t = CELLA_ValueType::BOOL;
                    break;
                }
                }
                break;
            }
            }
            if (outType)
                *outType = t;
            return true;
        }

        // 条件表达式检查：类型必须为 BOOL（limit/on/having/WHERE）
        bool checkBoolCondition(const CELLA_Expr &e, const std::vector<CELLA_ScopeEntry> &scope,
                                const CELLA_Catalog &cat, std::vector<CELLA_Error> &errors)
        {
            CELLA_ValueType t;
            if (!resolveExpr(e, scope, cat, errors, &t))
                return false;
            if (t != CELLA_ValueType::BOOL)
            {
                errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-310", e.line, e.col,
                                                 "条件表达式必须为 BOOL 类型，实际为 " + valueTypeName(t)));
                return false;
            }
            return true;
        }

        bool resolveColName(const CELLA_ColName &cn, const std::vector<CELLA_ScopeEntry> &scope,
                            const CELLA_Catalog &cat, std::vector<CELLA_Error> &errors)
        {
            return resolveColumn(cn.table, cn.column, cn.line, cn.col, scope, cat, errors);
        }

        std::string valueDisplay(const CELLA_Expr &e)
        {
            switch (e.lit)
            {
            case CELLA_LiteralKind::NUMBER:
                return e.text;
            case CELLA_LiteralKind::STRING:
                return "'" + e.text + "'";
            case CELLA_LiteralKind::DATE:
                return "'" + e.text + "'";
            case CELLA_LiteralKind::NULL_LIT:
                return "NULL";
            case CELLA_LiteralKind::BOOL_LIT:
                return e.boolVal ? "TRUE" : "FALSE";
            }
            return "?";
        }

        std::string valueTypeLabel(const CELLA_Expr &e)
        {
            switch (e.lit)
            {
            case CELLA_LiteralKind::NUMBER:
                return "NUMBER";
            case CELLA_LiteralKind::STRING:
                return "STRING";
            case CELLA_LiteralKind::DATE:
                return "DATE";
            case CELLA_LiteralKind::NULL_LIT:
                return "NULL";
            case CELLA_LiteralKind::BOOL_LIT:
                return "BOOLEAN";
            }
            return "?";
        }

        // 常量能否赋给目标列类型（简化规则见 README 实现决策）
        bool checkValueType(const CELLA_Expr &e, const CELLA_Column &col, std::vector<CELLA_Error> &errors)
        {
            bool ok = false;
            switch (e.lit)
            {
            case CELLA_LiteralKind::NUMBER:
                ok = col.type == CELLA_DataType::INT || col.type == CELLA_DataType::FLOAT ||
                     col.type == CELLA_DataType::DOUBLE;
                break;
            case CELLA_LiteralKind::STRING:
                ok = col.type == CELLA_DataType::CHAR || col.type == CELLA_DataType::VARCHAR ||
                     col.type == CELLA_DataType::TEXT || col.type == CELLA_DataType::TIME ||
                     col.type == CELLA_DataType::DATETIME;
                break;
            case CELLA_LiteralKind::DATE:
                ok = col.type == CELLA_DataType::DATE;
                break;
            case CELLA_LiteralKind::NULL_LIT:
                if (col.notNull)
                {
                    errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-307", e.line, e.col,
                                                     "不能向 NOT NULL 列 \"" + col.name + "\" 插入 NULL"));
                    return false;
                }
                return true;
            case CELLA_LiteralKind::BOOL_LIT:
                ok = false;
                break;
            }
            if (!ok)
            {
                errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-306", e.line, e.col,
                                                 "常量 " + valueDisplay(e) + "(" + valueTypeLabel(e) +
                                                     ") 与列 " + col.name + "(" + cella_typeName(col.type) +
                                                     ") 类型不匹配"));
                return false;
            }
            return true;
        }

        std::string joinComma(const std::vector<std::string> &v)
        {
            std::string s;
            for (size_t i = 0; i < v.size(); i++)
            {
                if (i)
                    s += ", ";
                s += v[i];
            }
            return s;
        }

        // ---------------- 各语句 ----------------

        bool semCreateTable(const CELLA_Stmt &st, CELLA_Catalog &cat, int idx, CELLA_SemanticResult &res)
        {
            if (cat.findTable(st.tableName))
            {
                res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-302", st.line, st.col,
                                                     "表 \"" + st.tableName + "\" 已存在，重复定义"));
                return false;
            }
            std::set<std::string> seen;
            for (const auto &cd : st.columns)
            {
                std::string key = cella_toUpper(cd.name);
                if (!seen.insert(key).second)
                {
                    res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-304", cd.line, cd.col,
                                                         "列 \"" + cd.name + "\" 在表 \"" + st.tableName +
                                                             "\" 中重复定义"));
                    return false;
                }
            }
            CELLA_Table table;
            table.name = st.tableName;
            for (const auto &cd : st.columns)
            {
                CELLA_Column c;
                c.name = cd.name;
                c.type = cd.type;
                c.notNull = cd.notNull;
                c.len = cd.hasLen ? cd.len : ((cd.type == CELLA_DataType::CHAR || cd.type == CELLA_DataType::VARCHAR) ? 255 : 0);
                table.columns.push_back(std::move(c));
            }
            cat.addTable(std::move(table));

            std::vector<std::string> names;
            for (const auto &cd : st.columns)
                names.push_back(cd.name);
            res.okMessages.push_back("[语义] OK: 语句#" + std::to_string(idx) + " CREATE TABLE " +
                                     st.tableName + "（列: " + joinComma(names) + "）");
            return true;
        }

        bool semDropTable(const CELLA_Stmt &st, CELLA_Catalog &cat, int idx, CELLA_SemanticResult &res)
        {
            if (!cat.findTable(st.tableName))
            {
                res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-301", st.line, st.col,
                                                     "表 \"" + st.tableName + "\" 不存在"));
                return false;
            }
            cat.dropTable(st.tableName);
            res.okMessages.push_back("[语义] OK: 语句#" + std::to_string(idx) + " DROP TABLE " + st.tableName);
            return true;
        }

        bool semInsert(const CELLA_Stmt &st, CELLA_Catalog &cat, int idx, CELLA_SemanticResult &res)
        {
            const CELLA_Table *table = cat.findTable(st.tableName);
            if (!table)
            {
                res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-301", st.line, st.col,
                                                     "表 \"" + st.tableName + "\" 不存在"));
                return false;
            }

            // 目标列：有列清单按列清单，否则按建表列序取全部列
            std::vector<const CELLA_Column *> targets;
            if (!st.insertColumns.empty())
            {
                for (const auto &name : st.insertColumns)
                {
                    const CELLA_Column *c = CELLA_Catalog::findColumn(*table, name);
                    if (!c)
                    {
                        res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-303", st.line, st.col,
                                                             "列 \"" + name + "\" 不存在于表 \"" +
                                                                 st.tableName + "\""));
                        return false;
                    }
                    targets.push_back(c);
                }
            }
            else
            {
                for (const auto &c : table->columns)
                    targets.push_back(&c);
            }

            for (const auto &row : st.rows)
            {
                if (row.size() != targets.size())
                {
                    res.errors.push_back(cella_makeError(
                        CELLA_Phase::SEM, "SEM-305", st.line, st.col,
                        "INSERT 值个数(" + std::to_string(row.size()) + ") 与列个数(" +
                            std::to_string(targets.size()) + ") 不一致"));
                    return false;
                }
                for (size_t i = 0; i < row.size(); i++)
                {
                    if (!checkValueType(*row[i], *targets[i], res.errors))
                        return false;
                }
            }
            // 记录实际目标列（省略列清单时展开为全部列），供计划阶段使用
            std::vector<std::string> names;
            for (const CELLA_Column *c : targets)
                names.push_back(c->name);
            if (idx > 0 && static_cast<size_t>(idx) <= res.insertColumns.size())
                res.insertColumns[idx - 1] = std::move(names);
            res.okMessages.push_back("[语义] OK: 语句#" + std::to_string(idx) + " INSERT INTO " +
                                     st.tableName);
            return true;
        }

        bool semDelete(const CELLA_Stmt &st, CELLA_Catalog &cat, int idx, CELLA_SemanticResult &res)
        {
            const CELLA_Table *table = cat.findTable(st.tableName);
            if (!table)
            {
                res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-301", st.line, st.col,
                                                     "表 \"" + st.tableName + "\" 不存在"));
                return false;
            }
            if (st.where)
            {
                std::vector<CELLA_ScopeEntry> scope{{st.tableName, ""}};
                if (!checkBoolCondition(*st.where, scope, cat, res.errors))
                    return false;
            }
            res.okMessages.push_back("[语义] OK: 语句#" + std::to_string(idx) + " DELETE " +
                                     st.tableName);
            return true;
        }

        bool semUpdate(const CELLA_Stmt &st, CELLA_Catalog &cat, int idx, CELLA_SemanticResult &res)
        {
            const CELLA_Table *table = cat.findTable(st.tableName);
            if (!table)
            {
                res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-301", st.line, st.col,
                                                     "表 \"" + st.tableName + "\" 不存在"));
                return false;
            }
            std::vector<CELLA_ScopeEntry> scope{{st.tableName, ""}};
            for (const auto &s : st.sets)
            {
                const CELLA_Column *col = CELLA_Catalog::findColumn(*table, s.first);
                if (!col)
                {
                    res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-303", st.line, st.col,
                                                         "列 \"" + s.first + "\" 不存在于表 \"" +
                                                             st.tableName + "\""));
                    return false;
                }
                // 表达式内部类型检查（列存在性 + 运算类型规则）
                CELLA_ValueType et;
                if (!resolveExpr(*s.second, scope, cat, res.errors, &et))
                    return false;
                if (s.second->kind == CELLA_Expr::Kind::LITERAL)
                {
                    if (!checkValueType(*s.second, *col, res.errors))
                        return false;
                }
                else
                {
                    CELLA_ValueType ct = dataTypeToValue(col->type);
                    bool ok = isNumericType(et) ? isNumericType(ct)
                                                : (isStringType(et) ? isStringType(ct) : et == ct);
                    if (!ok)
                    {
                        res.errors.push_back(cella_makeError(
                            CELLA_Phase::SEM, "SEM-306", s.second->line, s.second->col,
                            "表达式(" + valueTypeName(et) + ") 与列 " + col->name + "(" +
                                cella_typeName(col->type) + ") 类型不匹配"));
                        return false;
                    }
                }
            }
            if (st.where)
            {
                if (!checkBoolCondition(*st.where, scope, cat, res.errors))
                    return false;
            }
            res.okMessages.push_back("[语义] OK: 语句#" + std::to_string(idx) + " UPDATE " + st.tableName);
            return true;
        }

        bool semGet(const CELLA_Stmt &st, CELLA_Catalog &cat, int idx, CELLA_SemanticResult &res,
                    bool topLevel)
        {
            std::vector<CELLA_ScopeEntry> scope;
            const CELLA_Table *t = cat.findTable(st.from.name);
            if (!t)
            {
                res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-301", st.from.line, st.from.col,
                                                     "表 \"" + st.from.name + "\" 不存在"));
                return false;
            }
            scope.push_back(CELLA_ScopeEntry{st.from.name, st.from.alias});
            for (const auto &jc : st.joins)
            {
                const CELLA_Table *jt = cat.findTable(jc.ref.name);
                if (!jt)
                {
                    res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-301", jc.ref.line, jc.ref.col,
                                                         "表 \"" + jc.ref.name + "\" 不存在"));
                    return false;
                }
                scope.push_back(CELLA_ScopeEntry{jc.ref.name, jc.ref.alias});
            }
            if (!st.star)
            {
                for (const auto &si : st.selectItems)
                {
                    if (!resolveExpr(*si.expr, scope, cat, res.errors))
                        return false;
                }
            }
            if (st.limit && !checkBoolCondition(*st.limit, scope, cat, res.errors))
                return false;
            for (const auto &jc : st.joins)
            {
                if (!checkBoolCondition(*jc.on, scope, cat, res.errors))
                    return false;
            }
            if (st.having && !checkBoolCondition(*st.having, scope, cat, res.errors))
                return false;
            for (const auto &cn : st.grouped)
            {
                if (!resolveColName(cn, scope, cat, res.errors))
                    return false;
            }
            for (const auto &oi : st.ordered)
            {
                if (!resolveColName(oi.col, scope, cat, res.errors))
                    return false;
            }
            if (st.unionQuery)
            {
                if (!semGet(*st.unionQuery, cat, idx, res, false))
                    return false;
            }
            // 分页参数必须为正整数（页码 1 起，每页行数 ≥ 1）
            if (st.hasPage && (st.pageNo < 1 || st.pageSize < 1))
            {
                res.errors.push_back(cella_makeError(
                    CELLA_Phase::SEM, "SEM-311", st.line, st.col,
                    "分页参数必须为正整数（页码 " + std::to_string(st.pageNo) + "，每页行数 " +
                        std::to_string(st.pageSize) + "）"));
                return false;
            }
            // 分页与 among 冲突校验：among 先限定总行数，page 的起始行不得超出该范围
            if (st.hasPage && st.among >= 0)
            {
                long long offset = (st.pageNo - 1) * st.pageSize;
                if (offset >= st.among)
                {
                    res.errors.push_back(cella_makeError(
                        CELLA_Phase::SEM, "SEM-312", st.line, st.col,
                        "分页与 among 冲突：页码 " + std::to_string(st.pageNo) + "（起始行 " +
                            std::to_string(offset + 1) + "）超出 among 限定的 " + std::to_string(st.among) +
                            " 行"));
                    return false;
                }
            }
            if (topLevel)
            {
                res.okMessages.push_back("[语义] OK: 语句#" + std::to_string(idx) + " get in " +
                                         st.from.name);
            }
            return true;
        }

        bool analyzeStmt(const CELLA_Stmt &st, CELLA_Catalog &cat, int idx, CELLA_SemanticResult &res)
        {
            switch (st.kind)
            {
            case CELLA_Stmt::Kind::CREATE_TABLE:
                return semCreateTable(st, cat, idx, res);
            case CELLA_Stmt::Kind::DROP_TABLE:
                return semDropTable(st, cat, idx, res);
            case CELLA_Stmt::Kind::INSERT:
                return semInsert(st, cat, idx, res);
            case CELLA_Stmt::Kind::DELETE:
                return semDelete(st, cat, idx, res);
            case CELLA_Stmt::Kind::UPDATE:
                return semUpdate(st, cat, idx, res);
            case CELLA_Stmt::Kind::GET:
                return semGet(st, cat, idx, res, true);
            }
            res.errors.push_back(cella_makeError(CELLA_Phase::SEM, "SEM-399", st.line, st.col,
                                                 "未知语句类型"));
            return false;
        }

    } // namespace

    CELLA_SemanticResult cella_analyze(const CELLA_Program &program, CELLA_Catalog &catalog)
    {
        CELLA_SemanticResult result;
        result.stmtOk.assign(program.statements.size(), false);
        result.insertColumns.assign(program.statements.size(), std::vector<std::string>());
        for (size_t i = 0; i < program.statements.size(); i++)
        {
            result.stmtOk[i] = analyzeStmt(*program.statements[i], catalog, static_cast<int>(i + 1),
                                           result);
        }
        return result;
    }

} // namespace cella
