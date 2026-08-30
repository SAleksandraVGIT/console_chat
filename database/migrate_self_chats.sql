USE console_chat;

ALTER TABLE chats
    DROP CHECK chat_users_check;

ALTER TABLE chats
    ADD CONSTRAINT chat_users_check
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
    );
