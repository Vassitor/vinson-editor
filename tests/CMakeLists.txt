# Keep target registration, QtTest linkage, and headless execution consistent.
function(vinson_add_test target source library)
    add_executable(${target} ${source} ${ARGN})
    target_link_libraries(${target} PRIVATE ${library} Qt6::Test)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    set(vinson_test_targets ${vinson_test_targets} ${target} PARENT_SCOPE)
endfunction()

vinson_add_test(editor_widget_tests EditorWidgetTest.cpp vinson_editor_ui)
vinson_add_test(plugin_manager_tests PluginManagerTest.cpp vinson_editor_ui)
vinson_add_test(file_core_tests FileCoreTest.cpp vinson_core)
vinson_add_test(document_history_tests DocumentHistoryTest.cpp vinson_core)
vinson_add_test(file_change_monitor_tests FileChangeMonitorTest.cpp vinson_core)
vinson_add_test(recovery_manager_tests RecoveryManagerTest.cpp vinson_core)
vinson_add_test(search_controller_tests SearchControllerTest.cpp vinson_editor_ui)
vinson_add_test(appearance_tests AppearanceTest.cpp vinson_editor_ui)
vinson_add_test(window_controller_tests WindowControllerTest.cpp vinson_editor_ui)
vinson_add_test(tray_controller_tests TrayControllerTest.cpp vinson_editor_ui)
vinson_add_test(global_shortcut_tests GlobalShortcutTest.cpp vinson_editor_ui)
vinson_add_test(settings_manager_tests SettingsManagerTest.cpp vinson_editor_ui)
vinson_add_test(main_window_persistence_tests MainWindowPersistenceTest.cpp vinson_editor_ui)

if(VINSON_TRANSLATIONS_ENABLED)
    vinson_add_test(localization_tests LocalizationTest.cpp vinson_editor_ui)
endif()

vinson_add_test(single_instance_tests SingleInstanceTest.cpp Qt6::Core
    ../src/app/SingleInstance.cpp
    ../src/app/SingleInstance.h)
target_include_directories(single_instance_tests PRIVATE ../src)

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
            "$<TARGET_FILE:Qt6::Qml>"
            "$<TARGET_FILE:Qt6::Network>"
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
    set_tests_properties(${vinson_test_targets} application_smoke_test PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:$ENV{SystemRoot}/System32")
endif()
