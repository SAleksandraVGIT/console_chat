add_library(chat_qt_ui STATIC
  app.cpp app.h
  theme.cpp theme.h
  session_types.h
  client_worker.cpp client_worker.h
  session_controller.cpp session_controller.h
  widgets/login_page.cpp widgets/login_page.h
  widgets/chat_page.cpp widgets/chat_page.h
  widgets/main_window.cpp widgets/main_window.h
)
set_target_properties(chat_qt_ui PROPERTIES AUTOMOC ON)
target_include_directories(chat_qt_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chat_qt_ui PUBLIC chat_client_core Qt6::Widgets)

add_executable(chat_gui main.cpp)
target_link_libraries(chat_gui PRIVATE chat_qt_ui)
set_target_properties(chat_gui PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
