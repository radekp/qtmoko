CREATE TABLE mailmessages ( 
    id INTEGER PRIMARY KEY NOT NULL,
    type INTEGER NOT NULL,
    parentfolderid INTEGER NOT NULL,
    previousparentfolderid INTEGER,
    sender VARCHAR,
    recipients VARCHAR,
    subject VARCHAR,
    stamp TIMESTAMP,
    status UNSIGNED BIG INT,
    parentaccountid INTEGER,
    frommailbox VARCHAR,
    mailfile VARCHAR,
    serveruid VARCHAR,
    size INTEGER,
    contenttype INTEGER,
    FOREIGN KEY (parentfolderid) REFERENCES mailfolders(id),
    FOREIGN KEY (parentaccountid) REFERENCES mailaccounts(id));

CREATE INDEX parentfolderid_idx ON mailmessages("parentfolderid"); 
CREATE INDEX parentaccountid_idx ON mailmessages("parentaccountid");
CREATE INDEX frommailbox_idx ON mailmessages("frommailbox");
CREATE INDEX stamp_idx ON mailmessages("stamp");
