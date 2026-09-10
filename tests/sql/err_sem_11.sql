-- expect: SEM-310
-- 条件表达式必须为 BOOL：limit 直接引用 INT 列应报错
CREATE TABLE student(id INT);
get id in student limit id;
