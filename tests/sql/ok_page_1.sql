-- ok_page_1.sql 分页：page 页码, 每页行数（页码 1 起，行数默认 10）
CREATE TABLE student(id INT, name VARCHAR, age INT);
get name in student ordered age desc page 2, 5;
get name in student page 3;
get name in student limit age > 18 page 1, 2;
get name in student among 10 page 2, 5;
