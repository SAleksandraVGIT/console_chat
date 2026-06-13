# Архитектура Console Chat

## Классы и их ответственность

### Core (`namespace console_chat::core`)

- `Message` (`include/console_chat/core/base_chat.h`)
  - простая структура сообщения:
    - `Name` — имя отправителя;
    - `Text` — текст сообщения.

- `PasswordProtector` (`include/console_chat/core/password_protector.h`, `src/core/password_protector.cpp`)
  - класс защиты пароля:
    - вычисляет хеш (`Hash`);
    - проверяет пароль по хешу (`Verify`);
    - проверяет формат хеша (`IsHash`).

- `User` (`include/console_chat/core/user.h`)
  - модель пользователя:
    - хранит имя (`m_name`) и хеш пароля (`m_passwordHash`);
    - `GetName()` возвращает имя;
    - `CheckPassword(...)` делегирует проверку в `PasswordProtector`.

- `BaseChat` (`include/console_chat/core/base_chat.h`, `src/core/base_chat.cpp`)
  - базовый класс чата:
    - хранит историю `std::vector<Message> m_messages`;
    - `GetMessages()` возвращает историю;
    - `AddMessage(...)` добавляет сообщение с ограничением по размеру;
    - `IsParticipant(...)` по умолчанию возвращает `true` (общий доступ);
    - `IsPrivate()` по умолчанию возвращает `false`.

- `PrivateChat` (`include/console_chat/core/private_chat.h`, `src/core/private_chat.cpp`)
  - наследник `BaseChat` для приватного диалога:
    - хранит двух участников в `std::array<std::string, 2>`;
    - `HasUser(...)`/`IsParticipant(...)` проверяют участие пользователя;
    - `IsPrivate()` возвращает `true`.

- `ChatService` (`include/console_chat/core/chat_service.h`, `src/core/chat_service.cpp`)
  - основная бизнес-логика:
    - регистрация (`Register`);
    - авторизация (`Authenticate`);
    - создание приватного чата (`CreatePrivateChat`);
    - получение списка чатов пользователя (`GetMyChats`);
    - получение сообщений (`GetMessages`);
    - отправка сообщений (`SendMessage`);
    - получение списка пользователей (`GetAllUserLogins`);
    - загрузка начального состояния (`Initialize`, `ImportState`);
    - точечное сохранение пользователя, чата или сообщения через `IManager` до изменения памяти.
  - хранит:
    - `m_users` — зарегистрированные пользователи;
    - `m_chats` — чаты на сервере.
  - автоматически создаёт чат `GENERAL` в конструкторе.

`ChatService` не хранит глобальную авторизацию процесса; текущий логин передаётся в методы явно.

### Storage (`namespace console_chat::storage`)

- `IManager`
  - `Initialize` подготавливает хранилище;
  - `Load` загружает полный `ServiceState` при запуске;
  - `AddUser` сохраняет одного нового пользователя;
  - `AddChat` сохраняет один чат и его участников;
  - `AddMessage` сохраняет одно сообщение и получает логин автора для будущего внешнего ключа;
  - `Reset` очищает хранилище.
- `FileManager`
  - поддерживает снимок `ServiceState` в памяти;
  - после точечной операции обновляет снимок и перезаписывает прежние два файла.

Для подключения будущей базы данных достаточно реализовать новый менеджер через
`IManager`. Например, MySQL-реализация сможет использовать `SELECT` в `Load`, отдельный
`INSERT` в `AddUser`/`AddMessage` и транзакцию в `AddChat`. Роутер и сессии от СУБД не зависят.

### Client (`namespace console_chat::client`)

- `ChatClient` (`include/console_chat/client/chat_client.h`, `src/client/chat_client.cpp`)
  - транспортный клиент поверх TCP:
    - формирует команды протокола;
    - отправляет запросы на сервер;
    - получает и разбирает ответы.
  - предоставляет API для `ChatConsole`:
    - `Register`, `Authenticate`, `Logout`;
    - `GetMyChats`, `GetMessages`, `SendMessage`;
    - `GetAllUserLogins`, `GetCurrentUserName`, `IsAuthenticated`.

- `ChatConsole` (`include/console_chat/client/chat_console.h`, `src/client/chat_console.cpp`)
  - консольный UI:
    - главное меню и меню пользователя;
    - сценарии регистрации/входа;
    - открытие общего и приватных чатов;
    - отправка сообщений и вывод истории.
  - команды в сессии чата:
    - `/0` — выход из чата;
    - `/all` — печать всей истории.

- `src/client/main.cpp`
  - точка входа клиента:
    - создаёт `ChatClient`;
    - создаёт `ChatConsole`;
    - запускает `Run()`.

### Server (`namespace console_chat::server`)

- `src/server/main.cpp` — запуск TCP-сервера и `accept`-цикл.
- `request_router.*` — обработка команд протокола.
- `protocol.*` — разбор и сборка строкового протокола.
- `session.*` — обработка клиентской сессии.

### Network (`namespace console_chat::network`)

- `TcpSocket` (`include/console_chat/network/tcp_socket.h`, `src/network/tcp_socket.cpp`)
  - обёртка над системным TCP-сокетом:
    - `Connect(...)`;
    - `BindAndListen(...)`;
    - `Accept()`;
    - `SendLine(...)`/`RecvLine(...)`;
    - `Close()`.

## Протокол

Обмен идёт строками по TCP:

- разделитель полей: `\t`;
- конец сообщения: `\n`.

Основные команды:

- `REGISTER`;
- `LOGIN`;
- `LOGOUT`;
- `GET_MY_CHATS`;
- `GET_MESSAGES`;
- `SEND_MESSAGE`.

Ответы:

- `OK ...` — успех;
- `ERR ...` — ошибка.

## Ограничения `ChatService`

- максимум `991` чатов на сервере (`MAX_CHATS_ON_SERVER`);
- максимум `45` приватных чатов на пользователя (`MAX_PRIVATE_CHATS_PER_USER`);
- длина сообщения: от `1` до `256` символов (`MAX_MESSAGE_LENGTH`);
- максимум `1000` сообщений в одном чате (`MAX_MESSAGES_PER_CHAT`).

## Хранение состояния

- Состояние сервера сохраняется через точечные методы `IManager`.
- Сейчас используется `FileManager`.
- `ChatService` сначала сохраняет изменение, затем изменяет состояние в памяти.
- При обычном запуске сервер пытается загрузить предыдущее состояние.
- При запуске с `--reset-state` сервер очищает файлы состояния и стартует с пустым состоянием.

## Диаграммы

- [Клиентская часть](client.puml).
- [Сервис и доменная модель](service.puml).
- [Взаимодействие компонентов](components.puml).
