-- ok_query_1.sql 多行书写查询（每行一个子句）
CREATE TABLE student(id INT, name VARCHAR, age INT, score INT);
get name
in student
limit age > 18 and score < 90;

insert into student(id,name,age,score) values (1,'Alice',20,null);
get name in student limit age > 18 and score is null;
get name in student limit score is not null and age = 20;