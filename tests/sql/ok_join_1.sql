-- ok_join_1.sql 内连接(middle join) + 别名 + ordered + among
CREATE TABLE student(id INT, name VARCHAR, cid INT);
CREATE TABLE course(id INT, title VARCHAR);
get s.name, c.title in student s middle join course c on s.cid = c.id ordered s.name asc among 10;
