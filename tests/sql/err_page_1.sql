-- expect: SEM-311
-- 分页参数必须为正整数：页码 0 应报错
CREATE TABLE t(id INT);
get id in t page 0, 5;
