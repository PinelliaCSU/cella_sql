# cella SQL 文法设计文档（grammar.md）

> 交付物对应指导书 guide.md 4.2 / 11.4。本文件是编译器的权威文法，与代码实现保持一致。
> 实现策略：**递归下降 + 预测分析**（每个非终结符对应一个函数，lookahead 为 1 个 Token，
> 查询子句顺序固定，故无需复杂 LR 工具；错误恢复同步到下一个 `;`）。

## 1. 词法（Token）

| 类别 | 规则 | 示例 |
|---|---|---|
| KEYWORD | 保留字总表（见 2.2 节），大小写不敏感 | `CREATE` `get` `middle` |
| IDENTIFIER | `[A-Za-z_][A-Za-z0-9_]*`，且非保留字 | `student` `_x1` |
| CONST(NUMBER) | 整数/小数（多小数点报 LEX-103） | `1` `3.14` |
| CONST(STRING) | `'...'`，内部 `''` 转义 | `'Alice'` `'Tom''s'` |
| CONST(DATE) | 字符串内容形如 `YYYY-MM-DD` | `'2024-02-20'` |
| OPERATOR | `= == != <> < <= > >= + - * / .` | — |
| DELIMITER | `(` `)` `,` `;` | — |
| EOF | 文件结束 | — |

注释：`-- 行注释`、`/* 块注释 */`（可跨行、不嵌套）。行尾兼容 CRLF/LF；文件头 UTF-8 BOM 自动跳过。

## 2. 保留字总表

```
CREATE TABLE ALTER DROP TRUNCATE RENAME
INSERT INTO VALUES UPDATE SET DELETE FROM WHERE
GET IN LIMIT GROUPED HAVING ORDERED AMONG PAGE
JOIN ON LEFT RIGHT MIDDLE UNION DISTINCT AS
AND OR NOT NULL TRUE FALSE ASC DESC IS
INT INTEGER FLOAT DOUBLE CHAR VARCHAR TEXT DATE TIME DATETIME
```

> v3.6：`FROM`/`WHERE` 仅为保留字（防止误用为标识符），文法中已彻底不使用。

## 3. 完整文法（EBNF，`%` 后为注释）

```
program          := { statement } EOF ;
statement        := create_table_stmt | insert_stmt | get_stmt
                  | delete_stmt | update_stmt | drop_table_stmt ;

create_table_stmt := CREATE TABLE table_name '(' column_def { ',' column_def } ')' ';' ;
column_def        := column_name data_type [ NOT NULL ] ;
data_type         := INT | INTEGER | FLOAT | DOUBLE
                   | CHAR [ '(' uint ')' ] | VARCHAR [ '(' uint ')' ] | TEXT
                   | DATE | TIME | DATETIME ;       % CHAR/VARCHAR 省略长度默认 255

insert_stmt       := INSERT INTO table_name [ '(' column_name { ',' column_name } ')' ]
                     VALUES row_values { ',' row_values } ';' ;
row_values        := '(' const_expr { ',' const_expr } ')' ;

get_stmt          := GET [ DISTINCT ] select_list
                     IN table_ref { join_clause }
                     [ LIMIT expr ]                       % = WHERE
                     [ GROUPED col_name { ',' col_name } ]% = GROUP BY
                     [ HAVING expr ]
                     [ ORDERED order_item { ',' order_item } ] % = ORDER BY
                     [ AMONG uint ]                       % = LIMIT 行数
                     [ PAGE uint [ ',' uint ] ]           % = 分页（页码 1 起，行数默认 10；
                                                        % 与 among 同现时起始行须落在 among 范围内，否则 SEM-312）
                     [ UNION GET [ DISTINCT ] select_list IN table_ref { join_clause }
                         [ LIMIT expr ] [ GROUPED ... ] [ HAVING expr ]
                         [ ORDERED ... ] [ AMONG uint ] [ UNION ... ] ]
                     ';' ;                                % 子句顺序固定，union 右臂递归

select_list       := '*' | select_item { ',' select_item } ;
select_item       := expr [ AS alias ] ;                  % AS 可省略，别名紧跟
table_ref         := table_name [ AS alias ] ;
join_clause       := [ LEFT | RIGHT | MIDDLE ] JOIN table_ref ON expr ;
order_item        := col_name [ ASC | DESC ] ;            % 默认 ASC

delete_stmt       := DELETE in table_name [ LIMIT expr ] ';' ;  % v3.6: FROM → in，条件用 limit
update_stmt       := UPDATE table_name SET col_name '=' expr
                     { ',' col_name '=' expr } [ LIMIT expr ] ';' ;
drop_table_stmt   := DROP TABLE table_name ';' ;

table_name        := IDENTIFIER ;
column_name       := IDENTIFIER ;
col_name          := IDENTIFIER [ '.' IDENTIFIER ] ;     % 表名.列名（多表/别名）
alias             := IDENTIFIER ;
uint              := CONST(NUMBER) 且为无小数点整数 ;
```

## 4. 表达式文法与优先级

```
expr        := or_expr ;
or_expr     := and_expr { OR and_expr } ;
and_expr    := not_expr { AND not_expr } ;
not_expr    := NOT not_expr | comparison ;
comparison  := add [ ( '=' | '==' | '!=' | '<>' | '<' | '<=' | '>' | '>=' ) add ] ;
add         := mul { ( '+' | '-' ) mul } ;
mul         := unary { ( '*' | '/' ) unary } ;
unary       := '-' unary | primary ;
primary     := const_expr | col_name | '(' expr ')' ;
const_expr  := NUMBER | STRING | DATE | NULL | TRUE | FALSE ;
```

**优先级与结合性（实现为准）**：

1. 按文法层次，比较运算位于 `not_expr` 之下：`NOT a = b` 解析为 `NOT (a = b)`。
   （注：guide.md 优先级表写 "NOT > 比较"，但 guide 自身文法 `not_expr := NOT not_expr | comparison`
   与之矛盾；本实现遵循文法，即 **NOT 作用于比较之上**，并在答辩材料中说明。）
2. 算术层次：`-`(一元) > `*` `/` > `+` `-`；同层左结合。
3. 逻辑层次：比较 > `NOT` > `AND` > `OR`。
4. `(` `)` 可改变结合；`==` 与 `=` 等价（EQ），`<>` 与 `!=` 等价（NE）。

## 5. 表达式类型系统（语义阶段）

| 操作 | 规则 | 结果类型 |
|---|---|---|
| `+ - * /` | 两侧均数值（INT/FLOAT/DOUBLE） | INT < FLOAT < DOUBLE 提升 |
| `+ - * /` | 任一侧非数值 | 报 SEM-309 |
| 比较 | 数值-数值 / 字符串-字符串 / 日期-日期 / BOOL-BOOL | BOOL |
| 比较 | 跨族（如 INT 与 VARCHAR） | 报 SEM-309 |
| 比较 | 任一侧 NULL | BOOL（合法） |
| `AND` / `OR` | 两侧必须 BOOL | BOOL |
| `NOT` | 操作数必须 BOOL | BOOL |
| 一元 `-` | 操作数必须数值 | 数值 |

- 字面量：整数→INT；含小数点→FLOAT；`'...'`→VARCHAR；`'YYYY-MM-DD'`→DATE；`NULL`→NULL；`TRUE/FALSE`→BOOL。
- `limit`/`on`/`having`/`WHERE` 条件结果必须为 BOOL，否则 SEM-310。

## 6. AST 节点结构

- 语句（tagged struct `CELLA_Stmt`，基元字段 `kind/line/col`）：
  `CreateTableStmt{tableName, columns[]}`、`InsertStmt{tableName, insertColumns[]?, rows[][]}`、
  `GetStmt{distinct, star, selectItems[], from, joins[], limit, grouped[], having, ordered[], among, unionQuery}`、
  `DeleteStmt{tableName, where?}`、`UpdateStmt{tableName, sets[], where?}`、`DropTableStmt{tableName}`；
  容器 `CELLA_Program{statements[]}`。
- 结构体：`CELLA_ColumnDef{name,type,len,hasLen,notNull}`、`CELLA_TableRef{name,alias}`、
  `CELLA_JoinClause{kind,ref,on}`、`CELLA_SelectItem{expr,alias}`、`CELLA_ColName{table?,column}`、
  `CELLA_OrderItem{col,asc}`。
- 表达式 `CELLA_Expr`（tagged struct，含 `line/col`）：
  `LiteralExpr{lit,text,num,boolVal}`、`ColumnRefExpr{table?,column}`、
  `UnaryExpr{uop∈{NEG,NOT},child}`、`BinaryExpr{bop∈{EQ,NE,LT,LE,GT,GE,PLUS,MINUS,MUL,DIV,AND,OR},left,right}`。
- 每个节点携带源位置（行:列，1 起），供语义错误与诊断定位。

## 7. Plan 节点结构

- `CELLA_PlanNode{op, detail, extra[], line, col, pred?, onExpr?, joinKind?, children[]}`。
- 算子：`CreateTable` `Insert` `Delete(filter)` `SeqScan` `Filter(pred)` `Project` `Sort` `Limit`
  `Page(page,size,offset)` `Aggregate(grouped)` `Join(kind,on)` `Union` `Distinct` `Update` `DropTable`。
- 转换规则：`get ... in t limit c` → `Project → Filter(c) → SeqScan(t)`；
  无条件 `delete` → `Delete → SeqScan`；`get *` 省略 Project；
  查询自下而上：SeqScan → Join → Filter(limit) → Aggregate → Filter(having) → Project → Distinct → Sort → Limit(among) → Union。

## 8. 优化规则（`-o`）

1. 常量折叠：`age > 10 + 8` → `age > 18`；`1 = 1` → `TRUE`。
2. 布尔化简：`x AND TRUE`→`x`；`x OR FALSE`→`x`；`x AND FALSE`→`FALSE`；`x OR TRUE`→`TRUE`；`NOT TRUE`→`FALSE`。
3. 恒真 Filter 消除：谓词折叠为 `TRUE` 时删除 Filter 节点。
- 范围：Filter（limit/having）与 Join(on)；除零时放弃折叠；保证语义等价。

## 9. 关键非终结符 FIRST 集（供递归下降 lookahead 参考）

| 非终结符 | FIRST |
|---|---|
| statement | CREATE, INSERT, GET, DELETE, UPDATE, DROP |
| expr / or / and / not | NOT, IDENTIFIER, CONST, `(`, `-` |
| comparison 之后 | `= == != <> < <= > >=` |
| select_list | `*`, NOT, IDENTIFIER, CONST, `(`, `-` |
| get 子句序 | IN → (LEFT/RIGHT/MIDDLE/JOIN)* → LIMIT → GROUPED → HAVING → ORDERED → AMONG → PAGE → UNION → `;` |
