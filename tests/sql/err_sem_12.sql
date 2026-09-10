-- expect: SEM-313
CREATE TABLE student(id INT, name VARCHAR, age INT, score INT);
get name in student limit age > 18 and score < null;
get name in student limit score = null;
get name in student limit null != 1;
