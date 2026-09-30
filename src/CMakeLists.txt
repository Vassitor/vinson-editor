add_library(vinson_core STATIC
    editor/EditorDocument.cpp
    editor/EditorDocument.h
    file/EncodingDetector.cpp
    file/EncodingDetector.h
    file/FileLoader.cpp
    file/FileLoader.h
    file/FileChangeMonitor.cpp
    file/FileChangeMonitor.h
    file/FileManager.cpp
    file/FileManager.h
    file/FileSaver.cpp
    file/FileSaver.h
    file/FileTypes.cpp
    file/FileTypes.h
    largefile/LargeFilePolicy.cpp
    largefile/LargeFilePolicy.h
    session/DocumentHistory.cpp
    session/DocumentHistory.h
    session/RecoveryManager.cpp
    session/RecoveryManager.h
)
target_include_directories(vinson_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vinson_core PUBLIC Qt6::Core)

add_library(vinson_editor_ui STATIC
    editor/EditorWidget.cpp
    editor/EditorWidget.h
    search/SearchController.cpp
    search/SearchController.h
    search/SearchTypes.h
    settings/Appearance.h
    settings/AppearanceIO.cpp
    settings/AppearanceIO.h
    settings/Localization.cpp
    settings/Localization.h
    settings/SettingsManager.cpp
    settings/SettingsManager.h
    settings/ThemeManager.cpp
    settings/ThemeManager.h
    ui/FindReplaceWidget.cpp
    ui/FindReplaceWidget.h
    ui/EditHistoryWidget.cpp
    ui/EditHistoryWidget.h
    ui/MenuAppearance.cpp
    ui/MenuAppearance.h
    ui/SettingsDialog.cpp
    ui/SettingsDialog.h
    window/NativeWindowAppearance.cpp
    window/NativeWindowAppearance.h
    window/NativeTitleBar.cpp
    window/NativeTitleBar.h
    window/GlobalShortcut.cpp
    window/GlobalShortcut.h
    window/TrayController.cpp
    window/TrayController.h
    window/WindowController.cpp
    window/WindowController.h
    window/MainWindow.cpp
    window/MainWindow.h
)
target_include_directories(vinson_editor_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vinson_editor_ui
    PUBLIC vinson_core Qt6::Widgets Scintilla::Scintilla)

qt_add_resources(vinson_editor_ui vinson_settings_icons
    PREFIX "/settings"
    BASE "${PROJECT_SOURCE_DIR}/resources/icons/settings"
    FILES
        "${PROJECT_SOURCE_DIR}/resources/icons/settings/chevron-down.xpm"
        "${PROJECT_SOURCE_DIR}/resources/icons/settings/chevron-up.xpm"
        "${PROJECT_SOURCE_DIR}/resources/icons/settings/chevron-down-disabled.xpm"
        "${PROJECT_SOURCE_DIR}/resources/icons/settings/chevron-up-disabled.xpm")

if(WIN32)
    target_link_libraries(vinson_editor_ui PRIVATE dwmapi)
endif()

set(VINSON_TRANSLATIONS_ENABLED OFF)
if(Qt6LinguistTools_FOUND)
    qt_add_translations(vinson_editor_ui
        SOURCE_TARGETS vinson_core vinson_editor_ui
        TS_FILES "${PROJECT_SOURCE_DIR}/resources/i18n/vinson-editor_zh_CN.ts"
        RESOURCE_PREFIX "/i18n"
        MERGE_QT_TRANSLATIONS
        QT_TRANSLATION_CATALOGS qtbase)
    set(VINSON_TRANSLATIONS_ENABLED ON)

    set(vinson_qtbase_zh_cn_qm
        "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}/qtbase_zh_CN.qm")
    if(EXISTS "${vinson_qtbase_zh_cn_qm}")
        get_filename_component(vinson_qt_translations_dir
            "${vinson_qtbase_zh_cn_qm}" DIRECTORY)
        qt_add_resources(vinson_editor_ui vinson_qtbase_zh_cn_translations
            PREFIX "/i18n/qt"
            BASE "${vinson_qt_translations_dir}"
            FILES "${vinson_qtbase_zh_cn_qm}")
    else()
        message(WARNING
            "Qt Simplified Chinese translations were not found; standard "
            "dialog buttons will use the platform fallback language.")
    endif()
else()
    message(WARNING
        "Qt LinguistTools was not found; bundled translations are disabled.")
endif()

add_executable(vinson-editor
    app/main.cpp
    app/Application.cpp
    app/Application.h
    app/SingleInstance.cpp
    app/SingleInstance.h
    "${PROJECT_SOURCE_DIR}/resources/resources.qrc"
)

configure_file("${PROJECT_SOURCE_DIR}/cmake/Version.h.in"
    "${PROJECT_BINARY_DIR}/generated/app/Version.h" @ONLY)

target_include_directories(vinson-editor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(vinson-editor
    PRIVATE vinson_editor_ui)

if(WIN32)
    configure_file(
        "${PROJECT_SOURCE_DIR}/resources/windows/vinson-editor.rc.in"
        "${PROJECT_BINARY_DIR}/generated/vinson-editor.rc"
        @ONLY)
    target_sources(vinson-editor PRIVATE
        "${PROJECT_BINARY_DIR}/generated/vinson-editor.rc")
endif()

set(VINSON_TRANSLATIONS_ENABLED "${VINSON_TRANSLATIONS_ENABLED}" PARENT_SCOPE)

foreach(target IN ITEMS vinson_core vinson_editor_ui vinson-editor)
    vinson_enable_warnings(${target})
endforeach()

# Scripts and portable packages expect the executable at the build root.
set_target_properties(vinson-editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

if(WIN32)
    set_target_properties(vinson-editor PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(UNIX AND NOT APPLE)
    file(RELATIVE_PATH vinson_libdir_from_bindir
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
    set_target_properties(vinson-editor PROPERTIES
        INSTALL_RPATH "$ORIGIN;$ORIGIN/${vinson_libdir_from_bindir}")
endif()
