# Требует CMake версии 3.16 или новее.
cmake_minimum_required(VERSION 3.16)

# Объявляет проект console_chat, использующий язык C++.
project(console_chat LANGUAGES CXX)

# Ищет каталог с заголовочным файлом mysql.h для будущей реализации MySQLManager.
find_path(MYSQLCLIENT_INCLUDE_DIR
  NAMES mysql.h
  PATH_SUFFIXES mysql
)

# Ищет установленную клиентскую библиотеку MySQL.
find_library(MYSQLCLIENT_LIBRARY
  NAMES mysqlclient mysql libmysql
)

# Проверяет, что найдены и заголовочные файлы, и клиентская библиотека MySQL.
if (NOT MYSQLCLIENT_INCLUDE_DIR OR NOT MYSQLCLIENT_LIBRARY)
  # Останавливает конфигурацию проекта, если компоненты MySQL не найдены.
  message(FATAL_ERROR "MySQL client library and headers were not found")
endif()

# Создаёт импортированную CMake-цель для установленной системной библиотеки MySQL.
add_library(MySQLClient::MySQLClient UNKNOWN IMPORTED)

# Указывает путь к библиотеке MySQL и каталог с её заголовочными файлами.
set_target_properties(MySQLClient::MySQLClient PROPERTIES
  IMPORTED_LOCATION "${MYSQLCLIENT_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${MYSQLCLIENT_INCLUDE_DIR}"
)

# Устанавливает стандарт C++20 для всех целей проекта.
set(CMAKE_CXX_STANDARD 20)

# Запрещает CMake автоматически понижать требуемую версию стандарта C++.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Отключает нестандартные расширения компилятора, например режим gnu++20.
set(CMAKE_CXX_EXTENSIONS OFF)

# Формирует список общих исходников для статической библиотеки chat_core.
set(CHAT_CORE_SOURCES
  src/core/base_chat.cpp
  src/core/private_chat.cpp
  src/core/password_protector.cpp
  src/core/chat_service.cpp
  src/storage/file_manager.cpp
  src/storage/mysql_queries.h
  src/storage/mysql_manager.cpp
  src/client/chat_console.cpp
  src/network/tcp_socket.cpp
  src/client/chat_client.cpp
)

# Создаёт статическую библиотеку с общей логикой клиента, сервера и хранилищ.
add_library(chat_core STATIC ${CHAT_CORE_SOURCES})

# Добавляет публичный путь к заголовочным файлам проекта.
target_include_directories(chat_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Подключает MySQL Client ко всем целям, использующим библиотеку chat_core.
target_link_libraries(chat_core PUBLIC MySQLClient::MySQLClient)

# Проверяет, выполняется ли сборка под Windows.
if (WIN32)
  # Подключает системную библиотеку Windows Sockets к chat_core.
  target_link_libraries(chat_core PUBLIC ws2_32)
endif()


# Создаёт исполняемый файл консольного клиента.
add_executable(console_chat src/client/main.cpp)

# Подключает общую библиотеку chat_core к клиенту.
target_link_libraries(console_chat PRIVATE chat_core)


# Создаёт исполняемый файл сервера из серверных исходников.
add_executable(chat_server
  src/server/main.cpp
  src/server/protocol.cpp
  src/server/request_router.cpp
  src/server/session.cpp
)
# Добавляет серверу путь к публичным заголовочным файлам проекта.
target_include_directories(chat_server PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Подключает общую библиотеку chat_core к серверу.
target_link_libraries(chat_server PRIVATE chat_core)

# Проверяет, выполняется ли сборка сервера под Windows.
if (WIN32)
  # Подключает системную библиотеку Windows Sockets к серверу.
  target_link_libraries(chat_server PRIVATE ws2_32)
endif()


# Подключает CTest и создаёт стандартную опцию BUILD_TESTING.
include(CTest)

# Проверяет, включена ли сборка тестов через BUILD_TESTING.
if (BUILD_TESTING)
  # Подключает конфигурацию тестовых целей из каталога tests.
  add_subdirectory(tests)
endif()
