cmake_minimum_required(VERSION 3.25)

project(VinsonEditor
    VERSION 0.1.0
    DESCRIPTION "A lightweight native plain-text editor"
    LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Vinson Editor requires a 64-bit toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(VINSON_BUILD_TESTS "Build the automated test suite" ON)
option(VINSON_BUILD_BENCHMARKS "Build manual performance benchmarks" ON)

# Qt captures the GNUInstallDirs values when its Core package is loaded. A
# Windows portable archive must therefore select its flat runtime layout before
# find_package(Qt6), otherwise windeployqt places DLLs in bin/ while the
# application executable is installed at the archive root.
include(GNUInstallDirs)
if(WIN32)
    set(CMAKE_INSTALL_BINDIR ".")
    set(CMAKE_INSTALL_LIBDIR ".")
endif()

# The Linux archive uses a launcher-provided library path. Set this before Qt
# targets are created because Qt generates shared deployment support lazily.
if(UNIX AND NOT APPLE)
    set(QT_DEPLOY_FORCE_ADJUST_RPATHS OFF CACHE BOOL
        "Use the portable launcher instead of rewriting plugin RPATHs" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Core5Compat Gui Widgets)
find_package(Qt6 6.5 QUIET COMPONENTS LinguistTools)

include(cmake/Scintilla.cmake)

add_library(vinson_core STATIC
    src/editor/EditorDocument.cpp
    src/editor/EditorDocument.h
    src/file/EncodingDetector.cpp
    src/file/EncodingDetector.h
    src/file/FileLoader.cpp
    src/file/FileLoader.h
    src/file/FileManager.cpp
    src/file/FileManager.h
    src/file/FileSaver.cpp
    src/file/FileSaver.h
    src/file/FileTypes.cpp
    src/file/FileTypes.h
    src/largefile/LargeFilePolicy.cpp
    src/largefile/LargeFilePolicy.h
)
target_include_directories(vinson_core PUBLIC src)
target_link_libraries(vinson_core PUBLIC Qt6::Core)

add_library(vinson_editor_ui STATIC
    src/editor/EditorWidget.cpp
    src/editor/EditorWidget.h
    src/search/SearchController.cpp
    src/search/SearchController.h
    src/search/SearchTypes.h
    src/settings/Appearance.h
    src/settings/Localization.cpp
    src/settings/Localization.h
    src/settings/SettingsManager.cpp
    src/settings/SettingsManager.h
    src/settings/ThemeManager.cpp
    src/settings/ThemeManager.h
    src/ui/FindReplaceWidget.cpp
    src/ui/FindReplaceWidget.h
    src/ui/MenuAppearance.cpp
    src/ui/MenuAppearance.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/window/NativeWindowAppearance.cpp
    src/window/NativeWindowAppearance.h
    src/window/GlobalShortcut.cpp
    src/window/GlobalShortcut.h
    src/window/TrayController.cpp
    src/window/TrayController.h
    src/window/WindowController.cpp
    src/window/WindowController.h
    src/window/MainWindow.cpp
    src/window/MainWindow.h
)
target_include_directories(vinson_editor_ui PUBLIC src)
target_link_libraries(vinson_editor_ui
    PUBLIC vinson_core Qt6::Widgets Scintilla::Scintilla)

if(WIN32)
    target_link_libraries(vinson_editor_ui PRIVATE dwmapi)
endif()

set(VINSON_TRANSLATIONS_ENABLED OFF)
if(Qt6LinguistTools_FOUND)
    qt_add_translations(vinson_editor_ui
        SOURCE_TARGETS vinson_core vinson_editor_ui
        TS_FILES resources/i18n/vinson-editor_zh_CN.ts
        RESOURCE_PREFIX "/i18n"
        MERGE_QT_TRANSLATIONS
        QT_TRANSLATION_CATALOGS qtbase)
    set(VINSON_TRANSLATIONS_ENABLED ON)
else()
    message(WARNING
        "Qt LinguistTools was not found; bundled translations are disabled.")
endif()

add_executable(vinson-editor
    src/main.cpp
    src/app/Application.cpp
    src/app/Application.h
    resources/resources.qrc
)

configure_file(cmake/Version.h.in generated/app/Version.h @ONLY)

target_include_directories(vinson-editor PRIVATE
    src
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(vinson-editor
    PRIVATE vinson_editor_ui)

if(WIN32)
    configure_file(
        resources/windows/vinson-editor.rc.in
        generated/vinson-editor.rc
        @ONLY)
    target_sources(vinson-editor PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/vinson-editor.rc")
endif()

if(VINSON_BUILD_BENCHMARKS)
    add_executable(vinson-large-file-benchmark
        tools/LargeFileBenchmark.cpp
    )
    target_include_directories(vinson-large-file-benchmark PRIVATE src)
    target_link_libraries(vinson-large-file-benchmark
        PRIVATE vinson_editor_ui)
endif()

if(MSVC)
    target_compile_options(vinson_core PRIVATE /W4 /permissive-)
    target_compile_options(vinson_editor_ui PRIVATE /W4 /permissive-)
    target_compile_options(vinson-editor PRIVATE /W4 /permissive-)
    if(TARGET vinson-large-file-benchmark)
        target_compile_options(vinson-large-file-benchmark PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(vinson_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(vinson_editor_ui PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(vinson-editor PRIVATE -Wall -Wextra -Wpedantic)
    if(TARGET vinson-large-file-benchmark)
        target_compile_options(vinson-large-file-benchmark
            PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(WIN32)
    set_target_properties(vinson-editor PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(UNIX AND NOT APPLE)
    file(RELATIVE_PATH vinson_libdir_from_bindir
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
    set_target_properties(vinson-editor PROPERTIES
        INSTALL_RPATH "$ORIGIN;$ORIGIN/${vinson_libdir_from_bindir}")
endif()

install(TARGETS vinson-editor
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION ".")
install(FILES
    README.md
    README.zh-CN.md
    CHANGELOG.md
    CHANGELOG.zh-CN.md
    LICENSE
    DESTINATION ".")
install(DIRECTORY docs/
    DESTINATION docs
    FILES_MATCHING PATTERN "*.md")

# Qt uses windeployqt on Windows and CMake's runtime dependency scanner on
# Linux. This includes the required platform plugin while leaving the statically
# linked Scintilla implementation inside the executable.
if(WIN32)
    qt_generate_deploy_app_script(
        TARGET vinson-editor
        OUTPUT_SCRIPT vinson_deploy_script
        NO_TRANSLATIONS
        NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT "${vinson_deploy_script}")
elseif(UNIX AND NOT APPLE)
    # Some distribution plugins have no existing ELF RPATH slot for CMake to
    # rewrite. The installed launcher supplies the package lib directory for
    # both the executable and dynamically loaded plugins instead.
    configure_file(
        scripts/vinson-editor-linux.in
        generated/vinson-editor-launcher
        @ONLY)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/generated/vinson-editor-launcher"
        DESTINATION "."
        RENAME vinson-editor)

    # Qt 6.10 serializes multi-value forwarding arguments with semicolons in
    # qt_generate_deploy_app_script(). Generate the equivalent public deploy
    # call explicitly so glibc remains supplied by the target distribution.
    qt_generate_deploy_script(
        TARGET vinson-editor
        OUTPUT_SCRIPT vinson_deploy_script
        CONTENT "
qt_deploy_runtime_dependencies(
    EXECUTABLE \"$<TARGET_FILE:vinson-editor>\"
    GENERATE_QT_CONF
    NO_TRANSLATIONS
    INCLUDE_PLUGINS qoffscreen qminimal
    POST_EXCLUDE_REGEXES
        \".*/ld-linux[^/]*\\\\.so.*\"
        \".*/lib(c|m|dl|pthread|rt|resolv|nss_[^/]*)\\\\.so.*\"
)")
    install(SCRIPT "${vinson_deploy_script}")
endif()

include(CTest)
if(BUILD_TESTING AND VINSON_BUILD_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

set(CPACK_PACKAGE_NAME "VinsonEditor")
set(CPACK_PACKAGE_VENDOR "Vinson Editor contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "VinsonEditor-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_MONOLITHIC_INSTALL ON)
if(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
