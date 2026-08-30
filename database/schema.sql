CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    login VARCHAR(100) NOT NULL UNIQUE,
    banned_until_epoch BIGINT NOT NULL DEFAULT 0,
    banned_forever TINYINT(1) NOT NULL DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS users_passwords (
    user_id INT PRIMARY KEY,
    password_hash VARCHAR(255) NOT NULL,

    CONSTRAINT fk_users_passwords_user
        FOREIGN KEY (user_id)
        REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS chats (
    id INT AUTO_INCREMENT PRIMARY KEY,
    chat_name VARCHAR(100) NOT NULL UNIQUE,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    first_user_id INT,
    second_user_id INT,

    INDEX idx_chats_first_user_id (first_user_id),
    INDEX idx_chats_second_user_id (second_user_id),

    CONSTRAINT fk_chats_first_user
        FOREIGN KEY (first_user_id)
        REFERENCES users(id),

    CONSTRAINT fk_chats_second_user
        FOREIGN KEY (second_user_id)
        REFERENCES users(id),

    CONSTRAINT chat_users_check
        CHECK (
            (
                first_user_id IS NULL
                AND second_user_id IS NULL
            )
            OR
            (
                first_user_id IS NOT NULL
                AND second_user_id IS NOT NULL
            )
        )
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS messages (
    id INT AUTO_INCREMENT PRIMARY KEY,
    message_text TEXT NOT NULL,
    chat_id INT NOT NULL,
    sender_id INT NOT NULL,
    sent_at TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),

    INDEX idx_messages_chat_id (chat_id),
    INDEX idx_messages_sender_id (sender_id),

    CONSTRAINT fk_messages_chat
        FOREIGN KEY (chat_id)
        REFERENCES chats(id),

    CONSTRAINT fk_messages_sender
        FOREIGN KEY (sender_id)
        REFERENCES users(id)
) ENGINE=InnoDB;
