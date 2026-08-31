set(SCINTILLA_ROOT "${PROJECT_SOURCE_DIR}/third_party/scintilla")

if(NOT EXISTS "${SCINTILLA_ROOT}/version.txt")
    message(FATAL_ERROR
        "Scintilla sources are missing. Expected ${SCINTILLA_ROOT}/version.txt")
endif()

set(SCINTILLA_EDITOR_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/scintilla/Editor.cxx")
file(READ "${SCINTILLA_ROOT}/src/Editor.cxx" SCINTILLA_EDITOR_CONTENT)
set(SCINTILLA_STYLE_FORE_ORIGINAL
    "vs.styles[wParam].fore = ColourRGBA::FromIpRGB(lParam);")
set(SCINTILLA_STYLE_FORE_RGBA [=[
#if defined(VINSON_SCINTILLA_RGBA_STYLE_FORE)
		if ((static_cast<uintptr_t>(lParam) & (uintptr_t{1} << 32)) != 0) {
			vs.styles[wParam].fore = ColourRGBA(static_cast<int>(lParam));
		} else {
			vs.styles[wParam].fore = ColourRGBA::FromIpRGB(lParam);
		}
#else
		vs.styles[wParam].fore = ColourRGBA::FromIpRGB(lParam);
#endif]=])
set(SCINTILLA_STYLE_BACK_ORIGINAL
    "vs.styles[wParam].back = ColourRGBA::FromIpRGB(lParam);")
set(SCINTILLA_STYLE_BACK_RGBA [=[
#if defined(VINSON_SCINTILLA_RGBA_STYLE_BACK)
		if ((static_cast<uintptr_t>(lParam) & (uintptr_t{1} << 32)) != 0) {
			vs.styles[wParam].back = ColourRGBA(static_cast<int>(lParam));
		} else {
			vs.styles[wParam].back = ColourRGBA::FromIpRGB(lParam);
		}
#else
		vs.styles[wParam].back = ColourRGBA::FromIpRGB(lParam);
#endif]=])
string(FIND "${SCINTILLA_EDITOR_CONTENT}" "${SCINTILLA_STYLE_BACK_ORIGINAL}"
    SCINTILLA_STYLE_BACK_POSITION)
if(SCINTILLA_STYLE_BACK_POSITION EQUAL -1)
    message(FATAL_ERROR
        "The pinned Scintilla Editor.cxx no longer matches the RGBA adapter.")
endif()
string(FIND "${SCINTILLA_EDITOR_CONTENT}" "${SCINTILLA_STYLE_FORE_ORIGINAL}"
    SCINTILLA_STYLE_FORE_POSITION)
if(SCINTILLA_STYLE_FORE_POSITION EQUAL -1)
    message(FATAL_ERROR
        "The pinned Scintilla Editor.cxx no longer matches the foreground RGBA adapter.")
endif()
string(REPLACE "${SCINTILLA_STYLE_FORE_ORIGINAL}" "${SCINTILLA_STYLE_FORE_RGBA}"
    SCINTILLA_EDITOR_CONTENT "${SCINTILLA_EDITOR_CONTENT}")
string(REPLACE "${SCINTILLA_STYLE_BACK_ORIGINAL}" "${SCINTILLA_STYLE_BACK_RGBA}"
    SCINTILLA_EDITOR_CONTENT "${SCINTILLA_EDITOR_CONTENT}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/scintilla")
file(CONFIGURE OUTPUT "${SCINTILLA_EDITOR_SOURCE}"
    CONTENT "${SCINTILLA_EDITOR_CONTENT}" @ONLY)

add_library(scintilla_qt STATIC
    "${SCINTILLA_ROOT}/qt/ScintillaEdit/ScintillaDocument.cpp"
    "${SCINTILLA_ROOT}/qt/ScintillaEdit/ScintillaDocument.h"
    "${SCINTILLA_ROOT}/qt/ScintillaEdit/ScintillaEdit.cpp"
    "${SCINTILLA_ROOT}/qt/ScintillaEdit/ScintillaEdit.h"
    "${SCINTILLA_ROOT}/qt/ScintillaEditBase/PlatQt.cpp"
    "${SCINTILLA_ROOT}/qt/ScintillaEditBase/PlatQt.h"
    "${SCINTILLA_ROOT}/qt/ScintillaEditBase/ScintillaEditBase.cpp"
    "${SCINTILLA_ROOT}/qt/ScintillaEditBase/ScintillaEditBase.h"
    "${SCINTILLA_ROOT}/qt/ScintillaEditBase/ScintillaQt.cpp"
    "${SCINTILLA_ROOT}/qt/ScintillaEditBase/ScintillaQt.h"
    "${SCINTILLA_ROOT}/src/AutoComplete.cxx"
    "${SCINTILLA_ROOT}/src/CallTip.cxx"
    "${SCINTILLA_ROOT}/src/CaseConvert.cxx"
    "${SCINTILLA_ROOT}/src/CaseFolder.cxx"
    "${SCINTILLA_ROOT}/src/CellBuffer.cxx"
    "${SCINTILLA_ROOT}/src/ChangeHistory.cxx"
    "${SCINTILLA_ROOT}/src/CharacterCategoryMap.cxx"
    "${SCINTILLA_ROOT}/src/CharacterType.cxx"
    "${SCINTILLA_ROOT}/src/CharClassify.cxx"
    "${SCINTILLA_ROOT}/src/ContractionState.cxx"
    "${SCINTILLA_ROOT}/src/DBCS.cxx"
    "${SCINTILLA_ROOT}/src/Decoration.cxx"
    "${SCINTILLA_ROOT}/src/Document.cxx"
    "${SCINTILLA_ROOT}/src/EditModel.cxx"
    "${SCINTILLA_EDITOR_SOURCE}"
    "${SCINTILLA_ROOT}/src/EditView.cxx"
    "${SCINTILLA_ROOT}/src/Geometry.cxx"
    "${SCINTILLA_ROOT}/src/Indicator.cxx"
    "${SCINTILLA_ROOT}/src/KeyMap.cxx"
    "${SCINTILLA_ROOT}/src/LineMarker.cxx"
    "${SCINTILLA_ROOT}/src/MarginView.cxx"
    "${SCINTILLA_ROOT}/src/PerLine.cxx"
    "${SCINTILLA_ROOT}/src/PositionCache.cxx"
    "${SCINTILLA_ROOT}/src/RESearch.cxx"
    "${SCINTILLA_ROOT}/src/RunStyles.cxx"
    "${SCINTILLA_ROOT}/src/ScintillaBase.cxx"
    "${SCINTILLA_ROOT}/src/Selection.cxx"
    "${SCINTILLA_ROOT}/src/Style.cxx"
    "${SCINTILLA_ROOT}/src/UndoHistory.cxx"
    "${SCINTILLA_ROOT}/src/UniConversion.cxx"
    "${SCINTILLA_ROOT}/src/UniqueString.cxx"
    "${SCINTILLA_ROOT}/src/ViewStyle.cxx"
    "${SCINTILLA_ROOT}/src/XPM.cxx"
)

target_include_directories(scintilla_qt
    PUBLIC
        "${SCINTILLA_ROOT}/include"
        "${SCINTILLA_ROOT}/qt/ScintillaEdit"
        "${SCINTILLA_ROOT}/qt/ScintillaEditBase"
        # ScintillaEditBase's public header includes Platform.h and Debugging.h.
        # Upstream's qmake project exposes src for the same reason.
        "${SCINTILLA_ROOT}/src"
)

# Upstream's Qt project defaults to DLL import/export annotations on Windows.
# This project links Scintilla statically, so the API annotation must be empty
# for both the library and every consumer.
target_compile_definitions(scintilla_qt
    PUBLIC SCINTILLA_QT=1 EXPORT_IMPORT_API=
    PRIVATE
        VINSON_SCINTILLA_RGBA_STYLE_BACK=1
        VINSON_SCINTILLA_RGBA_STYLE_FORE=1)
set_target_properties(scintilla_qt PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_link_libraries(scintilla_qt
    PUBLIC Qt6::Core Qt6::Core5Compat Qt6::Gui Qt6::Widgets)

add_library(Scintilla::Scintilla ALIAS scintilla_qt)
