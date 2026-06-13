# Тесты

Тесты написаны на GoogleTest и подключены через CTest.

## Структура

- `CMakeLists.txt` — сборка всех тестовых целей, подключается из основного `CMakeLists.txt` при `BUILD_TESTING=ON`.
- `unit/` — unit-тесты отдельных классов и функций.
- `integration/` — интеграционные и e2e-тесты совместной работы серверного роутера, протокола, `ChatService`, TCP-клиента и файлов состояния.

## Запуск

### Linux (Ubuntu, bash)

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target console_chat_unit_tests console_chat_integration_tests console_chat_e2e_tests -j
ctest --test-dir build --output-on-failure
```

### Windows (PowerShell)

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DBUILD_TESTING=ON
cmake --build build --target console_chat_unit_tests console_chat_integration_tests console_chat_e2e_tests
ctest --test-dir build --output-on-failure
```

Запуск только одной группы:

```bash
ctest --test-dir build -L unit --output-on-failure
ctest --test-dir build -L integration --output-on-failure
ctest --test-dir build -L e2e --output-on-failure
```

## Unit-Тесты

### BaseChat

| Тест | Что проверяет |
| --- | --- |
| `BaseChat.Defaults` | Общий чат доступен любому пользователю, не является приватным и сначала пустой. |
| `BaseChat.AddMessage` | Сообщения добавляются и сохраняют порядок. |
| `BaseChat.MessageLimit` | После лимита в `1000` сообщений новые сообщения отклоняются. |

### ChatService

| Тест | Что проверяет |
| --- | --- |
| `ChatService.RegisterLogin` | Регистрация, запрет дублей, авторизация и получение имени по логину. |
| `ChatService.SortedLogins` | Список логинов возвращается в отсортированном виде. |
| `ChatService.GeneralChat` | Зарегистрированный пользователь видит чат `GENERAL`. |
| `ChatService.GeneralMessages` | Отправка и чтение сообщений в общем чате, а также базовые ошибки отправки. |
| `ChatService.MessageLimit` | Сообщение длиной `256` символов принимается, `257` символов отклоняется. |
| `ChatService.PrivateChat` | Создание приватного чата и запрет некорректных вариантов. |
| `ChatService.PrivateAccess` | Только участники приватного чата могут писать и читать его сообщения. |
| `ChatService.PrivateLimit` | Ограничение на `45` приватных чатов для пользователя. |
| `ChatService.StorageFailureDoesNotChangeMemoryState` | Ошибка точечной записи пользователя, чата или сообщения не изменяет состояние в памяти. |
| `ChatServiceState.SaveLoad` | Сохранение и загрузка пользователей, чатов и сообщений. |
| `ChatServiceState.InvalidLoad` | Неудачная загрузка не ломает текущее состояние сервиса. |

### PasswordProtector

| Тест | Что проверяет |
| --- | --- |
| `PasswordProtector.Verify` | Хеш пароля проходит проверку только с исходным паролем. |
| `PasswordProtector.StableHash` | Один и тот же пароль дает одинаковый хеш. |
| `PasswordProtector.DifferentHash` | Разные пароли дают разные хеши. |
| `PasswordProtector.HashPrefix` | Проверка признака хеша по ожидаемому префиксу. |

### PrivateChat

| Тест | Что проверяет |
| --- | --- |
| `PrivateChat.Users` | Приватный чат хранит двух участников. |
| `PrivateChat.Access` | Доступ есть только у участников приватного чата. |
| `PrivateChat.UserOrder` | `GetUsers()` возвращает участников в порядке конструктора. |

### User

| Тест | Что проверяет |
| --- | --- |
| `User.StoresData` | Пользователь хранит имя и хеш пароля. |
| `User.CheckPassword` | Проверка пароля работает через сохраненный хеш. |

## Integration-Тесты

### Protocol

| Тест | Что проверяет |
| --- | --- |
| `Protocol.SplitJoin` | Разбор и сборку строк протокола с разделителем `\t`. |

### RequestFlow

| Тест | Что проверяет |
| --- | --- |
| `RequestFlow.GeneralChat` | Полный сценарий команд: статус авторизации, регистрация, вход, общий чат, отправка сообщения, выход. |
| `RequestFlow.PrivateChat` | Сценарий нескольких сессий: пользователи, приватный чат, доступ участников и запрет доступа третьему пользователю. |
| `RequestFlow.StateReload` | Сохранение состояния через серверные команды и загрузку этого состояния новым `ChatService`. |
| `RequestFlow.BadRequests` | Ответы роутера на пустые, неизвестные и некорректные команды. |

## E2E-Тесты

E2E-тесты поднимают тестовый сервер в отдельном потоке и подключают к нему реальные `ChatClient` через TCP на `127.0.0.1`.

### ClientServerE2E

| Тест | Что проверяет |
| --- | --- |
| `ClientServerE2E.GeneralChat` | Два клиента подключаются по TCP, регистрируются, входят и обмениваются сообщением в `GENERAL`. |
| `ClientServerE2E.PrivateChat` | Три TCP-клиента проверяют создание приватного чата, доставку сообщения участнику и запрет доступа третьему пользователю. |
| `ClientServerE2E.StatePersistsAcrossServerRestart` | Сообщение сохраняется в файлы состояния и доступно после перезапуска тестового сервера. |
