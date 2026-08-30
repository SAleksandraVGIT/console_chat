USE console_chat;

ALTER TABLE users
    ADD COLUMN banned_until_epoch BIGINT NOT NULL DEFAULT 0,
    ADD COLUMN banned_forever TINYINT(1) NOT NULL DEFAULT 0;
