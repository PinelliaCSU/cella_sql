-- ok_core_2.sql 多行插入 + 多种条件 + 全列
CREATE TABLE student(id INT, name VARCHAR, age INT);
INSERT INTO student VALUES (1,'Alice',20),(2,'Bob',19),(3,'Carol',22);
get * in student;
get id,name in student limit age >= 20 and age < 22;
get name in student limit age > 18;
DELETE in student limit name = 'Bob';
