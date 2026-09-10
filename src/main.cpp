// main.cpp —— CLI 入口：参数解析 -> 按阶段调度 -> 统一诊断输出 -> 退出码。
// 退出码: 0=全部成功；1=存在任意词法/语法/语义/计划错误；2=用法错误。
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cella/cella_ast.h"
#include "cella/cella_catalog.h"
#include "cella/cella_common.h"
#include "cella/cella_lexer.h"
#include "cella/cella_optimizer.h"
#include "cella/cella_parser.h"
#include "cella/cella_planner.h"
#include "cella/cella_printer.h"
#include "cella/cella_semantic.h"

#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int);
#endif

namespace
{

    void printUsage(std::ostream &os)
    {
        os << "用法: cella_sql [选项] [文件]\n"
           << "选项:\n"
           << "  -l, --lex      仅词法分析，输出 Token 流\n"
           << "  -a, --ast      词法+语法，输出 AST\n"
           << "  -s, --sem      执行到语义分析并输出各语句语义结论/Catalog 概要\n"
           << "  -p, --plan     输出执行计划（默认值：不加选项时即此行为）\n"
           << "  -o, --opt      输出优化前/后执行计划对比（常量折叠、布尔化简、恒真 Filter 消除）\n"
           << "      --all      依次打印 词法→AST→语义→计划→优化后计划 全阶段\n"
           << "  -h, --help     帮助\n"
           << "若省略文件，则从标准输入读取（Windows 下 Ctrl+Z 或 EOF 结束）。\n"
           << "退出码: 0=全部成功；1=存在任意词法/语法/语义/计划错误；2=用法错误。\n";
    }

} // namespace

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(65001); // 控制台按 UTF-8 显示（不影响重定向到文件的字节流）
#endif

    bool optLex = false, optAst = false, optSem = false, optPlan = false, optAll = false;
    bool optOpt = false;
    std::string filePath;
    bool hasFile = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            printUsage(std::cout);
            return 0;
        }
        else if (arg == "-l" || arg == "--lex")
        {
            optLex = true;
        }
        else if (arg == "-a" || arg == "--ast")
        {
            optAst = true;
        }
        else if (arg == "-s" || arg == "--sem")
        {
            optSem = true;
        }
        else if (arg == "-p" || arg == "--plan")
        {
            optPlan = true;
        }
        else if (arg == "-o" || arg == "--opt")
        {
            optOpt = true;
        }
        else if (arg == "--all")
        {
            optAll = true;
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "未知选项: " << arg << "\n";
            printUsage(std::cerr);
            return 2;
        }
        else if (hasFile)
        {
            std::cerr << "只能指定一个输入文件\n";
            printUsage(std::cerr);
            return 2;
        }
        else
        {
            filePath = arg;
            hasFile = true;
        }
    }

    if (!optLex && !optAst && !optSem && !optPlan && !optAll && !optOpt)
        optPlan = true; // 默认输出执行计划
    if (optAll)
    {
        optLex = optAst = optSem = optPlan = true;
    }

    // 读取输入：文件或标准输入
    std::string source;
    if (hasFile)
    {
        std::ifstream fin(filePath, std::ios::binary);
        if (!fin)
        {
            std::cerr << "无法打开文件: " << filePath << "\n";
            return 2;
        }
        std::ostringstream ss;
        ss << fin.rdbuf();
        source = ss.str();
    }
    else
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            source += line;
            source += '\n';
        }
    }

    // 词法 -> 语法 -> 语义 -> 计划
    std::vector<cella::CELLA_Error> errors;
    auto tokens = cella::cella_tokenize(source, errors);

    const bool needParse = optAst || optSem || optPlan || optOpt;
    std::unique_ptr<cella::CELLA_Program> program;
    std::vector<bool> stmtOk;
    std::vector<std::string> semMessages;
    std::vector<std::vector<std::string>> semInsertCols;
    cella::CELLA_Catalog catalog;
    std::vector<std::unique_ptr<cella::CELLA_PlanNode>> plans;

    bool firstSection = true;
    auto sectionSep = [&]()
    {
        if (!firstSection)
            std::cout << "\n";
        firstSection = false;
    };

    if (optLex)
    {
        sectionSep();
        cella::cella_printTokens(tokens, std::cout);
    }
    if (needParse)
    {
        program = cella::cella_parse(tokens, errors);
        if (optAst)
        {
            sectionSep();
            if (program)
                cella::cella_printAst(*program, std::cout);
        }
    }
    if (optSem || optPlan || optOpt)
    {
        if (program)
        {
            auto sem = cella::cella_analyze(*program, catalog);
            errors.insert(errors.end(), sem.errors.begin(), sem.errors.end());
            semMessages = std::move(sem.okMessages);
            stmtOk = std::move(sem.stmtOk);
            semInsertCols = std::move(sem.insertColumns);
        }
        if (optSem)
        {
            sectionSep();
            for (const auto &m : semMessages)
                std::cout << m << "\n";
        }
    }
    if (optPlan || optOpt)
    {
        if (program)
            plans = cella::cella_plan(*program, stmtOk, semInsertCols, errors);
        if (optPlan)
        {
            sectionSep();
            cella::cella_printPlan(plans, std::cout);
        }
    }
    if (optOpt && program)
    {
        auto opt = cella::cella_optimizePlans(plans);
        sectionSep();
        std::cout << "== 优化前 ==\n";
        cella::cella_printPlan(plans, std::cout);
        std::cout << "\n== 优化后 ==\n";
        cella::cella_printPlan(opt.plans, std::cout);
    }
    if (optAll && program)
    {
        // --all 完整流水线：词法→AST→语义→计划(未优化)→优化后计划
        auto opt = cella::cella_optimizePlans(plans);
        sectionSep();
        std::cout << "== 优化后 ==\n";
        cella::cella_printPlan(opt.plans, std::cout);
    }

    // 统一诊断输出（词法/语法/语义/计划，按发生顺序）
    if (!errors.empty())
    {
        sectionSep();
        for (const auto &e : errors)
            std::cout << cella::cella_errorText(e) << "\n";
    }

    return errors.empty() ? 0 : 1;
}
