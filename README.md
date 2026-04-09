# Console Chat

## Cписок участников команды
- [@SAleksandraVGIT](https://github.com/SAleksandraVGIT) (реализация)
- [@Hillontrop](https://github.com/Hillontrop) (сборка под Linux и Windows)

## Описание выбранной идеи решения

Небольшой консольный чат на C++20.

Проект поддерживает:
- регистрацию и авторизацию пользователей;
- общий чат `GENERAL`;
- приватные чаты между двумя пользователями;
- отправку и просмотр сообщений через консольное меню.

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

## Сборка и запуск

### Linux (Ubuntu, bash)

```bash
# Сгенерировать файлы сборки в папку build
cmake -S . -B build
# Собрать проект (параллельно по ядрам CPU)
cmake --build build -j
# Запустить собранное приложение
./build/console_chat
```

### Windows (PowerShell)

```powershell
# Сгенерировать файлы сборки в папку build
cmake -S . -B build -G "MinGW Makefiles"
# Собрать проект
cmake --build build
# Запустить собранное приложение
.\build\console_chat.exe
```

Для VS Code:
- задачи из `.vscode/tasks.json` уже учитывают ОС (Linux/Windows);
- в `.vscode/c_cpp_properties.json` есть отдельные конфигурации `Linux` и `MinGW`;
- при необходимости поправьте путь `compilerPath` в конфигурации `MinGW` под ваш локальный путь к `g++.exe`.

Если хотите запускать одной строкой в PowerShell, используйте `;` вместо `&&`:

```powershell
cmake -S . -B build -G "MinGW Makefiles"; cmake --build build; .\build\console_chat.exe
```

Пересборка с нуля:

### Linux (Ubuntu, bash)

```bash
rm -rf build
cmake -S . -B build
cmake --build build -j
./build/console_chat
```

### Windows (PowerShell)

```powershell
Remove-Item -Recurse -Force build
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
.\build\console_chat.exe
```

Важно: если меняете генератор CMake (например, `Unix Makefiles` <-> `MinGW Makefiles`) или переносите проект между разными ОС, очищайте `build`, чтобы не использовать несовместимый `CMakeCache.txt`.

## Структура проекта

- `include/console_chat/` — заголовочные файлы
- `src/` — реализации классов
- `src/main.cpp` — точка входа
- `CMakeLists.txt` — конфигурация сборки

## Классы и их ответственность

### `Message` (`include/console_chat/base_chat.h`)

Простая структура сообщения:
- `Name` — имя отправителя;
- `Text` — текст сообщения.

### `User` (`include/console_chat/user.h`)

Модель пользователя:
- хранит имя и пароль;
- логин используется как ключ в `ChatService`;
- проверяет пароль через `CheckPassword`.

### `BaseChat` (`include/console_chat/base_chat.h`, `src/base_chat.cpp`)

Базовый класс чата:
- хранит список сообщений;
- имя чата хранится в `ChatService`;
- по умолчанию доступен всем (`IsParticipant(...) == true`);
- по умолчанию не приватный (`IsPrivate() == false`);
- добавляет сообщение через `AddMessage`.

Ограничение:
- максимум `1000` сообщений в одном чате (`MAX_MESSAGES_PER_CHAT`).

### `PrivateChat` (`include/console_chat/private_chat.h`, `src/private_chat.cpp`)

Наследник `BaseChat` для диалога двух пользователей:
- хранит участников в `std::array<std::string, 2>`;
- проверяет участие через поиск (`std::find`);
- предоставляет метод `HasUser`;
- помечен как приватный (`IsPrivate() == true`).

### `ChatService` (`include/console_chat/chat_service.h`, `src/chat_service.cpp`)

Сервис бизнес-логики:
- регистрирует пользователей (`Register`);
- авторизует пользователя (`Authenticate`);
- хранит текущего пользователя через `m_currentUserLogin`;
- создаёт приватные чаты (`CreatePrivateChat`);
- возвращает список чатов пользователя (`GetMyChats`);
- отправляет/читает сообщения (`SendMessage`, `GetMessages`);
- возвращает все логины (`GetAllUserLogins`);
- выполняет выход (`Logout`).

Автоматически создаёт общий чат `GENERAL` в конструкторе.

Ограничения в сервисе:
- максимум `991` чатов на сервере (`MAX_CHATS_ON_SERVER`);
- максимум `45` приватных чатов на одного пользователя (`MAX_PRIVATE_CHATS_PER_USER`);
- длина сообщения: от `1` до `256` символов (`MAX_MESSAGE_LENGTH`);
- нельзя создать чат с самим собой;
- нельзя создать чат с несуществующим пользователем;
- нельзя создать второй приватный чат с тем же пользователем;
- имена чатов должны быть уникальны.

### `Console` (`include/console_chat/console.h`, `src/console.cpp`)

Консольный UI:
- запускает главное меню (`Run`);
- ведёт сценарии регистрации/входа;
- показывает меню пользователя;
- открывает общий/приватный чат;
- отправляет сообщения и печатает историю.

Команды в сессии чата:
- `/0` — выйти из чата;
- `/all` — показать всю историю чата.

Поведение вывода:
- показываются последние `15` сообщений (константа `LAST_MESSAGE_COUNT`);
- сообщения текущего пользователя отмечаются как `You:`.

## Точка входа

`src/main.cpp`:
- создаёт `ChatService` и `Console`;
- запускает `console.Run()`;
- ловит исключения и завершает программу с кодом `1` при фатальной ошибке.
