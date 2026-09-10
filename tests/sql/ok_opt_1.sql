-- ok_opt_1.sql 优化规则演示：常量折叠 + 布尔化简 + 恒真 Filter 消除
CREATE TABLE student(id INT, name VARCHAR, age INT);
get name in student limit 1 = 1 and age > 10 + 8;
get name in student limit name = 'Alice' and true;
get name in student limit 1 = 1;
get name in student limit 2 = 3;
