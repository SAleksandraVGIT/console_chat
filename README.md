# Console Chat

## Cписок участников команды
- [@SAleksandraVGIT](https://github.com/SAleksandraVGIT) (реализация)
- [@Hillontrop](https://github.com/Hillontrop) (сборка под Linux и Windows)

## Описание

Сетевой консольный чат на C++20 с архитектурой `server + clients`.

Проект поддерживает:
- регистрацию и авторизацию пользователей;
- общий чат `GENERAL`;
- приватные чаты между двумя пользователями;
- отправку и просмотр сообщений через консольное меню;
- одновременную работу нескольких клиентов через TCP.
- сохранение пользователей, чатов и истории сообщений между перезапусками сервера.

## Требования

### Linux (Ubuntu)

- CMake >= 3.16
- Компилятор с поддержкой C++20:
  - g++ 10+
  - или clang++ 12+

### Windows (MinGW + VS Code)

- CMake >= 3.16
- MinGW (g++ с поддержкой C++20)
- MinGW добавлен в PATH

## Сборка

### Linux (Ubuntu, bash)

```bash
# Сгенерировать файлы сборки в папку build
cmake -S . -B build
# Собрать проект (параллельно по ядрам CPU)
cmake --build build -j
```

### Windows (PowerShell)

```powershell
# Сгенерировать файлы сборки в папку build
cmake -S . -B build -G "MinGW Makefiles"
# Собрать проект
cmake --build build
```

Пересборка с нуля:

### Linux (Ubuntu, bash)

```bash
rm -rf build
cmake -S . -B build
cmake --build build -j
```

### Windows (PowerShell)

```powershell
Remove-Item -Recurse -Force build
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Запуск

Собираются два исполняемых файла:
- `chat_server` — сервер;
- `console_chat` — клиент.

### Linux (Ubuntu, bash)

1. Запустите сервер в первом терминале:

```bash
./build/chat_server
```

2. Запустите клиент(ы) во втором и третьем терминалах:

```bash
./build/console_chat
```

Параметры запуска:
- сервер: `./build/chat_server --port 7777`
- клиент: `./build/console_chat --host 127.0.0.1 --port 7777`
- допустимый диапазон порта: `1024..49151`
- файлы состояния сервера по умолчанию: `data/users.db`, `data/chats.db`
- запуск сервера без подгрузки истории: `./build/chat_server --reset-state`
- пользовательские файлы состояния: `./build/chat_server --users-file data/users.db --chats-file data/chats.db`

### Windows (PowerShell)

1. Запустите сервер:

```powershell
.\build\chat_server.exe
```

2. Запустите один или несколько клиентов:

```powershell
.\build\console_chat.exe
```

Параметры запуска:
- сервер: `.\build\chat_server.exe --port 7777`
- клиент: `.\build\console_chat.exe --host 127.0.0.1 --port 7777`
- допустимый диапазон порта: `1024..49151`
- файлы состояния сервера по умолчанию: `data/users.db`, `data/chats.db`
- запуск сервера без подгрузки истории: `.\build\chat_server.exe --reset-state`
- пользовательские файлы состояния: `.\build\chat_server.exe --users-file data/users.db --chats-file data/chats.db`

## Структура проекта

- `include/console_chat/core/` — доменные модели и бизнес-логика
- `include/console_chat/client/` — клиентский API и консольный интерфейс
- `include/console_chat/network/` — TCP-сокет обёртка
- `src/core/` — реализации доменной логики
- `src/client/` — клиентская реализация и точка входа клиента
- `src/server/` — серверная точка входа и серверные обработчики протокола
- `src/network/` — реализация сокетного слоя
- `CMakeLists.txt` — конфигурация сборки

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
    - получение списка пользователей (`GetAllUserLogins`).
    - загрузка/сохранение состояния (`LoadState`, `SaveState`).
  - хранит:
    - `m_users` — зарегистрированные пользователи;
    - `m_chats` — чаты на сервере.
  - автоматически создаёт чат `GENERAL` в конструкторе.

`ChatService` не хранит глобальную авторизацию процесса; текущий логин передаётся в методы явно.

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

- `src/server/main.cpp` — запуск TCP-сервера и `accept`-цикл
- `request_router.*` — обработка команд протокола
- `protocol.*` — разбор/сборка строкового протокола
- `session.*` — обработка клиентской сессии

### Network (`namespace console_chat::network`)

- `TcpSocket` (`include/console_chat/network/tcp_socket.h`, `src/network/tcp_socket.cpp`)
  - обёртка над системным TCP-сокетом:
    - `Connect(...)`;
    - `BindAndListen(...)`;
    - `Accept()`;
    - `SendLine(...)`/`RecvLine(...)`;
    - `Close()`.

## Протокол (текущая версия)

Обмен идёт строками по TCP:
- разделитель полей: `\t`
- конец сообщения: `\n`

Примеры команд:
- `REGISTER`
- `LOGIN`
- `LOGOUT`
- `GET_MY_CHATS`
- `GET_MESSAGES`
- `SEND_MESSAGE`

Ответы:
- `OK ...` — успех
- `ERR ...` — ошибка

## Ограничения в `ChatService`

- максимум `991` чатов на сервере (`MAX_CHATS_ON_SERVER`)
- максимум `45` приватных чатов на пользователя (`MAX_PRIVATE_CHATS_PER_USER`)
- длина сообщения: от `1` до `256` символов (`MAX_MESSAGE_LENGTH`)
- максимум `1000` сообщений в одном чате (`MAX_MESSAGES_PER_CHAT`)

## Хранение состояния

- Состояние сервера (пользователи, чаты, сообщения) сохраняется в файлы.
- При обычном запуске сервер пытается загрузить предыдущее состояние.
- При запуске с `--reset-state` сервер очищает файлы состояния и стартует с пустым состоянием.

## Примечания

- Для VS Code:
  - задачи из `.vscode/tasks.json` уже учитывают ОС (Linux/Windows);
  - в `.vscode/c_cpp_properties.json` есть отдельные конфигурации `Linux` и `MinGW`;
  - при необходимости поправьте путь `compilerPath` в конфигурации `MinGW` под ваш локальный путь к `g++.exe`.

- Если хотите запускать одной строкой в PowerShell, используйте `;` вместо `&&`:

```powershell
cmake -S . -B build -G "MinGW Makefiles"; cmake --build build; .\build\console_chat.exe
```

- Если меняете генератор CMake (например, `Unix Makefiles` <-> `MinGW Makefiles`) или переносите проект между разными ОС, очищайте `build`, чтобы не использовать несовместимый `CMakeCache.txt`.

## План по развитию проекта

### 18

- Написать Makefile сборки проекта. Сделайте в нём несколько синтетических целей.

### 19

- Добавить использование системных вызовов, например вывод названия и версии ОС при запуске.
- Обеспечить сборку/работу в Windows и вывод информации о процессе и системе через Windows API.

### 1

- Добавить защиту для персональных данных
