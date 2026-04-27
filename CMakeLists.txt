cmake_minimum_required(VERSION 3.16)
Set(QT_FORCE_CMP0156_TO_VALUE NEW)
project (nped VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD    23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(QT_QML_GENERATE_QMLLS_INI ON)

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
      WebChannel
      DBus
      PrintSupport
      )

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

# find_library(MD4C_LIB md4c)
# find_library(MD4C_HTML_LIB md4c-html)


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
      src/file.cpp
      src/editwin.cpp
      src/kontext.cpp
      src/undo.cpp
      src/ide.cpp
      src/lsclient.cpp
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
      src/anthropic.cpp
      src/openai.cpp
      src/chatdisplay.cpp
      src/session.cpp
      src/screenshot.cpp
      src/dashboard.cpp
      src/dashboard.h
      src/textstyle.cpp
      src/textstyle.h
      src/filetype.cpp
      src/filetype.h
      src/ls.cpp
      src/ls.h
      src/model.cpp
      src/configwebview.cpp
      src/configwebview.h
      src/editor.cpp
      src/editor.h
      src/mcp.cpp
      src/mcp.h
      )


qt_add_resources(nped "res"
      FILES
            res/diff.html
            res/config.json
            res/css/bootstrap.min.css
            res/diff2html.min.css
            res/js/diff2html.min.js
            res/js/diff2html-ui.min.js
      )

qt_add_resources(nped "images"
      PREFIX "/"
      FILES
      images/configure.svg
      images/configure_white.svg
      images/trash.svg
      images/trash_white.svg
      images/trash_sketch.svg
      images/trash_sketch_white.svg
      images/thinking.svg
      images/thinking-dark.svg
      images/camera.svg
      images/camera-dark.svg
      images/tool.svg
      images/Git-Icon-1788C.svg
      images/Git-Icon-1788C-white.svg
      images/tool-dark.svg
      images/stop.svg
      images/stop_white.svg
      images/back.svg
      images/back_white.svg
      images/forward.svg
      images/forward_white.svg
      images/reload.svg
      images/reload_white.svg
      images/home.svg
      images/home_white.svg
      )

qt_add_resources(nped style
      PREFIX "/"
      FILES
      src/light.qss
      src/dark.qss
      )

target_link_libraries(nped PRIVATE
      Qt::Core
      Qt::Gui
      Qt::Widgets
      Qt::Network
      Qt::WebEngineCore
      Qt::WebEngineWidgets
      Qt::WebChannel
      Qt::DBus
      Qt::PrintSupport
      pthread
      git2
      md4c-html
      md4c
      )
