-- expect: SEM-312
-- 分页与 among 冲突：页码 2 的起始行(6) 超出 among 限定的 5 行
CREATE TABLE t(id INT);
get id in t among 5 page 2, 5;
