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
install(FILES THIRD_PARTY_NOTICES.md
    DESTINATION "." COMPONENT third-party-licenses)
install(DIRECTORY licenses/
    DESTINATION licenses COMPONENT third-party-licenses)

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
        cmake/vinson-editor-linux.in
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
    # A stable installation identity lets later versions find the previous
    # installation instead of creating a second Apps & features entry.
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Vinson Editor")
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "VinsonEditor")
    set(CPACK_PACKAGE_EXECUTABLES "vinson-editor" "Vinson Editor")
    set(CPACK_NSIS_DISPLAY_NAME "Vinson Editor")
    set(CPACK_NSIS_PACKAGE_NAME "Vinson Editor")
    set(CPACK_NSIS_MUI_ICON "${VINSON_WINDOWS_ICON}")
    set(CPACK_NSIS_MUI_UNIICON "${VINSON_WINDOWS_ICON}")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "vinson-editor.exe")
    # Reuse the registered installation directory and update files in place.
    # This keeps upgrades silent about uninstalling the previous version while
    # still falling back to Program Files for a first installation.
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL OFF)
    file(TO_NATIVE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/VinsonUpgrade.nsh"
        VINSON_NSIS_UPGRADE_INCLUDE)
    string(REPLACE "\\" "\\\\" VINSON_NSIS_UPGRADE_INCLUDE
        "${VINSON_NSIS_UPGRADE_INCLUDE}")
    set(CPACK_NSIS_DEFINES
        "InstallDirRegKey HKLM 'Software\\\\${CPACK_PACKAGE_VENDOR}\\\\${CPACK_PACKAGE_INSTALL_REGISTRY_KEY}' ''\n!include \\\"${VINSON_NSIS_UPGRADE_INCLUDE}\\\"")
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
        "!insertmacro VinsonCheckUpgrade")
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
    set(CPACK_NSIS_MENU_LINKS
        "THIRD_PARTY_NOTICES.md" "Third-party licenses")
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
