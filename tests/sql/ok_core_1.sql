-- ok_core_1.sql 核心四条（冒烟测试）
CREATE TABLE student(id INT, name VARCHAR, age INT);
INSERT INTO student(id,name,age) VALUES (1,'Alice',20);
get id,name in student limit age > 18;
DELETE in student limit id = 1;
