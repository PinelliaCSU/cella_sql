-- ok_ext_1.sql 扩展子句：distinct / grouped / having / union
CREATE TABLE student(id INT, name VARCHAR, age INT, class VARCHAR);
INSERT INTO student VALUES (1,'Alice',20,'A'),(2,'Bob',19,'A'),(3,'Carol',22,'B');
get distinct class in student ordered class asc among 5;
get class in student grouped class having class = 'A';
get name in student limit age > 20 union get name in student limit age < 19;
