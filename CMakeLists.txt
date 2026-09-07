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

# Формирует список исходников серверной предметной области и хранилищ.
set(CHAT_CORE_SOURCES
  src/core/base_chat.cpp
  src/core/private_chat.cpp
  src/core/password_protector.cpp
  src/core/chat_service.cpp
  src/storage/file_manager.cpp
  src/storage/mysql_queries.h
  src/storage/mysql_manager.cpp
)

# Серверная предметная область и хранилища не зависят от интерфейсов клиента.
add_library(chat_core STATIC ${CHAT_CORE_SOURCES})

# Добавляет публичный путь к заголовочным файлам проекта.
target_include_directories(chat_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Подключает MySQL Client ко всем целям, использующим библиотеку chat_core.
target_link_libraries(chat_core PUBLIC MySQLClient::MySQLClient)

# Общий транспорт и клиентский API не зависят от UI и хранилищ.
add_library(chat_network STATIC src/network/tcp_socket.cpp)
target_include_directories(chat_network PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(chat_client_core STATIC
  src/client/chat_client.cpp
  src/client/client_config.cpp
)
target_link_libraries(chat_client_core PUBLIC chat_network)

add_library(chat_console_ui STATIC
  src/client/chat_console.cpp
  src/client/live_view.cpp
)
target_link_libraries(chat_console_ui PUBLIC chat_client_core)

if (WIN32)
  # Подключает системную библиотеку Windows Sockets к транспорту.
  target_link_libraries(chat_network PUBLIC ws2_32)
endif()


# Создаёт исполняемый файл консольного клиента.
add_executable(console_chat src/client/main.cpp)

# Подключает консольное представление и общий клиентский API.
target_link_libraries(console_chat PRIVATE chat_console_ui)

option(BUILD_QT_CLIENT "Build the optional Qt Widgets client (Qt 6.4+)" OFF)
if (BUILD_QT_CLIENT)
  find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
  add_subdirectory(src/qt)
  target_link_libraries(console_chat PRIVATE chat_qt_ui)
  target_compile_definitions(console_chat PRIVATE CONSOLE_CHAT_WITH_QT)
endif()


# Создаёт исполняемый файл сервера из серверных исходников.
add_executable(chat_server
  src/server/main.cpp
  src/server/admin_config.cpp
  src/server/server_config.cpp
  src/server/protocol.cpp
  src/server/request_router.cpp
  src/server/session.cpp
)
# Добавляет серверу путь к публичным заголовочным файлам проекта.
target_include_directories(chat_server PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Подключает общую библиотеку chat_core к серверу.
target_link_libraries(chat_server PRIVATE chat_core chat_network)

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
