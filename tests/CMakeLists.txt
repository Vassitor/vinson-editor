add_executable(editor_widget_tests EditorWidgetTest.cpp)
target_link_libraries(editor_widget_tests PRIVATE vinson_editor_ui Qt6::Test)

add_test(NAME editor_widget_tests COMMAND editor_widget_tests)
set_tests_properties(editor_widget_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(file_core_tests FileCoreTest.cpp)
target_link_libraries(file_core_tests PRIVATE vinson_core Qt6::Test)
add_test(NAME file_core_tests COMMAND file_core_tests)

add_executable(search_controller_tests SearchControllerTest.cpp)
target_link_libraries(search_controller_tests PRIVATE vinson_editor_ui Qt6::Test)
add_test(NAME search_controller_tests COMMAND search_controller_tests)
set_tests_properties(search_controller_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(appearance_tests AppearanceTest.cpp)
target_link_libraries(appearance_tests PRIVATE vinson_editor_ui Qt6::Test)
add_test(NAME appearance_tests COMMAND appearance_tests)
set_tests_properties(appearance_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(window_controller_tests WindowControllerTest.cpp)
target_link_libraries(window_controller_tests PRIVATE vinson_editor_ui Qt6::Test)
add_test(NAME window_controller_tests COMMAND window_controller_tests)
set_tests_properties(window_controller_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(settings_manager_tests SettingsManagerTest.cpp)
target_link_libraries(settings_manager_tests PRIVATE vinson_editor_ui Qt6::Test)
add_test(NAME settings_manager_tests COMMAND settings_manager_tests)
set_tests_properties(settings_manager_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(main_window_persistence_tests MainWindowPersistenceTest.cpp)
target_link_libraries(main_window_persistence_tests
    PRIVATE vinson_editor_ui Qt6::Test)
add_test(NAME main_window_persistence_tests COMMAND main_window_persistence_tests)
set_tests_properties(main_window_persistence_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
