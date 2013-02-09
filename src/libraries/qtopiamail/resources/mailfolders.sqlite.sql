CREATE TABLE mailfolders (
    id INTEGER PRIMARY KEY NOT NULL,
    name VARCHAR(255) NOT NULL,
    parentid INTEGER,
    parentaccountid INTEGER,
    status UNSIGNED BIG INT,
    displayname VARCHAR(255),
    FOREIGN KEY (parentaccountid) REFERENCES mailaccounts(id),
    FOREIGN KEY (parentid) REFERENCES mailfolder(id));

