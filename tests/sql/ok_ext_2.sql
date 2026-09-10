-- ok_ext_2.sql UPDATE / DROP TABLE
CREATE TABLE student(id INT, name VARCHAR, age INT);
INSERT INTO student VALUES (1,'Alice',20);
UPDATE student SET age = 21, name = 'Alicia' limit id = 1;
get id,name,age in student;
DROP TABLE student;
