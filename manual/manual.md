# NPEd Manual

[TOC]

## Overview

NPed (Program Editor) is a modern C++23 and Qt6-based text editor or a lightweight IDE.

Here is an overview of the core features and architectural characteristics of the project:

#### Core Editor Features
  * Multi-file management: Open and edit multiple files in tabs (TabBar). Tabs visually indicate file status (e.g., red for modified, blue for read-only).
  * Context management: Each tab and open file is managed within a context that stores details such as cursor position and history. This enables easy navigation (forward/backward) through the code.
  * Standard editing: Undo/Redo stack (undo.cpp), search & replace with regular expressions (search.cpp), code folding, and line- and column-wise selection (Pick/Put).
  * Syntax Highlighting & Formatting: Automated formatting and pretty-printing of code (pretty.cpp).

### Language Server Protocol (LSP) Integration

Using the built-in LSclient, the editor can communicate with language servers (e.g., clangd for C/C++). This results in advanced IDE features:

  * Code Navigation: Go to Definition, Go to Type Definition, Go to Implementation.
  * Autocomplete: Request completions via LSP with a graphical popup (completion.cpp).
  * Hover Information: Displaying type or documentation hints when hovering with the cursor.
  * Refactoring: Context-wide renaming of symbols (Rename).

### Integrated AI Agent

Arguably the most prominent feature is the deep integration of an AI agent that can actively intervene in development:

  * Model support: Compatible with local models (via Ollama) as well as cloud models (Gemini, Anthropic).
  * Tool-Calling: The agent has direct read and write access to the project. It can read, modify, and create files (handleReadFile, handleModifyFile), list directories, or search the project for text and symbols (handleSearchProject, handleFindSymbol).
  * Project builds: The agent can execute build commands in the project (runBuildCommand) to trigger CMake or Make, for example.
  * Web research: The agent can specifically fetch web documentation (fetchWebDocumentation).

### Git Integration
The editor has native Git support integrated (based on libgit2 in git.cpp):

  * Retrieving Git status, diffs, and logs.
  * Graphical Git panel and Git history view in the editor window.
  * The AI agent itself can automatically create commits (createGitCommit) once it has completed tasks.

### Architecture and Technology
  * C++23 & CMake: Modern C++ paired with a CMake build system.
  * Qt6: Used for the graphical user interface (widgets, splitters, menus). Styling is also done via Qt stylesheets (style.qss).
  * Libraries: Heavily uses nlohmann::json for parsing and generating LSP messages, LLM prompts/tool calls, and saving settings/sessions.

In summary, nped is an intelligent code editor that combines LSP for precise static code analysis with LLM-based AI agents to automate development tasks.
NPed is a source code editor with special features for C and C++.
For those who prefer working on the command line, Ped is a kind of mini-IDE.

### Operational Concept

The operating concept of **nped** is strongly oriented towards efficiency for power users and developers who prefer programming without using a mouse. It is conceived as a type of "mini-IDE for the command line, but with a GUI".

Here are the essential aspects of the operational concept:

#### Keyboard-centric ("Blind Editing")
* **No mouse requirement:** Almost all functions are called via keyboard shortcuts. You shouldn't have to take your hands off the keyboard.
* **Touch-typing recommended:** The concept is not optimal for the "hunt-and-peck" system, as commands are intended to be executed blindly and quickly.
* **No cluttered menus:** The available commands are not prominently visible as buttons in the program. Learning the shortcuts via a reference card or the manual is necessary.

#### The Command System (Command Entry)
Functions that require parameters (such as opening a file or search/replace) follow a special workflow:

1. **Press `<Escape>`**: This opens an input field at the bottom of the screen (in the status line).
2. **Enter text**: Type the argument (e.g., the search term or filename).
3. **Press command key**: Instead of pressing Enter, finish the entry with the shortcut for the desired function. For example, if you want to search for the entered text, press `<F7>`.

#### Cursor Control (WordStar/Turbo-Pascal Style)
Cursor movements are oriented toward a classic block concept around the `S, E, D, X` keys (the "cursor block"), using `Ctrl` as a modifier and `Ctrl+Q` as an enhancer (prefix).
This layout is strongly modeled after editors like *WordStar* or the old *Turbo Pascal / Borland* editor.

* **Single steps:**
  * `<Ctrl+S>`: Character left
  * `Ctrl+D`: Character right
  * `Ctrl+E`: Line up
  * `Ctrl+X`: Line down
* **Word/Page based:**
  * `Ctrl+A`: Word left
  * `Ctrl+F`: Word right
  * `Ctrl+R`: Page up
  * `Ctrl+C`: Page down

* **Jumps (with `Ctrl+Q` as prefix):**
  * `Ctrl+Q, Ctrl+S`: Start of line
  * `Ctrl+Q, Ctrl+D`: End of line
  * `Ctrl+Q, Ctrl+E`: Start of page
  * `Ctrl+Q, Ctrl+X`: End of page
  * `Ctrl+Q, Ctrl+R`: Start of text
  * `Ctrl+Q, Ctrl+C`: End of text

#### Code-specific formatting rules (Auto-Clean)
The editor automatically enforces certain formatting to ensure clean code:

* Trailing spaces at the end of a line are automatically removed.
* Empty lines at the end of a file are deleted.
* The last line is **always** terminated with a newline (`\n`).
* Completely empty files cannot be saved; they are ignored upon saving.

#### IDE Features via Shortcuts
Since nped acts as a mini-IDE, LSP functions are also mapped to keys (often in conjunction with the Command Entry).
For example, formatting the current file is done via the combination `Ctrl+O, Ctrl+F`.

In summary, nped is aimed at developers looking for a purist, highly keyboard-controlled environment that bridges classic editor flow with modern C++ Language Server features.

## IDE - Features

A "project" is a directory that contains a `CMakeLists.txt` file. The editor can also be started in a project subdirectory.

For information about the entire project, Ped contacts a language server. For C++, the `clangd` language server is started for this purpose.
The LS parses the current source code and requires the same environment as the subsequent compiler run so that it, for example, finds all header files. For this, you must instruct CMake to generate a `compile_commands.json` file that contains exactly this information.

CMake is called for this as follows:

    cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja

CMake then writes the "compile_commands.json" into the build directory "build".
If Ped is started within the project directory or a project subdirectory, the `clangd` LS finds everything else automatically.

### **Global Commands**

      [F1]                Saves modified files and exits the editor.
      [Shift+F1]          Saves
      [Ctrl+K, Ctrl+Q]    Exits the editor **without** writing back modified files.
      [Escape]            "Enter" command: opens input window on the status line to enter parameters
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

### **Editing Text**

      [Ctrl+Z]                  CMD_UNDO
      [Shift+Ctrl+Z]            CMD_REDO
      [Delete; Backspace]       CMD_RUBOUT
      [Ctrl+G]                  CMD_CHAR_DELETE
      [Ctrl+N]                  CMD_INSERT_LINE
      [F8]                      CMD_PICK
      [F9]                      CMD_PUT
      [Ctrl+Y]                  CMD_DELETE_LINE
      [Ctrl+Q,  Ctrl+Y]         CMD_DELETE_LINE_RIGHT
      [Ctrl+T]                  CMD_DELETE_WORD


### **File Commands**

      [Ctrl+K,  Ctrl+S]         CMD_SAVE
      [Ctrl+K,  Ctrl+K; F4]     CMD_KONTEXT_COPY
      [Ctrl+K,  Ctrl+J; Shift+F3] CMD_KONTEXT_PREV
      [Ctrl+K,  Ctrl+L; F3]     CMD_KONTEXT_NEXT
      [Ctrl+K,  Ctrl+I]         CMD_SHOW_BRACKETS
      [Ctrl+Up]                 CMD_KONTEXT_UP
      [Ctrl+Down]               CMD_KONTEXT_DOWN
      [F5]                      CMD_SELECT_ROW
      [F6]                      CMD_SELECT_COL
      [Ctrl+O,  Ctrl+W]         CMD_ENTER_WORD


### **Search/Replace**

      [Enter <text> F7]               search for `text`
      [Enter <search>/<replace> F7]   search `search` and replace with `replace`
      [F7]                            search forward
      [SHIFT+F7]                      search backward
      [Ctrl+O, Ctrl+R]                project-wide renaming via Language Server

### **Copy/Cut Paste**
#### Selection Modes

NPed supports three types of selections:

      [F5]              CMD_SELECT_ROW    selects rows
      [F6]              CMD_SELECT_COL    selects columns
      [Ctrl + F5]       CMD_SELECT_CHAR   selects all characters between a start and end marker

#### Selection Functions

      [F8]              CMD_PICK          Copies selection (Copy) or the current line
                                          if selection is empty (Pick) into Copy Buffer.
      [Ctrl + Y]        CMD_DELETE_LINE   Copies selection or current line
                                          if selection is empty into Copy Buffer and then deletes it.
      [F9]              CMD_PUT           Inserts Copy Buffer (Paste/Put) at current
                                          cursor position.
      [MMT]             Paste             Copies content of the system clipboard to
                                          cursor position.

### **IDE Commands**

      [Ctrl+K,  Ctrl+M]             CMD_SHOW_LEVEL

#### **`Ctrl+O, Ctrl+F`** Format a file
The format command is sent to the Language Server. An LS for the current file type must be configured and available, and it must support the format command. For C/C++, this works great with the clangd LanguageServer.

#### **`Ctrl+V`** Switches to a different view of the current file.
For Markdown and HTML files, the view switches from editable text representation to a rendered 'read-only' representation.
For C/C++ files, a list of function or method names is shown. You can position the cursor on a function and switch back to the text representation to navigate to that function at lightning speed.

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

## AI Agent
### Model

LLM models must be configured and then appear in the AI panel pulldown menu "Models".
If an Ollama server is running on the computer, the available Ollama models are automatically detected and added to the pulldown menu.

Models from Google (Gemini), Anthropic (Claude), and Ollama are supported.
Ollama is particularly interesting because it allows for local models to be used.
Ollama is, for example, hardcoded to be accessed via the URL http://localhost:11434.

Reasoning is supported for suitable models.

### Session

Sessions are automatically saved to disk. New sessions can be created and existing ones deleted. You can switch between sessions via the Session pulldown menu in the AI panel.
A session is always associated with an AI model. When switching a model, the corresponding model is always selected. If the model is changed, the current session is saved and a new session is created for this model.

### Build/Plan Mode

In "Plan" mode, the AI model only has read access to the system. In "Build" mode, the AI model can also create/delete and modify files in and below the project directory.

If NPed was not started in a project, the "Build" mode cannot be turned on.

## Installation
### Font
      There are various fonts that are particularly suitable for programming:

- Fira Code
The concept behind Fira Code is simple: the monospaced font was developed to combine frequently used character sequences into one, shortening the time you need to scan your code and find what you are looking for.

For example, the equals sign (!=) becomes an equals sign with a slash, the opening and closing symbols in HTML (</) are closer together, and so on. These ligatures exist for many programming languages.

The underlying characters themselves are not changed, so there is no impact on your code. It just makes it easier to read! There are also several character variations so you can customize the font to your liking.

Fira Code is supported by most browsers and you can look at the code examples to see how it looks in practice.

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

Nped can only use fonts that are non-proportional (monospace).

### Language Server

For the "C" language family: c, c++

      sudo apt install clangd

### Core Dumps

A standard Kubuntu Linux does not create core dumps. Core dumps are essential for testing and debugging programs.

To enable core dumps on Ubuntu/Kubuntu you must:

- Disable Apport

      sudo nano /etc/default/apport

change "enabled=1" to "enabled=0"

then:

      sudo systemctl stop apport.service
      sudo systemctl disable apport.service

## Examples
[Examples](manual/examples.md)
