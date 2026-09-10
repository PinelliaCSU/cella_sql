// cella_catalog.h —— 内存目录：表/列结构与大小写不敏感的表查询（任务书 3.3 节）。
// 一次进程内累积、不重置；进程结束后不保留。CREATE TABLE / DROP TABLE 会改变它。
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "cella_common.h"

namespace cella
{

    enum class CELLA_DataType
    {
        INT,
        FLOAT,
        DOUBLE,
        CHAR,
        VARCHAR,
        TEXT,
        DATE,
        TIME,
        DATETIME
    };

    inline std::string cella_typeName(CELLA_DataType t)
    {
        switch (t)
        {
        case CELLA_DataType::INT:
            return "INT";
        case CELLA_DataType::FLOAT:
            return "FLOAT";
        case CELLA_DataType::DOUBLE:
            return "DOUBLE";
        case CELLA_DataType::CHAR:
            return "CHAR";
        case CELLA_DataType::VARCHAR:
            return "VARCHAR";
        case CELLA_DataType::TEXT:
            return "TEXT";
        case CELLA_DataType::DATE:
            return "DATE";
        case CELLA_DataType::TIME:
            return "TIME";
        case CELLA_DataType::DATETIME:
            return "DATETIME";
        }
        return "?";
    }

    struct CELLA_Column
    {
        std::string name; // 原始拼写
        CELLA_DataType type = CELLA_DataType::INT;
        int len = 0; // CHAR/VARCHAR 长度（非字符类型为 0）
        bool notNull = false;
    };

    struct CELLA_Table
    {
        std::string name; // 原始拼写
        std::vector<CELLA_Column> columns;
    };

    // 目录：表名/列名比较不区分大小写（内部以大写为键）
    struct CELLA_Catalog
    {
        std::map<std::string, CELLA_Table> tables;

        const CELLA_Table *findTable(const std::string &name) const
        {
            auto it = tables.find(cella_toUpper(name));
            return it == tables.end() ? nullptr : &it->second;
        }

        CELLA_Table *findTable(const std::string &name)
        {
            auto it = tables.find(cella_toUpper(name));
            return it == tables.end() ? nullptr : &it->second;
        }

        static const CELLA_Column *findColumn(const CELLA_Table &table, const std::string &name)
        {
            std::string key = cella_toUpper(name);
            for (const auto &c : table.columns)
            {
                if (cella_toUpper(c.name) == key)
                    return &c;
            }
            return nullptr;
        }

        bool addTable(CELLA_Table table)
        {
            std::string key = cella_toUpper(table.name);
            if (tables.count(key) != 0)
                return false;
            tables.emplace(std::move(key), std::move(table));
            return true;
        }

        bool dropTable(const std::string &name) { return tables.erase(cella_toUpper(name)) > 0; }
    };

} // namespace cella
