add_executable(editor_widget_tests EditorWidgetTest.cpp)
target_link_libraries(editor_widget_tests PRIVATE vinson_editor_ui Qt6::Test)

add_test(NAME editor_widget_tests COMMAND editor_widget_tests)
set_tests_properties(editor_widget_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(file_core_tests FileCoreTest.cpp)
target_link_libraries(file_core_tests PRIVATE vinson_core Qt6::Test)
add_test(NAME file_core_tests COMMAND file_core_tests)

add_executable(document_history_tests DocumentHistoryTest.cpp)
target_link_libraries(document_history_tests PRIVATE vinson_core Qt6::Test)
add_test(NAME document_history_tests COMMAND document_history_tests)

add_executable(file_change_monitor_tests FileChangeMonitorTest.cpp)
target_link_libraries(file_change_monitor_tests PRIVATE vinson_core Qt6::Test)
add_test(NAME file_change_monitor_tests COMMAND file_change_monitor_tests)

add_executable(recovery_manager_tests RecoveryManagerTest.cpp)
target_link_libraries(recovery_manager_tests PRIVATE vinson_core Qt6::Test)
add_test(NAME recovery_manager_tests COMMAND recovery_manager_tests)

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

add_executable(tray_controller_tests TrayControllerTest.cpp)
target_link_libraries(tray_controller_tests
    PRIVATE vinson_editor_ui Qt6::Test)
add_test(NAME tray_controller_tests COMMAND tray_controller_tests)
set_tests_properties(tray_controller_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(global_shortcut_tests GlobalShortcutTest.cpp)
target_link_libraries(global_shortcut_tests
    PRIVATE vinson_editor_ui Qt6::Test)
add_test(NAME global_shortcut_tests COMMAND global_shortcut_tests)
set_tests_properties(global_shortcut_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(settings_manager_tests SettingsManagerTest.cpp)
target_link_libraries(settings_manager_tests PRIVATE vinson_editor_ui Qt6::Test)
add_test(NAME settings_manager_tests COMMAND settings_manager_tests)
set_tests_properties(settings_manager_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(VINSON_TRANSLATIONS_ENABLED)
    add_executable(localization_tests LocalizationTest.cpp)
    target_link_libraries(localization_tests PRIVATE vinson_editor_ui Qt6::Test)
    add_test(NAME localization_tests COMMAND localization_tests)
    set_tests_properties(localization_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

add_executable(main_window_persistence_tests MainWindowPersistenceTest.cpp)
target_link_libraries(main_window_persistence_tests
    PRIVATE vinson_editor_ui Qt6::Test)
add_test(NAME main_window_persistence_tests COMMAND main_window_persistence_tests)

add_executable(single_instance_tests
    SingleInstanceTest.cpp
    ../src/app/SingleInstance.cpp
    ../src/app/SingleInstance.h)
target_include_directories(single_instance_tests PRIVATE ../src)
target_link_libraries(single_instance_tests PRIVATE Qt6::Test)
add_test(NAME single_instance_tests COMMAND single_instance_tests)
set_tests_properties(main_window_persistence_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

set(vinson_test_targets
    editor_widget_tests
    file_core_tests
    document_history_tests
    file_change_monitor_tests
    recovery_manager_tests
    search_controller_tests
    appearance_tests
    window_controller_tests
    tray_controller_tests
    global_shortcut_tests
    settings_manager_tests
    main_window_persistence_tests
    single_instance_tests)

if(TARGET localization_tests)
    list(APPEND vinson_test_targets localization_tests)
endif()

if(WIN32)
    add_test(NAME application_smoke_test
        COMMAND $<TARGET_FILE:vinson-editor> --smoke-test)
    set_tests_properties(application_smoke_test PROPERTIES
        TIMEOUT 20
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>")

    # Keep test executables runnable both through CTest and directly from an
    # IDE. Windows does not use an RPATH for Qt DLL discovery, so relying on a
    # developer-specific PATH makes every fresh build fail before QtTest starts.
    add_custom_target(vinson_test_runtime ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Core>"
            "$<TARGET_FILE:Qt6::Gui>"
            "$<TARGET_FILE:Qt6::Widgets>"
            "$<TARGET_FILE:Qt6::Core5Compat>"
            "$<TARGET_FILE:Qt6::Test>"
            "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
            "${CMAKE_CURRENT_BINARY_DIR}/platforms"
        VERBATIM)
    foreach(test_target IN LISTS vinson_test_targets)
        add_dependencies(${test_target} vinson_test_runtime)
    endforeach()

    # Prefer the Windows system ICU shim over unrelated ICU builds that may be
    # present on the caller's PATH, then expose the matching Qt runtime.
    set_tests_properties(${vinson_test_targets} PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$ENV{SystemRoot}/System32;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
endif()
