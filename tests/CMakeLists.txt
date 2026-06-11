# Сначала пробуем найти установленный в системе GoogleTest.
find_package(GTest QUIET)

# Если GoogleTest не найден, скачиваем его на этапе конфигурации через FetchContent.
if (NOT GTest_FOUND)
  include(FetchContent)

  # На Windows заставляет GoogleTest использовать тот же CRT, что и основной проект.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  # Описывает внешний проект GoogleTest, который CMake сможет скачать и собрать.
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  # Делает цели GoogleTest доступными в текущем проекте.
  FetchContent_MakeAvailable(googletest)
endif()

# Подключает CMake-функцию gtest_discover_tests.
include(GoogleTest)

# Создает исполняемый файл с unit-тестами.
add_executable(console_chat_unit_tests
  unit/base_chat.cpp
  unit/chat_service.cpp
  unit/password_protector.cpp
  unit/private_chat.cpp
  unit/user.cpp
)

# Линкует unit-тесты с кодом проекта и gtest_main, который содержит main() для тестов.
target_link_libraries(console_chat_unit_tests
  PRIVATE
    chat_core
    GTest::gtest_main
)

# Автоматически находит TEST/TEST_F из бинаря и регистрирует их в CTest.
gtest_discover_tests(console_chat_unit_tests
  PROPERTIES LABELS unit
)

# Создает исполняемый файл с интеграционными тестами.
add_executable(console_chat_integration_tests
  ${CMAKE_SOURCE_DIR}/src/server/protocol.cpp
  ${CMAKE_SOURCE_DIR}/src/server/request_router.cpp
  integration/request_flow.cpp
)

# Заголовки сервера лежат рядом с исходниками в src/server.
target_include_directories(console_chat_integration_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
)

# Интеграционные тесты проверяют связку серверного роутера с chat_core.
target_link_libraries(console_chat_integration_tests
  PRIVATE
    chat_core
    GTest::gtest_main
)

# Регистрирует интеграционные тесты в CTest отдельной меткой.
gtest_discover_tests(console_chat_integration_tests
  PROPERTIES LABELS integration
)

# Создает исполняемый файл с end-to-end тестами через реальные TCP-сокеты.
add_executable(console_chat_e2e_tests
  ${CMAKE_SOURCE_DIR}/src/server/protocol.cpp
  ${CMAKE_SOURCE_DIR}/src/server/request_router.cpp
  ${CMAKE_SOURCE_DIR}/src/server/session.cpp
  integration/client_server_e2e.cpp
)

target_include_directories(console_chat_e2e_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
)

target_link_libraries(console_chat_e2e_tests
  PRIVATE
    chat_core
    GTest::gtest_main
)

# End-to-end тесты имеют отдельную метку e2e.
gtest_discover_tests(console_chat_e2e_tests
  PROPERTIES LABELS e2e
)
