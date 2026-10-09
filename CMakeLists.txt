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
    Core Core5Compat Gui Widgets Qml)
find_package(Qt6 6.5 QUIET COMPONENTS LinguistTools)

include(cmake/Scintilla.cmake)

function(vinson_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(WIN32)
    set(VINSON_WINDOWS_ICON
        "${PROJECT_SOURCE_DIR}/resources/icons/vinson-editor.ico")
endif()

add_subdirectory(src)
if(VINSON_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

include(CTest)
if(BUILD_TESTING AND VINSON_BUILD_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

include(cmake/Packaging.cmake)
