CREATE DATABASE IF NOT EXISTS console_chat
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE console_chat;

SOURCE database/schema.sql;

-- Create the application account separately with an administrator-selected password:
-- CREATE USER IF NOT EXISTS 'console_chat'@'127.0.0.1' IDENTIFIED BY 'replace_with_password';
-- GRANT SELECT, INSERT, UPDATE, DELETE ON console_chat.* TO 'console_chat'@'127.0.0.1';
