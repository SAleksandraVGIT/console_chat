# Архитектура Console Chat

## Цели сборки

- `chat_core`: серверная предметная область и хранилища; без Qt и консольного UI.
- `chat_network`: общий `TcpSocket`, без Qt и зависимости от MySQL.
- `chat_client_core`: `ChatClient` и `ClientConfig`, зависит от `chat_network`.
- `chat_console_ui`: `ChatConsole` и `LiveView`, зависит от `chat_client_core`.
- `chat_qt_ui`: необязательные Qt Widgets, контроллер и worker, зависит от
  `chat_client_core` и Qt Widgets; без зависимости от консольного UI или хранилищ.
- `chat_server`: серверные обработчики + `chat_core` + `chat_network`.
- `console_chat`: консольная точка входа; при `BUILD_QT_CLIENT=ON` также подключает
  `chat_qt_ui` для выбора `--ui console|qt`.
- `chat_gui`: отдельная Qt-точка входа + `chat_qt_ui`.

Тип UI серверу не передаётся: все клиенты используют одинаковые команды и проверки доступа.
Подробнее: [Qt-клиент](qt_client.md), [схема Qt-слоёв](qt_client.puml).

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
    - хранит имя (`m_name`), хеш пароля (`m_passwordHash`) и состояние бана;
    - `GetName()` возвращает имя;
    - `CheckPassword(...)` делегирует проверку в `PasswordProtector`;
    - `IsBannedAt(...)` проверяет активен ли бан по серверному времени;
    - `SetBan(...)` обновляет временный или вечный бан.

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
    - создание приватного чата, включая чат пользователя с собой (`CreatePrivateChat`);
    - получение списка чатов пользователя (`GetMyChats`);
    - получение сообщений (`GetMessages`);
    - отправка сообщений (`SendMessage`);
    - получение списка пользователей (`GetAllUserLogins`);
    - админский просмотр пользователей и чатов (`GetAllUsersInfo`, `GetAllChatsInfo`);
    - админский просмотр любого чата (`GetMessagesForAdmin`);
    - отправка сообщения в общий чат от имени `ADMIN` (`SendAdminMessageToGeneral`);
    - создание приватного чата администратора с любым пользователем (`CreateAdminPrivateChat`);
    - отправка сообщения от `ADMIN` в админские приватные чаты (`SendAdminMessageToChat`);
    - бан и досрочный разбан пользователей (`BanUser`, `UnbanUser`);
    - удаление своего аккаунта пользователем (`DeleteUserAccount`);
    - отдельное удаление всех аккаунтов с вечным баном (`DeleteForeverBannedUsers`);
    - загрузка начального состояния (`Initialize`, `ImportState`);
    - точечное сохранение пользователя, чата или сообщения через `IManager` до изменения памяти.
  - хранит:
    - `m_users` — зарегистрированные пользователи;
    - `m_chats` — чаты на сервере.
  - автоматически создаёт чат `GENERAL` в конструкторе.

`ChatService` не хранит глобальную авторизацию процесса; текущий логин передаётся в методы явно.
Администратор не хранится в `m_users` как обычный пользователь. Админская авторизация хранится
в серверной сессии, а `ChatService` предоставляет только доменные операции, которые сервер
вызывает после проверки прав.
Для приватных чатов администратора используется служебный логин `__admin__`; наружу он
показывается как имя отправителя `ADMIN` и не попадает в список обычных пользователей.
Логины `ADMIN` в любом регистре и `__admin__` зарезервированы и не могут использоваться
при регистрации обычного пользователя.

### Storage (`namespace console_chat::storage`)

- `IManager`
  - `Initialize` подготавливает хранилище;
  - `Load` загружает полный `ServiceState` при запуске;
  - `AddUser` сохраняет одного нового пользователя;
  - `AddChat` сохраняет один чат и его участников;
  - `AddMessage` сохраняет одно сообщение и получает логин автора для будущего внешнего ключа;
  - `AddAdminMessage` сохраняет сообщение администратора в общем или admin-private чате;
  - `UpdateUserBan` сохраняет временный или вечный бан пользователя;
  - `DeletePrivateChatsWithUser` удаляет приватные чаты пользователя;
  - `DeleteUser` удаляет пользователя и связанные с ним приватные чаты;
  - `Reset` очищает хранилище.
- `FileManager`
  - поддерживает снимок `ServiceState` в памяти;
  - после точечной операции обновляет снимок и перезаписывает прежние два файла.
- `MySQLManager`
  - использует MySQL C API;
  - SQL-константы хранятся во внутреннем файле `src/storage/mysql_queries.h`;
  - принимает `MySQLConfig` с дефолтными host, port, user, database, timeout в `std::chrono::seconds` и charset;
  - функция `LoadMySQLConfig` загружает эти параметры из локального файла `key=value`;
  - пароль по умолчанию пустой и не хранится в исходном коде;
  - конфигурация проверяется отдельным методом `ValidateConfig`;
  - `Initialize` создаёт клиентский handle, задаёт timeout и charset, затем открывает соединение;
  - `Load` восстанавливает пользователей, чаты и сообщения тремя `SELECT`;
  - `AddUser` сохраняет пользователя и хеш пароля двумя prepared statements в транзакции;
  - `AddChat` создаёт общий или приватный чат и разрешает логины участников в их id;
  - `AddMessage` проверяет существование отправителя, доступ к чату и лимит сообщений;
  - `AddAdminMessage` сохраняет сообщение от скрытого служебного пользователя `__admin__`;
  - при создании admin-private чата MySQL создаёт `__admin__`, если его ещё нет;
  - `UpdateUserBan` обновляет поля `banned_until_epoch` и `banned_forever`;
  - `DeletePrivateChatsWithUser` удаляет сообщения и приватные чаты с участием пользователя;
  - `DeleteUser` удаляет приватные чаты пользователя, его сообщения, пароль и сам аккаунт;
  - `Reset` очищает таблицы в транзакции с учётом внешних ключей.

Локальная конфигурация `config/mysql.conf` создаётся скриптом `scripts/setup_mysql.sh`,
имеет права `600` и исключена из Git. Сервер выбирает `FileManager` или `MySQLManager`
через параметр `--storage file|mysql`; для MySQL путь к конфигурации задаётся
параметром `--mysql-config`.

Для подключения другой базы данных достаточно реализовать новый менеджер через `IManager`.
Роутер, сессии и `ChatService` от конкретного формата хранения не зависят.

### Client (`namespace console_chat::client`)

- `ChatClient` (`include/console_chat/client/chat_client.h`, `src/client/chat_client.cpp`)
  - транспортный клиент поверх TCP:
    - формирует команды протокола;
    - отправляет запросы на сервер;
    - получает и разбирает ответы.
  - предоставляет API для `ChatConsole`:
    - `Register`, `Authenticate`, `Logout`, `DeleteAccount`;
    - `AuthenticateDetailed` для получения причины бана и серверного времени;
    - `GetMyChats`, `GetMessages`, `SendMessage`;
    - `GetAllUserLogins`, `GetCurrentUserLogin`, `GetCurrentUserName`, `IsAuthenticated`;
    - админские методы `AdminLogin`, `AdminGetUsers`, `AdminGetChats`,
      `AdminCreatePrivateChat`, `AdminGetMessages`, `AdminSendMessageToChat`,
      `AdminSendGeneral`, `AdminKickUser`, `AdminBanUser`, `AdminUnbanUser`,
      `AdminDeleteForeverBannedUsers`.
  - методы получения списков и сообщений принимают `background=false`; при `true`
    запрос оборачивается в `POLL`, который не продлевает серверный таймаут;
  - `NotifyActivity()` отправляет `ACTIVITY` при вводе в открытом экране.

- `ChatConsole` (`include/console_chat/client/chat_console.h`, `src/client/chat_console.cpp`)
  - консольный UI:
    - главное меню и меню пользователя;
    - отдельный админский режим `RunAdmin()` для запуска через `console_chat --admin`;
    - сценарии регистрации/входа;
    - сценарий админского входа;
    - открытие общего и приватных чатов;
    - удаление своего аккаунта с подтверждением текущего login;
    - отправка сообщений и вывод истории;
    - админский просмотр всех пользователей и всех чатов, отдельное открытие `GENERAL`
      и полной истории любого открытого чата;
    - создание и открытие приватных чатов `ADMIN <-> пользователь`.
  - команды в сессии чата:
    - `/0` — выход из чата;
    - `/all` — печать всей истории.

- `LiveView` (`include/console_chat/client/live_view.h`, `src/client/live_view.cpp`)
  - единый цикл ввода и опроса по `std::chrono::steady_clock`, без фонового сетевого потока;
  - POSIX: `poll` и временный режим `termios`; Windows: события консоли и VT-вывод;
  - отдельный экран терминала, сохранение черновика и курсора, прокрутка и обработка размера окна;
  - подсказки команд закреплены над строкой ввода и переносятся по ширине терминала;
  - снимок экрана строится через `ostringstream`, перерисовка нужна только при изменении
    содержимого, ввода, прокрутки или размеров терминала;
  - ввод передаёт `ACTIVITY` не чаще раза в секунду, Enter передаёт его сразу;
  - RAII восстанавливает режим терминала при выходе, EOF, Ctrl+C/Ctrl+D и исключениях;
  - без интерактивного терминала используется обычный построчный ввод без таймера.

- `ClientConfig` / `LoadClientConfig` задают `RefreshInterval` (по умолчанию 3000 мс),
  загружают `refresh_interval_ms=100..60000` из `config/client.conf`.
  Неизвестные ключи, дубликаты и некорректные значения отклоняются.

- `src/client/main.cpp`
  - точка входа клиента:
    - разбирает флаг `--admin`;
    - загружает клиентский конфиг, путь меняется через `--client-config`;
    - создаёт `ChatClient`;
    - создаёт `ChatConsole`;
    - запускает `Run()` или `RunAdmin()`.
    - при `--ui qt` передаёт запуск Qt-точке композиции `RunApplication`.

### Qt (`namespace console_chat::qt`)

- `LoginPage`, `ChatPage`, `MainWindow`: только виджеты, локальные черновики,
  отображение состояния и подтверждения действий; без сокетов и SQL.
- `SessionController`: живёт в GUI-потоке, планирует опрос и уведомления активности,
  ставит пользовательские операции в очередь, публикует состояние и ошибки.
- `ClientWorker`: живёт в отдельном `QThread`, единолично владеет `ChatClient`;
  последовательно выполняет запросы, переводит ответы в `SessionSnapshot`.
- `session_types.h`: значения для передачи между слоями; снимки передаются сигналами
  Qt по значению, виджеты никогда не обращаются к рабочему объекту напрямую.
- `theme.*`: оформление Widgets, отдельно от сетевой логики и поведения окон.

Все проверки полномочий остаются на сервере. GUI скрывает недоступные действия и
отключает отправку в чужие приватные чаты ADMIN. Разделения на `IClient`/AdminClient нет.
При обычном входе отправляется `LOGIN`, при админском `ADMIN_LOGIN`; админские реквизиты
по-прежнему читает только сервер из `admin.conf`.

### Server (`namespace console_chat::server`)

- `src/server/main.cpp` — запуск TCP-сервера, загрузка `admin.conf`, `server.conf` и `accept`-цикл.
- `admin_config.*` — загрузка единого логина и пароля администратора из `key=value` файла.
- `server_config.*` — загрузка серверных лимитов и idle-timeout из `key=value` файла.
- `request_router.*` — обработка команд протокола, `RequestContext` и проверка админ-режима.
- `protocol.*` — разбор и сборка строкового протокола.
- `session.*` — обработка клиентской сессии, установка настроенного idle-timeout
  и `SessionRegistry` для отключения активных пользователей.

### Network (`namespace console_chat::network`)

- `TcpSocket` (`include/console_chat/network/tcp_socket.h`, `src/network/tcp_socket.cpp`)
  - обёртка над системным TCP-сокетом:
    - `Connect(...)`;
    - необязательный timeout подключения и `SetSendTimeout(...)` для Qt-worker;
    - `BindAndListen(...)`;
    - `Accept()`;
    - `SendLine(...)`/`RecvLine(...)`;
    - `Close()`;
    - `SetReceiveTimeout(...)`;
    - `WasLastReceiveTimedOut()`.

## Протокол

Обмен идёт строками по TCP:

- разделитель полей: `\t`;
- конец сообщения: `\n`.

Основные команды:

- `REGISTER`;
- `LOGIN`;
- `LOGOUT`;
- `CUR_LOGIN`;
- `DELETE_ACCOUNT`;
- `GET_MY_CHATS`;
- `GET_MESSAGES`;
- `SEND_MESSAGE`;
- `ADMIN_LOGIN`;
- `ADMIN_GET_USERS`;
- `ADMIN_GET_CHATS`;
- `ADMIN_CREATE_PRIVATE`;
- `ADMIN_GET_MESSAGES`;
- `ADMIN_SEND_CHAT`;
- `ADMIN_SEND_GENERAL`;
- `ADMIN_KICK_USER`;
- `ADMIN_BAN_USER`;
- `ADMIN_UNBAN_USER`;
- `ADMIN_DELETE_FOREVER_BANNED_USERS`.

Ответ на вход забаненного пользователя:

- `ERR banned TEMP <ban_until_epoch> <server_now_epoch>` — временный бан;
- `ERR banned FOREVER 0 <server_now_epoch>` — вечный бан.

Ответ на повторное создание приватного чата с тем же участником:

- `ERR chat already exists <chat_name>` — приватный чат с такой парой участников уже есть.

Если пары участников ещё нет, но имя занято другим чатом (включая `GENERAL`),
сервер возвращает `ERR chat name already in use`. Этот ответ не содержит участников
или сообщений существующего чата и не является командой его открытия.

Сроки бана задаются серверу строками `1d`, `10d`, `1m`, `1y`, `forever`.
Время окончания бана считается на сервере через `std::chrono` от момента выдачи бана.
Клиент выводит оставшееся время, используя переданное сервером текущее время.
`ADMIN_GET_CHATS` возвращает все чаты, включая `GENERAL`. Для приватных чатов ответ содержит
двух участников, для `GENERAL` поля участников пустые. `GENERAL` открывается отдельной
командой клиента через `ADMIN_GET_MESSAGES GENERAL`.
`CUR_LOGIN` используется клиентским меню, чтобы пометить текущего пользователя в списке
логинов как `(You)`.
`DELETE_ACCOUNT <login>` удаляет аккаунт текущего пользователя только если переданный login
совпадает с login в сессии; после успешного удаления сессия разлогинивается.
`ADMIN_DELETE_FOREVER_BANNED_USERS` удаляет все аккаунты, у которых стоит вечный бан, и
возвращает количество удалённых пользователей.
При `ADMIN_KICK_USER` сервер отправляет служебный ответ `ERR disconnected by ADMIN` и закрывает
сокет. Из-за синхронного протокола клиент показывает это сообщение при следующем запросе к
серверу, включая ближайший тик открытого списка или чата. После настроенного idle-timeout
без пользовательских действий сервер отправляет
`ERR disconnected by inactivity timeout` и завершает сессию.

`POLL <read-command> [chatName]` допускается только после входа. Разрешены
`GET_MY_CHATS`, `GET_ALL_USERS`, `GET_MESSAGES`, `ADMIN_GET_USERS`, `ADMIN_GET_CHATS`,
`ADMIN_GET_MESSAGES`; аргументы и права проверяются сервером как для обычного запроса.
Изменяющие команды и вложенный `POLL` запрещены. Ответ совпадает с ответом команды чтения.
`ACTIVITY` без аргументов подтверждает действие авторизованного пользователя или ADMIN.
Обычные запросы и `ACTIVITY` обновляют серверную отметку `steady_clock`; `POLL` её не меняет.
Перед приёмом очередного запроса сервер выставляет оставшееся время ожидания (с округлением
вверх до секунды), а после приёма проверяет срок до выполнения команды.
Автообновление требует новой версии сервера; старый сервер не поддерживает `POLL`/`ACTIVITY`.

Ответы:

- `OK ...` — успех;
- `ERR ...` — ошибка.

## Ограничения `ChatService`

По умолчанию сервер использует значения из `ServiceLimits`, совпадающие со старыми
константами:

- максимум пользователей: `0`, без ограничения (`MAX_USERS`);
- максимум `991` чатов на сервере (`MAX_CHATS_ON_SERVER`);
- максимум `45` приватных чатов на пользователя (`MAX_PRIVATE_CHATS_PER_USER`);
- длина сообщения: от `1` до `256` символов (`MAX_MESSAGE_LENGTH`);
- максимум `1000` сообщений в одном чате (`MAX_MESSAGES_PER_CHAT`);
- idle-timeout клиента: 15 минут.

Эти значения можно изменить в `config/server.conf`:

```text
max_users=0
client_timeout_minutes=15
max_chats=991
max_private_chats_per_user=45
max_message_length=256
max_messages_per_chat=1000
```

Путь к файлу задаётся параметром `--server-config`.

## Хранение состояния

- Состояние сервера сохраняется через точечные методы `IManager`.
- По умолчанию используется `FileManager`; `MySQLManager` включается через `--storage mysql`.
- `ChatService` сначала сохраняет изменение, затем изменяет состояние в памяти.
- При обычном запуске сервер пытается загрузить предыдущее состояние.
- При запуске с `--reset-state` сервер очищает файлы состояния и стартует с пустым состоянием.
- Пользователь хранит поля `BannedUntilEpoch` и `BannedForever`.
- Вечный бан только запрещает вход пользователя; удаление аккаунтов с вечным баном выполняется
  отдельной админской командой.
- Для MySQL после обновления старой базы требуется миграция `database/migrate_admin_bans.sql`.

## Диаграммы

- [Клиентская часть](client.puml).
- [Сервис и доменная модель](service.puml).
- [Взаимодействие компонентов](components.puml).
