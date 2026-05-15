# Минимальная версия CMake
cmake_minimum_required(VERSION 3.16)

# Имя проекта и используемый язык
project(console_chat LANGUAGES CXX)

# Настройки стандарта C++: используем строго C++20 без расширений компилятора
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Общие исходники.
# Эти файлы собираются в статическую библиотеку, которую потом линкуют исполняемые файлы.
set(CHAT_CORE_SOURCES
  src/core/base_chat.cpp
  src/core/private_chat.cpp
  src/core/password_protector.cpp
  src/core/chat_service.cpp
  src/client/chat_console.cpp
  src/network/tcp_socket.cpp
  src/client/chat_client.cpp
)

# Библиотека с общей логикой чата
add_library(chat_core STATIC ${CHAT_CORE_SOURCES})
# Публичный include-путь, чтобы зависимые цели видели заголовки из include/
target_include_directories(chat_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
if (WIN32)
  target_link_libraries(chat_core PUBLIC ws2_32)
endif()


# Клиентское приложение
add_executable(console_chat src/client/main.cpp)
# Заголовки проекта для клиента
target_link_libraries(console_chat PRIVATE chat_core)


# Серверное приложение.
add_executable(chat_server
  src/server/main.cpp
  src/server/protocol.cpp
  src/server/request_router.cpp
  src/server/session.cpp
)
# Заголовки проекта для сервера
target_include_directories(chat_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(chat_server PRIVATE chat_core)
if (WIN32)
  target_link_libraries(chat_server PRIVATE ws2_32)
endif()
