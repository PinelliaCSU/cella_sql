-- ok_dml_limit_1.sql DML 语法：DELETE in 表（FROM→in），条件用 limit
CREATE TABLE student(id INT, name VARCHAR, age INT);
INSERT INTO student VALUES (1,'Alice',20),(2,'Bob',19);
UPDATE student SET age = 21 limit id = 1;
DELETE in student limit id = 2;
DELETE in student;
