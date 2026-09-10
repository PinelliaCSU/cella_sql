// cella_planner.cpp —— 执行计划生成：通过语义检查的 AST -> 执行计划树（任务书 3.4 节）。
// 核心算子：CreateTable/Insert/Delete/SeqScan/Filter/Project；
// 扩展算子：Sort/Limit/Aggregate/Join/Union/Distinct/Update/DropTable。
// get * 不生成 Project 节点（见 README 实现决策）。
#include "cella/cella_planner.h"

#include <string>
#include <vector>

#include "cella/cella_printer.h"

namespace cella
{
    namespace
    {

        std::unique_ptr<CELLA_PlanNode> makeNode(std::string op, std::string detail, int line, int col)
        {
            auto n = std::make_unique<CELLA_PlanNode>();
            n->op = std::move(op);
            n->detail = std::move(detail);
            n->line = line;
            n->col = col;
            return n;
        }

        std::unique_ptr<CELLA_PlanNode> wrapNode(std::string op, std::string detail,
                                                 std::unique_ptr<CELLA_PlanNode> child, int line, int col)
        {
            auto n = makeNode(std::move(op), std::move(detail), line, col);
            n->children.push_back(std::move(child));
            return n;
        }

        std::string joinStrs(const std::vector<std::string> &v, const std::string &sep)
        {
            std::string s;
            for (size_t i = 0; i < v.size(); i++)
            {
                if (i)
                    s += sep;
                s += v[i];
            }
            return s;
        }

        std::string tableDisplay(const CELLA_TableRef &r)
        {
            return r.alias.empty() ? r.name : r.name + " " + r.alias;
        }

        std::string colNameDisplay(const CELLA_ColName &cn)
        {
            return cn.table.empty() ? cn.column : cn.table + "." + cn.column;
        }

        // get 查询计划：自下而上 SeqScan -> Join -> Filter(limit) -> Aggregate -> Filter(having)
        //              -> Project -> Distinct -> Sort -> Limit(among) -> Union
        std::unique_ptr<CELLA_PlanNode> buildGet(const CELLA_Stmt &st)
        {
            auto node = makeNode("SeqScan", "[" + tableDisplay(st.from) + "]", st.line, st.col);
            for (const auto &jc : st.joins)
            {
                std::string kindText = jc.kind == CELLA_JoinKind::MIDDLE
                                           ? "middle"
                                           : (jc.kind == CELLA_JoinKind::LEFT ? "left" : "right");
                auto join = makeNode("Join", "", st.line, st.col);
                join->joinKind = kindText;
                join->onExpr = cella_cloneExpr(*jc.on);
                join->children.push_back(std::move(node));
                join->children.push_back(
                    makeNode("SeqScan", "[" + tableDisplay(jc.ref) + "]", jc.ref.line, jc.ref.col));
                node = std::move(join);
            }
            if (st.limit)
            {
                node = wrapNode("Filter", "", std::move(node), st.line, st.col);
                node->pred = cella_cloneExpr(*st.limit);
            }
            if (!st.grouped.empty())
            {
                std::vector<std::string> cols;
                for (const auto &cn : st.grouped)
                    cols.push_back(colNameDisplay(cn));
                node = wrapNode("Aggregate", "(grouped: " + joinStrs(cols, ", ") + ")", std::move(node),
                                st.line, st.col);
            }
            if (st.having)
            {
                node = wrapNode("Filter", "", std::move(node), st.line, st.col);
                node->pred = cella_cloneExpr(*st.having);
            }
            if (!st.star && !st.selectItems.empty())
            {
                std::vector<std::string> items;
                for (const auto &si : st.selectItems)
                {
                    std::string s = cella_exprToString(*si.expr);
                    if (!si.alias.empty())
                        s += " as " + si.alias;
                    items.push_back(s);
                }
                node = wrapNode("Project", "[" + joinStrs(items, ", ") + "]", std::move(node), st.line,
                                st.col);
            }
            if (st.distinct)
            {
                node = wrapNode("Distinct", "", std::move(node), st.line, st.col);
            }
            if (!st.ordered.empty())
            {
                std::vector<std::string> items;
                for (const auto &oi : st.ordered)
                {
                    items.push_back(colNameDisplay(oi.col) + (oi.asc ? " ASC" : " DESC"));
                }
                node = wrapNode("Sort", "(" + joinStrs(items, ", ") + ")", std::move(node), st.line, st.col);
            }
            if (st.among >= 0)
            {
                node = wrapNode("Limit", "(" + std::to_string(st.among) + ")", std::move(node), st.line,
                                st.col);
            }
            if (st.hasPage)
            {
                long long offset = (st.pageNo - 1) * st.pageSize;
                node = wrapNode("Page", "(page " + std::to_string(st.pageNo) + ", size " + std::to_string(st.pageSize) + ", offset " + std::to_string(offset) + ")",
                                std::move(node), st.line, st.col);
            }
            if (st.unionQuery)
            {
                auto u = makeNode("Union", "", st.line, st.col);
                u->children.push_back(std::move(node));
                u->children.push_back(buildGet(*st.unionQuery));
                node = std::move(u);
            }
            return node;
        }

    } // namespace

    std::vector<std::unique_ptr<CELLA_PlanNode>> cella_plan(const CELLA_Program &program,
                                                            const std::vector<bool> &stmtOk,
                                                            const std::vector<std::vector<std::string>> &insertCols,
                                                            std::vector<CELLA_Error> &errors)
    {
        std::vector<std::unique_ptr<CELLA_PlanNode>> plans;
        for (size_t i = 0; i < program.statements.size(); i++)
        {
            if (i < stmtOk.size() && !stmtOk[i])
                continue;
            const CELLA_Stmt &st = *program.statements[i];
            std::unique_ptr<CELLA_PlanNode> node;
            switch (st.kind)
            {
            case CELLA_Stmt::Kind::CREATE_TABLE:
            {
                node = makeNode("CreateTable", "", st.line, st.col);
                node->extra.push_back("table: " + st.tableName);
                std::vector<std::string> cols;
                for (const auto &cd : st.columns)
                {
                    std::string s = cd.name + " " + cella_typeName(cd.type);
                    if (cd.type == CELLA_DataType::CHAR || cd.type == CELLA_DataType::VARCHAR)
                    {
                        s += "(" + std::to_string(cd.hasLen ? cd.len : 255) + ")";
                    }
                    if (cd.notNull)
                        s += " NOT NULL";
                    cols.push_back(s);
                }
                node->extra.push_back("columns: " + joinStrs(cols, ", "));
                break;
            }
            case CELLA_Stmt::Kind::INSERT:
            {
                node = makeNode("Insert", "", st.line, st.col);
                node->extra.push_back("table: " + st.tableName);
                std::vector<std::string> colNames;
                if (i < insertCols.size() && !insertCols[i].empty())
                    colNames = insertCols[i];
                else
                    colNames = st.insertColumns;
                node->extra.push_back("columns: " + joinStrs(colNames, ", "));
                std::vector<std::string> rows;
                for (const auto &row : st.rows)
                {
                    std::vector<std::string> vals;
                    for (const auto &v : row)
                        vals.push_back(cella_exprToString(*v));
                    rows.push_back("[" + joinStrs(vals, ", ") + "]");
                }
                node->extra.push_back("rows: " + joinStrs(rows, ", "));
                break;
            }
            case CELLA_Stmt::Kind::DELETE:
            {
                node = makeNode("Delete",
                                st.where ? "(filter: " + cella_exprToString(*st.where) + ")" : "",
                                st.line, st.col);
                node->children.push_back(makeNode("SeqScan", "[" + st.tableName + "]", st.line, st.col));
                break;
            }
            case CELLA_Stmt::Kind::UPDATE:
            {
                node = makeNode("Update", "", st.line, st.col);
                node->extra.push_back("table: " + st.tableName);
                std::vector<std::string> sets;
                for (const auto &s : st.sets)
                {
                    sets.push_back(s.first + " = " + cella_exprToString(*s.second));
                }
                node->extra.push_back("sets: " + joinStrs(sets, ", "));
                node->children.push_back(makeNode("SeqScan", "[" + st.tableName + "]", st.line, st.col));
                break;
            }
            case CELLA_Stmt::Kind::DROP_TABLE:
            {
                node = makeNode("DropTable", "", st.line, st.col);
                node->extra.push_back("table: " + st.tableName);
                break;
            }
            case CELLA_Stmt::Kind::GET:
                node = buildGet(st);
                break;
            default:
                errors.push_back(cella_makeError(CELLA_Phase::PLN, "PLN-401", st.line, st.col,
                                                 "不支持的语句类型"));
                continue;
            }
            plans.push_back(std::move(node));
        }
        return plans;
    }

} // namespace cella
