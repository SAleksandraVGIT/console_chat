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
# Сгенерировать файлы сборки без тестов
cmake -S . -B build -DBUILD_TESTING=OFF

# Собрать проект (параллельно по ядрам CPU)
cmake --build build -j
```

### Windows (PowerShell)

```powershell
# Сгенерировать файлы сборки без тестов
cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TESTING=OFF

# Собрать проект
cmake --build build
```

Пересборка с нуля:

### Linux (Ubuntu, bash)

```bash
rm -rf build
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build -j
```

### Windows (PowerShell)

```powershell
Remove-Item -Recurse -Force build
cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TESTING=OFF
cmake --build build
```

## Запуск тестов

Тесты написаны на GoogleTest и подключены через CTest.
Если GoogleTest не установлен в системе, CMake скачает его автоматически при конфигурации проекта.
Тестовая конфигурация находится в `tests/CMakeLists.txt` и подключается только при `BUILD_TESTING=ON`.

### Linux (Ubuntu, bash)

```bash
# Сгенерировать файлы сборки с включенными тестами
cmake -S . -B build -DBUILD_TESTING=ON

# Собрать unit-, integration- и e2e-тесты
cmake --build build --target console_chat_unit_tests console_chat_integration_tests console_chat_e2e_tests -j

# Запустить все тесты
ctest --test-dir build --output-on-failure
```

### Windows (PowerShell)

```powershell
# Сгенерировать файлы сборки с включенными тестами
cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TESTING=ON

# Собрать unit-, integration- и e2e-тесты
cmake --build build --target console_chat_unit_tests console_chat_integration_tests console_chat_e2e_tests

# Запустить все тесты
ctest --test-dir build --output-on-failure
```

Подробная информация о структуре тестов и списке проверок находится в [документации по тестам](tests/README.md).

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

## Диаграммы проекта

- [Описание архитектуры, классов и протокола](doc/architecture.md).
- [Клиентская часть](doc/client.puml) — классы клиента и последовательность выполнения запроса.
- [Сервис и доменная модель](doc/service.puml) — серверные модули, `ChatService` и основные сущности.
- [Взаимодействие компонентов](doc/components.puml) — общая архитектура и обмен данными между клиентом и сервером.

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

### 2

- Добавть для User возможность настройки профиля (удалить аккаунт, заменить пароль, заменить имя, заменить логин)

### 3

- Заменить логин на почту (сделать проверку логина регуляркой)

### 4

- Добавть автоматическое обновление чата
