cmake_minimum_required(VERSION 3.16)
Set(QT_FORCE_CMP0156_TO_VALUE NEW)
project (nped VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD    23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_VERBOSE_MAKEFILE OFF)

set (NPED_VERSION_MAJOR "0")
set (NPED_VERSION_MINOR "0")
set (NPED_VERSION_PATCH "1")
set (PROJECT_VERSION "${NPED_VERSION_MAJOR}.${NPED_VERSION_MINOR}.${NPED_VERSION_PATCH}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_SRC_DIR ON)

set(CMAKE_BUILD_TYPE Debug)

find_package(Qt6 REQUIRED COMPONENTS
      Core
      Gui
      Widgets
      Network
      WebEngineCore
      WebEngineWidgets
      Qml
      Quick
      QuickControls2
      QuickWidgets
      )

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

find_library(MD4C_LIB md4c)
find_library(MD4C_HTML_LIB md4c-html)


include_directories(
      ${PROJECT_SOURCE_DIR}
      ${PROJECT_SOURCE_DIR}/src
      ${PROJECT_BINARY_DIR}
      )
link_directories("/usr/local/lib" "/usr/lib")

add_definitions(-DNPED_DIR="${PROJECT_BINARY_DIR}" -DNPED_BASE="${PROJECT_SOURCE_DIR}"
   -DNPED_VERSION="${PROJECT_VERSION}" -DPROCESSOR="${CMAKE_SYSTEM_PROCESSOR}"
   ${debugdefines}
   ${LLVM_DEFINITIONS}
   )

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall -Wextra")

set (CMAKE_AUTOMOC ON)

qt_add_executable(nped
      ${mocs}
      src/logger.cpp
      src/main.cpp
      src/line.cpp
      src/editor.cpp
      src/file.cpp
      src/filewatcher.cpp
      src/editwin.cpp
      src/kontext.cpp
#      src/enter.cpp
      src/undo.cpp
      src/ide.cpp
      src/lsclient.cpp
      src/ast.cpp
      src/completer.cpp
      src/completion.cpp
      src/search.cpp
      src/git.cpp
      src/pretty.cpp
      src/agent.cpp
      src/tools.cpp
      src/webview.cpp
      src/config.cpp
      src/llm.cpp
      src/ollama.cpp
      src/gemini.cpp
#      src/anthropic.cpp
#      src/openai.cpp
      src/chatdisplay.cpp
      )

qt_add_resources(nped "qmls"
      FILES qtquickcontrols2.conf
      )
qt_add_resources(nped "mages"
      FILES images/Git-Icon-1788C.svg
      FILES images/configure.svg
      )

qt_add_qml_module(nped
    URI "Nped.Config"
    VERSION 1.0
    SOURCES

    QML_FILES
        qml/ConfigDialog.qml
        qml/SettingsCategory.qml
        qml/InputRow.qml
        qml/GuiPanel.qml
      )

qt_add_resources(nped files
      FILES
      src/dark.qss
      src/light.qss
      )

target_link_libraries(nped PRIVATE
      Qt::Core
      Qt::Gui
      Qt::Widgets
      Qt::Network
      Qt::WebEngineCore
      Qt::WebEngineWidgets
      Qt::Qml
      Qt::Quick
      Qt::QuickControls2
      Qt::QuickWidgets
      pthread
      git2
      md4c-html
      md4c
      )
