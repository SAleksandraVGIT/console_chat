# Минимальная версия CMake
cmake_minimum_required(VERSION 3.16)

# Название проекта
project(console_chat LANGUAGES CXX)

# Подключаем CMake модуль для C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Подключение всех вложеных файлов с расширением cpp
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Создаём исполняемый файл
add_executable(console_chat ${SOURCES})

# Указываем путь к заголовочным файлам для console_chat
target_include_directories(console_chat PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)
