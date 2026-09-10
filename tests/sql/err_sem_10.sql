-- expect: SEM-309
-- 类型混算：数字与字符串相加应报错（v2 表达式类型系统）
CREATE TABLE student(id INT, name VARCHAR, age INT);
INSERT INTO student(id,name,age) VALUES (1,'Alice',20);
get id,name in student limit age > 18 + 'abc';
DELETE in student limit id = 1;
