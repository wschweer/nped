# NPEd Manual

[TOC]

## Overview

NPed (Program Editor) is a modern C++23 and Qt6-based text editor or a lightweight IDE.

Here is an overview of the project's central functions and architectural features:

#### Core Editor Functions
  * Multifile Management: Open and edit multiple files in tabs (TabBar). Tabs visualize the file status with colors (e.g., red for modified, blue for read-only).
  * Context Management: Each tab and open file is managed within a context that saves, among other things, the cursor position and history. This enables easy navigation (forward/backward) in the code.
  * Standard Editing: Undo/Redo stack (undo.cpp), search & replace with regular expressions (search.cpp), code folding, as well as line- and column-wise selection (Pick/Put).
  * Syntax Highlighting & Formatting: Automated code formatting and pretty-printing (pretty.cpp).

### Language Server Protocol (LSP) Integration

Via the built-in LSclient, the editor can communicate with language servers (e.g., clangd for C/C++). This results in advanced IDE features:

  * Code Navigation: Go to Definition, Go to Type Definition, Go to Implementation.
  * Autocompletion: Request completions via LSP with a graphical popup (completion.cpp).
  * Hover Information: Display type or documentation hints when lingering with the cursor.
  * Refactoring: Context-wide renaming of symbols (Rename).

### Integrated AI Agent

Arguably the most striking feature is the tight integration of an AI agent that can actively intervene in development:

  * Model Support: Compatible with local models (via Ollama) as well as cloud models (Gemini, Anthropic).
  * Tools (Tool-Calling): The agent has direct read and write access to the project. It can read, modify, and create files (handleReadFile, handleModifyFile), list directories, or search the project for texts and symbols (handleSearchProject, handleFindSymbol).
  * Project Builds: The agent can execute build commands in the project (runBuildCommand) to trigger CMake or Make, for example.
  * Web Research: The agent can retrieve web documentation (fetchWebDocumentation).

### Git Integration
The editor has native Git support integrated (based on libgit2 in git.cpp):

  * Retrieving Git status, diffs, and logs.
  * Graphical Git panel and Git history view in the editor window.
  * The AI agent itself can automatically create commits (createGitCommit) once it has completed tasks.

### Architecture and Technology
  * C++23 & CMake: Modern C++ paired with a CMake build system.
  * Qt6: Used for the graphical user interface (widgets, splitters, menus). Styling is also done via Qt Stylesheets (style.qss).
  * Libraries: Heavily uses nlohmann::json for parsing and creating LSP messages, LLM prompts/tool calls, and saving settings/sessions.

In summary, nped is an intelligent code editor that combines LSP for precise static code analysis with LLM-based AI agents to automate development tasks.
NPed is a source code editor with special features for C and C++.
For those who prefer working on the command line, Ped is a kind of mini-IDE.

### Operation Concept

The operation concept of **nped** is strongly oriented towards efficiency for prolific writers and developers who prefer to program without using a mouse. It is designed as a kind of "mini-IDE for the command line, but with a GUI".

Here are the essential aspects of the operation concept:

#### Keyboard-centric ("Blind Editing")
* **No mouse dependency:** Almost all functions are called via keyboard shortcuts. You shouldn't have to take your hands off the keyboard.
* **Ten-finger system recommended:** The concept is not optimal for the "two-finger hunt-and-peck system," as the commands are meant to be executed blindly and quickly.
* **No cluttered menus:** The available commands are not prominently visible as buttons in the program. Learning the shortcuts via a reference card or the manual is necessary.

#### The Command System (Command Entry)
Functions that require parameters (such as opening a file or search/replace) follow a special workflow:

1. **Press `<Escape>`:** This opens an input field at the bottom of the screen (in the status bar).
2. **Enter text:** You type the argument (e.g., the search term or file name).
3. **Press command key:** Instead of pressing Enter, you finish the input with the shortcut for the desired function. If you want to search for the entered text, for example, press `<F7>`.

#### Cursor Control (WordStar/Turbo-Pascal style)
Cursor movements are based on a classic block concept around the keys `S, E, D, X`
(the "cursor block"), using `Ctrl` as a modifier and `Ctrl+Q` as an enhancer (prefix).
This layout is strongly based on editors like *WordStar* or the old *Turbo Pascal / Borland* editor.

* **Single steps:**
  * <kbd>Ctrl+S</kbd>: Char left
  * `Ctrl+D`: Char right
  * `Ctrl+E`: Line up
  * `Ctrl+X`: Line down
* **Word/Page based:**
  * `Ctrl+A`: Word left
  * `Ctrl+F`: Word right
  * `Ctrl+R`: Page up
  * `Ctrl+C`: Page down

* **Jumps (with `Ctrl+Q` as prefix):**
  * `Ctrl+Q, Ctrl+S`: Line start
  * `Ctrl+Q, Ctrl+D`: Line end
  * `Ctrl+Q, Ctrl+E`: Page start
  * `Ctrl+Q, Ctrl+X`: Page end
  * `Ctrl+Q, Ctrl+R`: Text start
  * `Ctrl+Q, Ctrl+C`: Text end

#### Code-specific formatting rules (Auto-Clean)
The editor automatically enforces certain formatting to ensure clean code:

* Trailing spaces at the end of a line are automatically removed.
* Empty lines at the end of a file are deleted.
* The last line is **always** terminated with a line break (`\n`).
* Completely empty files cannot be saved; they are ignored during saving.

#### IDE Features via Shortcuts
Since nped functions as a mini-IDE, LSP features are also mapped to keys (often in conjunction with the Command Entry).
For example, formatting the current file is done via the combination `Ctrl+O, Ctrl+F`.

In summary, nped is aimed at developers looking for a purist, highly keyboard-driven environment that combines classic editor flow with modern C++ Language Server functions.

## IDE - Features

A "project" is a directory containing a CMakeLists.txt file. The editor can also be started in a project subdirectory.

For information about the entire project, Ped contacts a language server. For C++, the clangd language server is started.
The LS parses the current source code and requires the same environment as the later compiler run so that it, for example, finds all header files. To do this, one must instruct CMake to generate a `compile_commands.json` file that contains exactly this information.

CMake is called for this as follows:

    cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja

CMake then writes the "compile_commands.json" to the build directory "build".
If Ped is started within the project directory or a project subdirectory, the clangd LS automatically finds everything else.

### **Global Commands**

      [F1]                Saves modified files and exits the editor.
      [Shift+F1]          Saves
      [Ctrl+K, Ctrl+Q]    Exits the editor **without** saving modified files.
      [Escape]            "Enter" command: opens input window on the status bar to enter parameters
      [Enter <name> F3]   opens file `name` in a new context window


### **Cursor Commands**

      [Ctrl+D or Cursor-Right]  CMD_CHAR_RIGHT
      [Ctrl+S or Cursor-Left]   CMD_CHAR_LEFT
      [Up;    Ctrl+E]           CMD_LINE_UP
      [Down;  Ctrl+X]           CMD_LINE_DOWN
      [Ctrl+Q,  Ctrl+S]         CMD_LINE_START
      [Ctrl+Q,  Ctrl+D]         CMD_LINE_END
      [Ctrl+Q,  Ctrl+E]         CMD_LINE_TOP
      [Ctrl+Q,  Ctrl+X]         CMD_LINE_BOTTOM
      [Ctrl+R;  PageUp]         CMD_PAGE_UP
      [Ctrl+C;  PageDown]       CMD_PAGE_DOWN
      [Ctrl+Q,  Ctrl+R]         CMD_FILE_BEGIN
      [Ctrl+Q,  Ctrl+C]         CMD_FILE_END
      [Ctrl+A]                  CMD_WORD_LEFT
      [Ctrl+F]                  CMD_WORD_RIGHT
      [Enter <nn> Ctrl+G]       go to line nn

### **Modify Text**

      [Ctrl+Z]                     CMD_UNDO
      [Shift+Ctrl+Z]               CMD_REDO
      [Delete; Backspace]          CMD_RUBOUT
      [Ctrl+G]                     CMD_CHAR_DELETE
      [Ctrl+N]                     CMD_INSERT_LINE
      [F8]                         CMD_PICK
      [F9]                         CMD_PUT
      [Ctrl+Y]                     CMD_DELETE_LINE
      [Ctrl+Q,  Ctrl+Y]            CMD_DELETE_LINE_RIGHT
      [Ctrl+T]                     CMD_DELETE_WORD


### **File Commands**

      [Ctrl+K,  Ctrl+S]            CMD_SAVE
      [Ctrl+K,  Ctrl+K; F4]        CMD_KONTEXT_COPY
      [Ctrl+K,  Ctrl+J; Shift+F3]  CMD_KONTEXT_PREV
      [Ctrl+K,  Ctrl+L; F3]        CMD_KONTEXT_NEXT
      [Ctrl+K,  Ctrl+I]            CMD_SHOW_BRACKETS
      [Ctrl+Up]                    CMD_KONTEXT_UP
      [Ctrl+Down]                  CMD_KONTEXT_DOWN
      [F5]                         CMD_SELECT_ROW
      [F6]                         CMD_SELECT_COL
      [Ctrl+O,  Ctrl+W]            CMD_ENTER_WORD


### **Search/Replace**

      [Enter <text> F7]               search for `text`
      [Enter <search>/<replace> F7]   search `search` and replace with `replace`
      [F7]                            search forward
      [SHIFT+F7]                      search backward
      [Ctrl+O, Ctrl+R]                project-wide rename via the language server

### **Copy/Cut Paste**
#### Selection Modes

      [F5]              CMD_SELECT_ROW    selects rows
      [F6]              CMD_SELECT_COL    selects columns
      [Ctrl + F5]       CMD_SELECT_CHAR   selects all characters between a start and end marker

#### Selection Functions

      [F8]              CMD_PICK          Copies the selection (Copy) or the current line
                                          if the selection is empty (Pick) into the Copy Buffer.
      [Ctrl + Y]        CMD_DELETE_LINE   Copies the selection or the current line
                                          if the selection is empty into the Copy Buffer and then deletes it.
      [F9]              CMD_PUT           Inserts the Copy Buffer (Paste/Put) at the current
                                          cursor position.
      [MMT]             Paste             Copies the content of the system clipboard to the
                                          cursor position.

### **IDE Commands**

      [Ctrl+K,  Ctrl+M]             CMD_SHOW_LEVEL

#### **``Ctrl+O,  Ctrl+F``** Formats a file
The format command is sent to the language server. An LS must be configured and available for the current file type and it must support the format command. For C/C++, this works great with the clangd LanguageServer.

#### **``Ctrl+V``** Switches to another view of the current file.
For Markdown and HTML files, it switches from the editable text representation to a rendered 'read-only' representation.
For C/C++ files, a list of function or method names is shown. You can position the cursor on a function and switch back to the text representation to navigate to this function in a flash.

      [Ctrl+B]                      CMD_VIEW_BUGS
      [F10]                         CMD_GOTO_TYPE_DEFINITION
      [F11]                         CMD_GOTO_IMPLEMENTATION
      [F12]                         CMD_GOTO_DEFINITION
      [Ctrl+O, Ctrl+E]              CMD_EXPAND_MACROS
      [Ctrl+Tab; Shift+Tab]         CMD_COMPLETIONS
      [Ctrl+M]                      CMD_FOLD_ALL
      [Ctrl+Shift+M]                CMD_UNFOLD_ALL
      [Ctrl+<]                      CMD_FOLD_TOGGLE
      [Ctrl+O, Ctrl+H]              CMD_FUNCTION_HEADER
      [Ctrl+O, Ctrl+G]              CMD_GIT_TOGGLE
      [Ctrl+F12]                    CMD_GOTO_BACK
      [Ctrl+H]                      CMD_SHOW_INFO
      [Enter <name> Ctrl+F]         Creates empty C++ function named `name`


## Code Expander

The Code Expander is an experimental feature in NPed to generate C++ code via Python script.
The Python scripts are disguised as C comments and are executed when writing the source file.
Python output is then inserted into the file after the script.

This means:

* No separate preprocessor is necessary; source code is changed "in place"
* No adaptation for the build system
* Easy to debug, as both the Python scripts and their output are directly visible
* The expanded files are normal C++ code and the project can be edited without these Ped features as well.

Disadvantages:

* If the generated code parts are changed outside the Ped environment, the scripts should also be adapted. Otherwise, changes will be lost when code is re-generated by the script.

Normally, Ped also shows the generated part, which is useful for debugging purposes.
For better clarity, the generated text can also simply be skipped when reading the file. A comment in the first line of the file with the text "//##H" turns on this behavior.

## AI Agent
### Model

LLM models must be configured and then appear in the AI Panel pulldown menu "Models".
If an Ollama server is running on the computer, the available Ollama models are determined automatically and added to the pulldown menu.

Models from Google (Gemini), Anthropic (Claude), and Ollama are supported.
Ollama is particularly interesting as local models can be used through it.
Ollama, for example, is hard-wired via the URL http://localhost:11434.

Reasoning is supported for suitable models.

### Session

Sessions are automatically saved to disk. New sessions can be created and existing sessions deleted. You can switch between sessions via the Session pulldown menu in the AI Panel.
A session is always connected to an AI model. When switching a model, the corresponding model is always selected. If the model is changed, the current session is saved and a new session for this model is created.

### Build/Plan Mode

In "Plan" mode, the AI model only has read access to the system. In "Build" mode, the AI model can also create/delete and modify files in and below the project directory.

If NPed was not started in a project, the "Build" mode cannot be switched on.

## Installation
### Font
      There are various fonts that are particularly suitable for programming:

- Fira Code
The concept behind Fira Code is simple: the monospaced font was developed to combine frequently used character sequences into a single one, thus reducing the time you need to skim your code and find what you're looking for.

For example, the equals sign (!=) becomes an equals sign with a slash, the opening and closing symbols in HTML (</) are closer together, and so on. These ligatures exist for many programming languages.

The underlying characters themselves are not changed, so it has no effect on your code. It just makes it easier to read! There are also some character variants so that you can customize the font to your liking.

Fira Code is supported by most browsers and you can take a look at the code examples to see how it looks in practice.

- Proggy fonts
- DejaVu Sans Mono
- Source code Pro
- Dina
- Terminus
- Input
- Hack
- Cascadia Code
- JetBrains Mono
- Anonymous Pro
- Ubuntu Mono
- Consolas

Nped can only use fonts that are non-proportional (Monospace).

### Language Server

For the "C" language family: c, c++

      sudo apt install clangd

### Core Dumps

A normal Kubuntu Linux does not generate core dumps. Core dumps are essential for testing and debugging programs.

To enable core dumps on Ubuntu/Kubuntu you must:

- Turn off Apport

      sudo nano /etc/default/apport

change "enabled=1" to "enabled=0"

then:

      sudo systemctl stop apport.service
      sudo systemctl disable apport.service

## Examples
[Examples](manual/examples.md)
