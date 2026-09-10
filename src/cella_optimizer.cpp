// cella_optimizer.cpp —— 计划优化：常量折叠、布尔化简、恒真 Filter 消除。
// 与 Planner 分离；对输入计划树深拷贝后变换，保证语义等价且不改动原树。
#include "cella/cella_optimizer.h"

#include <charconv>
#include <memory>
#include <string>
#include <vector>

namespace cella
{
    namespace
    {

        bool isNumberLit(const CELLA_Expr &e)
        {
            return e.kind == CELLA_Expr::Kind::LITERAL && e.lit == CELLA_LiteralKind::NUMBER;
        }
        bool isStringLit(const CELLA_Expr &e)
        {
            return e.kind == CELLA_Expr::Kind::LITERAL && e.lit == CELLA_LiteralKind::STRING;
        }
        bool isDateLit(const CELLA_Expr &e)
        {
            return e.kind == CELLA_Expr::Kind::LITERAL && e.lit == CELLA_LiteralKind::DATE;
        }
        bool isBoolLit(const CELLA_Expr &e)
        {
            return e.kind == CELLA_Expr::Kind::LITERAL && e.lit == CELLA_LiteralKind::BOOL_LIT;
        }
        bool isNullLit(const CELLA_Expr &e)
        {
            return e.kind == CELLA_Expr::Kind::LITERAL && e.lit == CELLA_LiteralKind::NULL_LIT;
        }

        // 数字字面量：用最短十进制表示（to_chars）
        std::string formatNum(double v)
        {
            char buf[64];
            auto res = std::to_chars(buf, buf + sizeof(buf), v);
            return std::string(buf, res.ptr);
        }

        std::unique_ptr<CELLA_Expr> makeNumberLit(double v, const CELLA_Expr &src)
        {
            auto n = std::make_unique<CELLA_Expr>();
            n->kind = CELLA_Expr::Kind::LITERAL;
            n->lit = CELLA_LiteralKind::NUMBER;
            n->line = src.line;
            n->col = src.col;
            n->num = v;
            n->text = formatNum(v);
            return n;
        }

        std::unique_ptr<CELLA_Expr> makeBoolLit(bool v, const CELLA_Expr &src)
        {
            auto n = std::make_unique<CELLA_Expr>();
            n->kind = CELLA_Expr::Kind::LITERAL;
            n->lit = CELLA_LiteralKind::BOOL_LIT;
            n->line = src.line;
            n->col = src.col;
            n->boolVal = v;
            return n;
        }

        bool cmpNumber(double a, double b, CELLA_Expr::BinOp op)
        {
            switch (op)
            {
            case CELLA_Expr::BinOp::EQ:
                return a == b;
            case CELLA_Expr::BinOp::NE:
                return a != b;
            case CELLA_Expr::BinOp::LT:
                return a < b;
            case CELLA_Expr::BinOp::LE:
                return a <= b;
            case CELLA_Expr::BinOp::GT:
                return a > b;
            case CELLA_Expr::BinOp::GE:
                return a >= b;
            default:
                return false;
            }
        }

        bool cmpText(const std::string &a, const std::string &b, CELLA_Expr::BinOp op)
        {
            int c = a.compare(b);
            switch (op)
            {
            case CELLA_Expr::BinOp::EQ:
                return c == 0;
            case CELLA_Expr::BinOp::NE:
                return c != 0;
            case CELLA_Expr::BinOp::LT:
                return c < 0;
            case CELLA_Expr::BinOp::LE:
                return c <= 0;
            case CELLA_Expr::BinOp::GT:
                return c > 0;
            case CELLA_Expr::BinOp::GE:
                return c >= 0;
            default:
                return false;
            }
        }

        // 表达式优化：常量折叠 + 布尔化简；hits 累计规则触发次数
        std::unique_ptr<CELLA_Expr> optimizeExpr(const CELLA_Expr &e, int &hits)
        {
            // 字面量与列引用为叶子节点，直接拷贝
            if (e.kind == CELLA_Expr::Kind::LITERAL || e.kind == CELLA_Expr::Kind::COLUMN_REF)
                return cella_cloneExpr(e);

            if (e.kind == CELLA_Expr::Kind::UNARY)
            {
                auto child = optimizeExpr(*e.child, hits);
                if (e.uop == CELLA_Expr::UnOp::NEG && isNumberLit(*child))
                {
                    hits++;
                    return makeNumberLit(-child->num, e);
                }
                if (e.uop == CELLA_Expr::UnOp::NOT && isBoolLit(*child))
                {
                    hits++;
                    return makeBoolLit(!child->boolVal, e);
                }
                auto n = std::make_unique<CELLA_Expr>();
                n->kind = CELLA_Expr::Kind::UNARY;
                n->line = e.line;
                n->col = e.col;
                n->uop = e.uop;
                n->child = std::move(child);
                return n;
            }

            // BINARY
            auto l = optimizeExpr(*e.left, hits);
            auto r = optimizeExpr(*e.right, hits);

            // ---- 布尔化简 ----
            if (e.bop == CELLA_Expr::BinOp::AND)
            {
                if (isBoolLit(*l) && l->boolVal)
                {
                    hits++;
                    return r;
                }
                if (isBoolLit(*r) && r->boolVal)
                {
                    hits++;
                    return l;
                }
                if ((isBoolLit(*l) && !l->boolVal) || (isBoolLit(*r) && !r->boolVal))
                {
                    hits++;
                    return makeBoolLit(false, e);
                }
            }
            else if (e.bop == CELLA_Expr::BinOp::OR)
            {
                if ((isBoolLit(*l) && l->boolVal) || (isBoolLit(*r) && r->boolVal))
                {
                    hits++;
                    return makeBoolLit(true, e);
                }
                if (isBoolLit(*l) && !l->boolVal)
                {
                    hits++;
                    return r;
                }
                if (isBoolLit(*r) && !r->boolVal)
                {
                    hits++;
                    return l;
                }
            }

            // ---- 常量折叠：算术 ----
            if (isNumberLit(*l) && isNumberLit(*r))
            {
                switch (e.bop)
                {
                case CELLA_Expr::BinOp::PLUS:
                    hits++;
                    return makeNumberLit(l->num + r->num, e);
                case CELLA_Expr::BinOp::MINUS:
                    hits++;
                    return makeNumberLit(l->num - r->num, e);
                case CELLA_Expr::BinOp::MUL:
                    hits++;
                    return makeNumberLit(l->num * r->num, e);
                case CELLA_Expr::BinOp::DIV:
                    if (r->num != 0.0)
                    {
                        hits++;
                        return makeNumberLit(l->num / r->num, e);
                    }
                    break; // 除零：放弃折叠
                default:
                    break;
                }
            }

            // ---- 常量折叠：比较（双方同族才折叠；NULL 参与不折叠） ----
            bool comparable = false;
            if (isNumberLit(*l) && isNumberLit(*r))
                comparable = true;
            else if (isStringLit(*l) && isStringLit(*r))
                comparable = true;
            else if (isDateLit(*l) && isDateLit(*r))
                comparable = true;
            else if (isBoolLit(*l) && isBoolLit(*r))
                comparable = true;
            else if (isNullLit(*l) || isNullLit(*r))
                comparable = false;

            if (comparable && (e.bop == CELLA_Expr::BinOp::EQ || e.bop == CELLA_Expr::BinOp::NE ||
                               e.bop == CELLA_Expr::BinOp::LT || e.bop == CELLA_Expr::BinOp::LE ||
                               e.bop == CELLA_Expr::BinOp::GT || e.bop == CELLA_Expr::BinOp::GE))
            {
                bool result = false;
                if (isNumberLit(*l))
                    result = cmpNumber(l->num, r->num, e.bop);
                else if (isBoolLit(*l))
                    result = cmpNumber(l->boolVal ? 1.0 : 0.0, r->boolVal ? 1.0 : 0.0, e.bop);
                else
                    result = cmpText(l->text, r->text, e.bop);
                hits++;
                return makeBoolLit(result, e);
            }

            auto n = std::make_unique<CELLA_Expr>();
            n->kind = CELLA_Expr::Kind::BINARY;
            n->line = e.line;
            n->col = e.col;
            n->bop = e.bop;
            n->left = std::move(l);
            n->right = std::move(r);
            return n;
        }

        // 计划节点深拷贝（不含 children 递归，由 optimizeNode 处理）
        std::unique_ptr<CELLA_PlanNode> cloneShell(const CELLA_PlanNode &n)
        {
            auto out = std::make_unique<CELLA_PlanNode>();
            out->op = n.op;
            out->detail = n.detail;
            out->extra = n.extra;
            out->line = n.line;
            out->col = n.col;
            out->joinKind = n.joinKind;
            if (n.pred)
                out->pred = cella_cloneExpr(*n.pred);
            if (n.onExpr)
                out->onExpr = cella_cloneExpr(*n.onExpr);
            return out;
        }

        std::unique_ptr<CELLA_PlanNode> optimizeNode(const CELLA_PlanNode &n, int &hits)
        {
            auto out = cloneShell(n);

            // 恒真 Filter 消除：谓词折叠为 TRUE 时用其子节点替代
            if (n.op == "Filter" && n.pred)
            {
                out->pred = optimizeExpr(*n.pred, hits);
                if (out->pred->kind == CELLA_Expr::Kind::LITERAL &&
                    out->pred->lit == CELLA_LiteralKind::BOOL_LIT && out->pred->boolVal)
                {
                    hits++;
                    if (n.children.size() == 1)
                        return optimizeNode(*n.children[0], hits);
                }
            }
            if (n.op == "Join" && n.onExpr)
            {
                out->onExpr = optimizeExpr(*n.onExpr, hits);
            }
            for (const auto &c : n.children)
                out->children.push_back(optimizeNode(*c, hits));
            return out;
        }

    } // namespace

    CELLA_OptResult cella_optimizePlans(const std::vector<std::unique_ptr<CELLA_PlanNode>> &plans)
    {
        CELLA_OptResult result;
        for (const auto &p : plans)
            result.plans.push_back(optimizeNode(*p, result.ruleHits));
        return result;
    }

} // namespace cella
