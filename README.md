# NPed

NPed (Program Editor) is a modern C++23 and Qt6-based text editor and lightweight IDE.

Here is an overview of the project's central functions and architectural features:

#### Core Editor Features
  * **Multi-file Management**: Open and edit multiple files in tabs (TabBar). Tabs visually indicate the file status with colors (e.g., red for modified, blue for read-only).
  * **Context Management**: Each tab and open file is managed in a context that stores, among other things, the cursor position and history. This allows for easy navigation (Back/Forward) in the code.
  * **Standard Editing**: Undo/Redo stack (undo.cpp), Search & Replace with regular expressions (search.cpp), code folding, as well as line- and column-wise selection (Pick/Put).
  * **Syntax Highlighting & Formatting**: Automated formatting and pretty-printing of the code (pretty.cpp).

#### Language Server Protocol (LSP) Integration

Through the built-in LSclient, the editor can communicate with language servers (e.g., clangd for C/C++). This results in advanced IDE features:

  * **Code Navigation**: Go to Definition, Go to Type Definition, Go to Implementation.
  * **Auto-completion**: Request completions via LSP with a graphical popup (completion.cpp).
  * **Hover Information**: Displaying type or documentation hints when hovering with the cursor.
  * **Refactoring**: Context-wide renaming of symbols (Rename).

#### Integrated AI Agent (AgentUI)

Arguably the most standout feature is the tight integration of an AI agent (agent.cpp, agentui.cpp) that can actively intervene in the development process:

*   **Model Support**: Compatible with local models (via Ollama) as well as cloud models (Gemini, Anthropic).
*   **Tools (Tool-Calling)**: The agent has direct read and write access to the project. It can read, modify, and create files (handleReadFile, handleModifyFile), list directories, or search the project for text and symbols (handleSearchProject, handleFindSymbol).
*   **Project Builds**: The agent can execute build commands in the project (runBuildCommand), for example, to trigger CMake or Make.
*   **Web Research**: The agent can specifically retrieve web documentation (fetchWebDocumentation).

### Git Integration
The editor has native Git support integrated (based on libgit2 in git.cpp):

*   Fetching Git status, diffs, and logs.
*   Graphical Git panel and Git history view in the editor window.
*   The AI agent itself can automatically create commits (createGitCommit) once it has completed tasks.

### Architecture and Technology
*   **C++23 & CMake**: Modern C++ paired with a CMake build system.
*   **Qt6**: Used for the graphical user interface (Widgets, Splitter, Menus). Styling is also done via Qt Stylesheets (style.qss).
*   **Libraries**: Heavily uses nlohmann::json for parsing and generating LSP messages, LLM prompts/tool-calls, and for saving settings/sessions.

In summary, nped is an intelligent code editor that combines LSP for precise static code analysis with LLM-based AI agents to automate development tasks.
NPed is a source-code editor with special features for C and C++. For those who prefer working on the command line, Ped is a kind of mini-IDE.

### Operating Concept

The operating concept of **nped** is heavily focused on efficiency for prolific writers and developers who prefer to code without using a mouse. It is designed as a kind of "mini-IDE for the command line, but with a GUI".

Here are the key aspects of the operating concept:

### 1. Keyboard-Centric ("Blind Editing")
*   **No reliance on the mouse:** Nearly all functions are invoked via keyboard shortcuts. You shouldn't have to take your hands off the keyboard.
*   **Touch typing recommended:** The concept is not optimal for "hunt-and-peck typing," as commands are meant to be executed quickly and without looking.
*   **No cluttered menus:** The available commands are not prominently visible as buttons in the program. Learning the shortcuts via a reference card or the manual is necessary.

### 2. The Command System (Command Entry)
Functions that require parameters (like opening a file or search/replace) follow a specific workflow:

1.  **Press `<Escape>`**: This opens an input field at the bottom of the screen (in the status bar).
2.  **Enter text**: You type the argument (e.g., the search term or filename).
3.  **Press the command key**: Instead of pressing Enter, you conclude the input with the shortcut for the desired function. For example, if you want to search for the entered text, you press `<F7>`.

### 3. Cursor Control (WordStar/Turbo Pascal Style)
The cursor movements are based on a classic block concept around the `S, E, D, X` keys (the "cursor block"), using `Ctrl` as a modifier and `Ctrl+Q` as an enhancer (prefix). This layout is heavily inspired by editors like *WordStar* or the old *Turbo Pascal / Borland* editor.

*   **Single steps:**
    *   `Ctrl+S`: Character left
    *   `Ctrl+D`: Character right
    *   `Ctrl+E`: Line up
    *   `Ctrl+X`: Line down
*   **Word/Page-wise:**
    *   `Ctrl+A`: Word left
    *   `Ctrl+F`: Word right
    *   `Ctrl+R`: Page up
    *   `Ctrl+C`: Page down

*   **Jumps (with `Ctrl+Q` as prefix):**
    *   `Ctrl+Q, Ctrl+S`: Start of line
    *   `Ctrl+Q, Ctrl+D`: End of line
    *   `Ctrl+Q, Ctrl+E`: Top of page
    *   `Ctrl+Q, Ctrl+X`: Bottom of page
    *   `Ctrl+Q, Ctrl+R`: Start of text
    *   `Ctrl+Q, Ctrl+C`: End of text

### 4. Code-Specific Formatting Rules (Auto-Clean)
The editor automatically enforces certain formatting rules to ensure clean code:

*   Trailing spaces at the end of a line are automatically removed.
*   Blank lines at the end of a file are deleted.
*   The last line is **always** terminated with a line break (`\n`).
*   Completely empty files cannot be saved; they are ignored during the save process.

### 5. IDE Features via Shortcuts
Since nped functions as a mini-IDE, LSP features are also mapped to keys (often in conjunction with the Command Entry).
For example, formatting the current file is done via the combination `Ctrl+O, Ctrl+F`.

In summary, nped is aimed at developers looking for a purist, highly keyboard-driven environment that combines a classic editor workflow with modern C++ Language Server features.

[(this is a translation from manual.md (partially) with the help from Gemini 2.5 Pro)]
