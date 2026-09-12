## Overview

Here is an overview of the project's central functions and architectural features:

#### Core Editor Features
  * **Multi-file Management**: Open and edit multiple files in tabs (TabBar). Tabs visually indicate the file status with colors (e.g., red for modified, blue for read-only).
  * **Context Management**: Each tab and open file is managed in a context that stores, among other things, the cursor position and history. This allows for easy navigation (Back/Forward) in the code.
  * **Standard Editing**: Undo/Redo stack (undo.cpp), Search & Replace with regular expressions (search.cpp), code folding, as well as line- and column-wise selection (Pick/Put).
  * **Syntax Highlighting & Formatting**: Automated formatting and pretty-printing of the code
  * **Markdown**: switch between source and rendered view.

#### Language Server Protocol (LSP) Integration

Through the built-in LSclient, the editor can communicate with language servers (e.g., clangd for C/C++). This results in advanced IDE features:

  * **Code Navigation**: Go to Definition, Go to Type Definition, Go to Implementation.
  * **Annotations**: Get immediate compiler feedback with annotated source code lines.
  * **Auto-completion**: Request completions via LSP with a graphical popup.
  * **Refactoring**: Context-wide renaming of symbols

#### Integrated AI Agent

Arguably the most standout feature is the tight integration of an AI agent (agent.cpp, tools.cpp) that can actively intervene in the development process:

*   **Model Support**: Compatible with local models (via Ollama) as well as cloud models (Gemini, Anthropic, OpenAI).
*   **Tools (Tool-Calling)**: The agent has direct read access to the project and, in Build mode, write access as well. It can search the project for text and symbols (`search_project`, `find_symbol`), inspect file outlines and diagnostics (`get_file_outline`, `get_diagnostics`), find references (`find_references`) and format sources (`format`).
*   **Project Builds**: The agent can execute build commands in the project (`build_project`), for example, to trigger the CMake build.
*   **Shell Access**: The agent can run shell commands (`bash_command`) inside a sandbox.
*   **Screen Shots**: Take screenshots and add them to you prompt.
*   **Reasoning**: Support reasoning models.
*   **Sessions**: You can create and switch between persistent sessions.
*   **Web Research**: The agent can retrieve web documentation via MCP servers or the built-in fetching helper (`fetchWebDocumentation`).

### Git Integration
The editor has native Git support integrated (based on libgit2):

*   Fetching Git status, diffs, and logs.
*   Graphical Git panel and Git history view in the editor window.
*   The AI agent can perform Git operations through MCP servers (e.g. `mcp-server-git`).

### Architecture and Technology
*   **C++23 & CMake**: Modern C++ paired with a CMake build system.
*   **Qt6**: Used for the graphical user interface (Widgets, Splitter, Menus). Styling is also done via Qt Stylesheets (`src/light.qss`, `src/dark.qss`).
*   **Libraries**: Heavily uses nlohmann::json for parsing and generating LSP messages, LLM prompts/tool-calls, and for saving settings/sessions.

In summary, nped is an intelligent code editor that combines LSP for precise static code analysis with LLM-based AI agents to automate development tasks.
NPed is a source-code editor with special features for C and C++. For those who prefer working on the command line, Ped is a kind of mini-IDE.
