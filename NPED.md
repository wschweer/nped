# NPed — Agent's Guide

> **NPed** (Program Editor) is a modern C++23 / Qt6-based text editor and lightweight IDE with
> built-in Language Server Protocol (LSP) integration and a deeply integrated AI agent system.
> Copyright (C) 2025-2026 Werner Schweer. Licensed under GPL v2.

---

## 1. Quick Orientation

| What             | Where                                      |
|------------------|--------------------------------------------|
| Project root     | `/home/ws/nped`                            |
| Build directory  | `/home/ws/nped/build`                      |
| Source files     | `src/` (flat — all `.cpp`/`.h` in one dir) |
| Resources        | `res/` (HTML, CSS, JS for diff/config views) |
| Icons (SVG)      | `images/`                                  |
| Qt Stylesheets   | `src/light.qss`, `src/dark.qss`            |
| Manual & docs    | `manual/`                                  |
| User manual      | `manual/manual.md`                         |
| Build docs       | `manual/build.md`                          |
| Config schema    | `res/config.json`                          |
| Session storage  | `.nped/` (JSON session files)              |
| Settings file    | `.nped.json` (project-local settings)      |
| Build system     | `CMakeLists.txt` + Ninja                   |
| Helper Makefile  | `Makefile` (convenience targets)           |

---

## 2. Build & Run

### Prerequisites
- **C++23 compiler**: clang++ (preferred) or g++
- **Qt 6** (≥ 6.5): Core, Gui, Widgets, Network, WebEngineCore, WebEngineWidgets, WebChannel, DBus, PrintSupport, Concurrent
- **libgit2** (Git integration)
- **md4c / md4c-html** (Markdown rendering)
- **FFmpeg** libraries: libavcodec, libavformat, libavutil, libswscale (video frame extraction)
- **clangd** (language server for C/C++ — used at runtime, not build-time)
- **Bubblewrap (bwrap)** (sandboxing for AI agent shell commands)

### Build Commands
```bash
# Using clang (preferred):
rm -rf build; mkdir build; cd build
cmake -D CMAKE_CXX_COMPILER=clang++ -G Ninja ..
cmake --build . --parallel 32

# Using gcc:
rm -rf build; mkdir build; cd build
cmake -G Ninja ..
cmake --build . --parallel 32

# Or simply:
make nped    # uses the convenience Makefile → builds and runs
make t       # run built binary
make d       # debug with gdb
make v       # valgrind
```

### Run
```bash
./build/nped <file1> <file2> ...    # one or more files must be given
```
If no files are provided, the application shows an error and exits.

### Command-line flags
- `-v` : print version
- `-l` : use ISO Latin1 codec
- `-u` : use UTF8 codec (default)
- `-t` : do not remember settings (non-persistent)

---

## 3. Architecture Overview

```
main.cpp
  └── Editor (QMainWindow)              ← central hub, owns everything
        ├── EditWidget                   ← custom-rendered text editing area
        ├── TabBar                       ← multi-file tab management
        ├── Kontext[]                    ← one per open file/tab (cursor, selection, view mode)
        │     └── File                   ← file content, undo stack, syntax marks
        ├── LSclient[]                   ← language server connections (e.g. clangd)
        ├── Agent                        ← AI agent panel (right sidebar)
        │     ├── LLMClient              ← abstract LLM interface
        │     │     ├── OllamaClient
        │     │     ├── GeminiClient
        │     │     ├── Gemini2Client
        │     │     ├── AnthropicClient
        │     │     └── OpenAiClient
        │     ├── ChatDisplay            ← markdown-rendered chat (QWebEngineView)
        │     ├── Session                ← persistent chat history (JSON)
        │     ├── Dashboard              ← status bar with token count, model selector
        │     ├── ScreenshotHelper       ← XDG portal screenshots
        │     ├── McpManager             ← MCP (Model Context Protocol) servers
        │     └── tools (in tools.cpp)   ← tool definitions & execution
        ├── MarkdownWebView              ← rendered markdown/HTML view (QWebEngineView)
        ├── ConfigWebView                ← HTML-based config UI (json-editor)
        ├── Git                          ← libgit2 wrapper (status, diff, history)
        ├── Project Panel                ← file tree (QFileSystemModel)
        └── Git Panel                    ← git history list view
```

---

## 4. Core Components — Detailed

### 4.1 Editor (`editor.h` / `editor.cpp`)
The `Editor` class is the `QMainWindow` and **central orchestrator** of the entire application.
- Manages a list of `Kontext` objects (one per open file tab).
- Handles all keyboard shortcuts via a `KeyLogger` event filter that maps key sequences to `Action` objects.
- Owns the `Agent`, `Git`, project panel, git panel, and side panel stack.
- Persists settings (font, theme, shortcuts, models, agent roles, file types, language servers, MCP servers) to `.nped.json`.
- Commands are defined in the `Cmd` enum (editor.h) — covers cursor movement, editing, file ops, IDE features, view toggles.
- **PROP/PROPV macros** (types.h): Qt property declarations with getter/setter/signal auto-generated.

### 4.2 File & Line System (`file.h`, `line.h`, `kontext.h`)
- **`File`**: Represents a loaded file. Contains `Lines _fileText` (the actual content), `UndoStack`, syntax marks, git history, search results, outline symbols. Manages LSP `didOpen`/`didClose`/`didChange` notifications.
- **`Line`**: Extends `QString` with marks (syntax highlighting), labels, fold marks, and tag positions. Supports C++ token marking (`markCppToken`).
- **`Lines`**: `QList<Line>` with text manipulation helpers (`removeText`, `insertText`).
- **`Kontext`**: Represents a view onto a `File` — cursor position, screen position, selection, view mode (File, Outline, Annotations, GitVersion, GitDiff, SearchResults, WebView). Handles cursor movement, selection modes (row/col/char), and scrolling.
- **`ViewMode`**: File (normal text), Outline (function list), Annotations (LSP diagnostics), GitVersion (read-only historical version), GitDiff, SearchResults, WebView (rendered markdown/HTML).

### 4.3 Undo System (`undo.h`)
- `UndoStack` manages `UndoCommand` objects. `Patch` is the primary command type — it stores a list of `PatchItem` (position, characters to remove, text to insert).
- Supports macros (`beginMacro`/`endMacro`) for grouping multiple edits.
- Dirty tracking with `cleanIdx` for modified-file detection.

### 4.4 Language Server Client (`lsclient.h`, `ls.h`)
- **`LSclient`**: Communicates with language servers (e.g. `clangd`) via stdio pipes. Runs a reader thread. Supports initialization, completion, formatting, goto-definition/type/implementation, hover, rename, document symbols, references, and workspace symbol search.
- **`LanguageServerConfig`**: Defines name, command, and args for each language server. Configured in the settings UI.
- Multiple language servers can run simultaneously; `Editor::getLSclient(name)` retrieves a specific one.

### 4.5 AI Agent (`agent.h`, `agent.cpp`, `tools.cpp`)
The agent is the standout feature of NPed — it provides an AI assistant with deep project integration.
- **`Agent`** (QWidget): The right-side panel. Contains model selector, session manager, chat display, prompt input (`DropAwarePlainTextEdit` with image drag-and-drop), attachment buttons, screenshot button, stop button.
- **`LLMClient`** (abstract, `llm.h`): Interface for LLM providers. Each provider implements `prompt()`, `processJsonItem()`, `dataFinished()`, `setTools()`.
  - **`OllamaClient`**: Local models via Ollama (http://localhost:11434). Uses `ollama-hpp` submodule. Supports thinking/reasoning, tool calls, streaming.
  - **`GeminiClient`**: Google Gemini API (v1beta). Schema sanitization for Gemini's format.
  - **`Gemini2Client`**: Google Gemini Interactions API (v1alpha). Alternative endpoint.
  - **`AnthropicClient`**: Anthropic Claude API. Supports extended thinking blocks with signatures. Token tracking.
  - **`OpenAiClient`**: OpenAI API (untested per status docs).
  - Factory: `llmFactory()` in `llm.cpp` creates the right client based on `Model::api`.
- **Tools** (`tools.cpp`): The agent can call these tools (registered in `Agent::getMCPTools()`, dispatched in `Agent::executeToolImpl()`):
  - `search_project` — full-text search across project files
  - `find_symbol` — LSP symbol search
  - `get_file_outline` — LSP document symbols
  - `get_diagnostics` — LSP compiler diagnostics
  - `find_references` — LSP find-all-references
  - `format` — format a source file via LSP
  - `build_project` — run the CMake build (optionally a single `target`)
  - `bash_command` — execute shell commands (sandboxed via `bwrap`, optionally as `ai` user)
  - `run_valgrind` — run Valgrind with XML output, compressed to JSON
  - `extract_video_frames` — extract frames from video files via FFmpeg
  - `project_infos` — returns project root and build directory
  - `llm_info` — discovered LLM metadata (context window, capabilities) + history budget
  - **MCP tools** — dynamically loaded from configured MCP servers

  Internal helper methods (`readFile`, `writeFile`, `replaceLines`, `listDirectory`,
  `listFilesRecursive`, `getGitStatus`, `getGitDiff`, `getGitLog`, `createGitCommit`,
  `fetchWebDocumentation`) exist on `Agent` but are **not** registered as agent tools —
  file and Git access is currently provided through MCP servers.
- **Agent Roles**: Defined in settings. Each role has a name, a system prompt (manifest), read/write flag (`rw`), and a list of MCP servers. Plan mode = read-only; Build mode = read/write.
- **Canned Prompts**: Pre-defined prompt templates accessible from the UI.
- **LLM metadata discovery** (`llminfo.h`): on startup (and after the first
  answer) the agent probes the Ollama endpoints and stores the results in a
  persistent cache (`llm_info.json`, see §6.1). The discovered context window
  is the basis for the dynamic history budget.

### 4.6 Session Management (`session.h`)
- `Session`: Stores chat history as a `SessionItem` vector (each item is JSON content + token count).
- **Token accounting**: every message gets a token estimate via `Session::estimateTokens()` (~4 chars/token; covers `content`, Gemini `parts`, `tool_calls` and attachments). Provider-reported usage is stored separately in `lastReportedContextTokens` and used as a safety floor (`effectiveTokens()`), because it reflects the whole request context rather than a single message.
- **Token management**: a rolling window bounded by the model-dependent token budget (`contextBudget()` = 75 % of the model's context window, default ~90k tokens), a message cap `maxEntries` and a floor `minEntries`. The context window is taken from the discovered LLM metadata (see §6.1) with the model's `configuration` (`contextWindow`/`num_ctx`) as override/fallback. `trim()` first collapses redundant/unbounded tool outputs, then shrinks the window at `user` turn boundaries. Inside a running tool loop it only shrinks oversized tool outputs so a long loop cannot overflow the context window.
- Sessions are persisted as JSON files in `.nped/` directory.
- Session naming: `Session-YY-MM-DD-NN.json`.

### 4.7 MCP Integration (`mcp.h`)
- **`McpManager`**: Manages multiple `McpServer` instances.
- **`McpServer`**: Connects to MCP servers via subprocess (stdio) or SSE (HTTP). Discovers tools, resources, and roots. Supports tool calling with callbacks.
- **`MCPToolBuilder`**: Helper class for constructing tool definitions in JSON format compatible with all LLM providers.
- MCP servers are configured in settings (`McpServerConfig`: id, command, args, env, enabled, url).

### 4.8 Git Integration (`git.h`)
- **`Git`**: Wrapper around libgit2. Provides `isClean()`, `getCurrentBranch()`, `getHistory()`, `getFile()` (historical version), `getDiff()`.
- **`GitHistory`**: Commit OID + summary for history display.
- Git panel shows commit history; clicking a commit shows that version of the file.
- The AI agent accesses Git through MCP servers (e.g. `mcp-server-git`); a set of
  internal `Agent` Git helper methods exists but is not registered as tools.

### 4.9 WebView System (`webview.h`, `configwebview.h`, `chatdisplay.h`)
- **`MarkdownWebView`**: `QWebEngineView` subclass for rendering Markdown to HTML. Uses md4c for markdown→HTML conversion, then injects CSS (GitHub-style light/dark), highlight.js, Mermaid.js, KaTeX. Supports scroll control, diff display.
- **`ChatDisplay`**: Specialized `MarkdownWebView` for AI chat. Handles streaming chunks (thought + text), role-based message display, tool call formatting.
- **`ConfigWebView`**: Renders the configuration UI from `res/config.json` using json-editor. Bidirectional communication with C++ via `QWebChannel`.

### 4.10 Syntax Highlighting (`pretty.cpp`, `textstyle.h`)
- Custom C/C++ syntax highlighter in `File::markCpp()`. Tokenizes C++ code and applies `TextStyle::Style` marks (Flow keywords, Type keywords, Comments, Strings).
- `TextStyle` defines foreground/background colors, bold, italic per style category.
- Light and dark style sets are maintained separately and switchable.

### 4.11 Search (`search.cpp`)
- Search & replace with regular expressions.
- Project-wide search via `Editor::search()`.
- Search results displayed as a special `ViewMode::SearchResults`.

### 4.12 Code Formatting (`pretty.cpp`)
- LSP-based formatting via `LSclient::formattingRequest()`.
- Uses `clang-format` through the language server. The editor has a custom post-processor for the preferred code style (see `manual/status.md`).

### 4.13 Screenshot (`screenshot.h`)
- `ScreenshotHelper`: Takes screenshots via XDG Desktop Portal (D-Bus). Works on both Wayland and X11. Screenshots are added as image attachments to the AI prompt.

### 4.14 Configuration System
- Settings are stored in `.nped.json` at the project root.
- The config UI is rendered in `ConfigWebView` using a JSON schema (`res/config.json`) and json-editor library.
- Configurable: dark mode, font, font size, scale, shortcuts, text styles (light/dark), file types, language servers, AI models, agent roles, canned prompts, MCP servers.

### 4.15 Logging (`logger.h`)
- Custom logger with `Debug`, `Info`, `Log`, `Warning`, `Critical`, `Fatal` macros.
- Uses `std::format` for type-safe formatting.
- Custom `std::formatter` specializations for `QString`, `QStringView`, `QSize`.
- Log file: `.nped.log`.
- `Fatal` calls `abort()`. `Assert` macro for debug assertions.

---

## 5. Key Types (types.h)

| Type             | Purpose                                                   |
|------------------|-----------------------------------------------------------|
| `Pos`            | Column/row position in a file                             |
| `Cursor`         | File position + screen position                           |
| `Selection`      | Selection mode, start/end positions, cursor               |
| `SelectionMode`  | NoSelect, RowSelect, ColSelect, CharSelect                |
| `Range`          | Start/end Pos with JSON serialization (LSP ranges)        |
| `PatchItem`      | Position, characters to remove, text to insert            |
| `Attachment`     | Image/Text/Audio/Other attachment for AI prompts           |
| `PickText`       | Copy buffer content with selection mode                   |
| `Callback`       | `std::function<void(const json&)>` for LSP responses      |

---

## 6. LLM Model Configuration

Models are configured in settings (`.nped.json`) and selected via the dropdown in the AI panel.

| Field              | Description                                        |
|--------------------|----------------------------------------------------|
| `name`             | Display name                                       |
| `modelIdentifier`  | API model identifier (e.g. `claude-sonnet-4-20250514`) |
| `baseUrl`          | API endpoint URL                                   |
| `apiKey`           | API key (stored in settings)                       |
| `api`              | Provider: `ollama`, `gemini`, `gemini2`, `anthropic`, `openai` |
| `maxTokens`        | Max output tokens (<0 = per-client default)        |
| `configuration`    | JSON string of provider-native options, passed through to the provider unchanged (e.g. Ollama `options` keys like `temperature`, `top_p`, `num_ctx`; Anthropic `thinking`). Used as fallback/override for the history budget (`contextWindow` or `num_ctx`); the primary source is the discovered LLM metadata (see §6.1). |
| `stream`           | Enable streaming responses                         |
| `protected`        | Sandbox (bwrap) agent tools (true) vs host (false) |
| `dynamic`          | Auto-detected (e.g. Ollama models) — not saved     |

### 6.1 Discovered LLM metadata (`llminfo.h`)

For every Ollama model the agent collects provider metadata and persists it in
`<AppConfig>/llm_info.json` (keyed by model identifier) so it survives restarts
and is available before the first request.  The `LlmInfo` struct holds:

| Field                  | Source                  | Purpose                                    |
|------------------------|-------------------------|--------------------------------------------|
| `trainedContextLength` | `/api/show` `model_info`| context size of the trained model          |
| `runtimeContextLength` | `/api/ps`               | `num_ctx` Ollama actually uses (preferred) |
| `modelfileNumCtx`      | `/api/show` parameters  | `num_ctx` pinned in the Modelfile          |
| `architecture`, `family`, `parameterSize`, `quantization`, `blockCount`, `embeddingLength` | `/api/tags` + `/api/show` | model identification |
| `digest`               | `/api/tags`             | detect model changes/updates               |
| `capabilities`         | `/api/tags` + `/api/show` | completion, tools, thinking, vision, audio |

`LlmInfo::effectiveContextLength()` returns `runtimeContextLength` when known,
otherwise `trainedContextLength`.  `Session::discoveredContextWindow()` builds on
this, but lets an explicitly configured `num_ctx` take precedence over the
trained size until `/api/ps` confirms the runtime value.  The result drives
`Session::contextBudget()`, so the chat history is trimmed in line with the
model's real context window.

Endpoints and their role:
- `GET /api/tags` — cheap model list; carries `details.context_length` and
  `capabilities`.  Also used to auto-add models (`dynamic = true`).
- `POST /api/show` — `model_info.<arch>.context_length` (trained size),
  architecture, Modelfile `parameters` and `details`.
- `GET /api/ps` — models currently loaded, including the context length actually
  in use (queried again after the first answer).

Additionally, Ollama reports the real token accounting (`prompt_eval_count`,
`eval_count`) in the final streaming chunk; these are fed to
`Session::setReportedContextTokens()` as a safety floor.  The agent exposes all
of this via the `llm_info` tool (`Agent::executeTool()` in `tools.cpp`).

---

## 7. File Types & Language Servers

- **File types** (`filetype.h`): Map file extensions to language ID, language server, tab size, header flag, and tab creation flag.
- **Language servers** (`ls.h`): Configured by name, command, and args. The editor starts them as subprocesses and communicates via JSON-RPC over stdio.
- Default for C/C++: `clangd` (must be installed separately).

---

## 8. Important Patterns & Conventions

1. **All source in `src/`** — flat directory structure, no subdirectories.
2. **`#pragma once`** — used in all headers (no include guards).
3. **Naming**: PascalCase for classes/structs, camelCase for methods/variables, UPPER_CASE for constants, `CMD_*` for enum values, `_` prefix for member variables (sometimes).
4. **Qt MOC**: `CMAKE_AUTOMOC ON` — all `Q_OBJECT` classes are auto-processed.
5. **JSON**: Uses `nlohmann::json` everywhere (LSP, LLM, settings, sessions).
6. **`std::format`** — used extensively for string formatting (C++23).
7. **Path safety**: `Agent::isPathSafe()` ensures AI tools cannot access files outside the project root.
8. **Sandboxing**: Shell commands from the AI agent run in `bwrap` (Bubblewrap) with read-only root filesystem and project directory bind-mounted (read-write in Build mode, read-only in Plan mode).
9. **Output truncation**: All tool outputs have size limits (e.g. `kBuildLogMaxChars`, `kWebFetchMaxChars`) to prevent context window overflow.
10. **File descriptor hygiene**: QProcess instances call `close()` immediately after `waitForFinished()`. LSclient pipes have `FD_CLOEXEC` set.
11. **Debug builds only** — `CMAKE_BUILD_TYPE` is set to `Debug` by default. `NDEBUG` disables logging macros.
12. **Backup files**: Files starting with `.` and ending with `,` (e.g. `.editor.h,`) are NPed's backup/swap files. They are listed alongside originals in the `src/` directory.

---

## 9. Git Submodules

- `ollama-hpp` — Header-only C++ library for Ollama API communication. Located at `ollama-hpp/include/`.
- `res` — (in `.git/modules/`) Resources for the application.

Run `git submodule update --init --recursive` after cloning.

---

## 10. Current State & Known Issues

(from `manual/status.md`)
- **Editor**: Nearly feature-complete for the author's workflow.
- **Language Server**: Implementation incomplete, may contain bugs, experimental integration.
- **Code Formatting**: Uses `clang-format` with a custom post-processor hack. Swapping `clang-format` config alone won't produce the desired style.
- **AI Integration**: Works with Claude (Anthropic), Gemini (Google), and Ollama (local). OpenAI is untested. Context optimization (token reduction) needs further refinement. AI sometimes gets stuck in loops.

---

## 11. TODO

(from `TODO.md`)
- **Video Tool**: Create a video tool to extract image sequences from video files using FFmpeg. Encode images like screenshots, adjust resolution, and pass them as a structured list to the AI model. Parameters: video filename, start image number, image count, interval between images.
  - **Status**: Partially implemented in `tools.cpp` (`extractVideoFrames` using libavcodec/libavformat/libswscale).

---

## 12. Common Tasks for an AI Agent Working on This Project

### Adding a new tool for the AI agent
1. Add tool definition in `Agent::getMCPTools()` (tools.cpp) using `MCPToolBuilder`.
2. Add execution handler in `Agent::executeTool()` (tools.cpp).
3. Implement the actual logic (either inline or as a method on `Agent`).
4. Ensure path safety via `isPathSafe()` for file-related tools.
5. Add output truncation if the tool can produce large output.

### Adding a new LLM provider
1. Create `NewProviderClient` class inheriting `LLMClient` (see `llm.h`).
2. Implement `prompt()`, `processJsonItem()`, `dataFinished()`, `setTools()`, `name()`.
3. Register in `llmFactory()` (llm.cpp) — map `Model::api` string to the new class.
4. Add the provider name to the `api` enum options in `res/config.json`.

### Adding a new editor command
1. Add entry to the `Cmd` enum in `editor.h`.
2. Add shortcut config entry in the shortcuts initialization (search for `CMD_` in editor.cpp).
3. Create an `Action` with the key sequence and handler function.
4. Implement the command logic as a method on `Editor` or `Kontext`.

### Modifying syntax highlighting
1. Edit `File::markCpp()` in `pretty.cpp` for C/C++ token marking.
2. Add new `TextStyle::Style` enum value in `textstyle.h` if needed.
3. Update the default text style lists in editor.cpp initialization.

### Modifying the configuration UI
1. Edit `res/config.json` to add/modify config sections.
2. Add corresponding `Q_PROPERTY` to the relevant class (e.g., `Editor`).
3. Ensure the property type is registered with the Qt meta-object system (`Q_DECLARE_METATYPE`).

---

## 13. File Index (src/)

| File                   | Purpose                                              |
|------------------------|------------------------------------------------------|
| `main.cpp`             | Entry point, command-line parsing, creates `Editor`  |
| `editor.h/cpp`         | Main window, command system, shortcut management     |
| `editwin.h/cpp`        | Custom `QWidget` for text rendering and input        |
| `file.h/cpp`           | File model: content, loading, saving, syntax marks   |
| `line.h`               | `Line` (extends QString with marks) and `Lines`      |
| `kontext.h/cpp`        | View context: cursor, selection, view modes          |
| `undo.h/cpp`           | Undo/redo stack and commands (`Patch`)               |
| `types.h`              | Core types: `Pos`, `Cursor`, `Selection`, `Range`    |
| `globals.h`            | Global `persistent` flag                             |
| `logger.h/cpp`         | Logging system with `std::format` support            |
| `lsclient.h/cpp`       | Language Server Protocol client (stdio JSON-RPC)     |
| `ls.h/cpp`             | Language server config and list types                |
| `completion.h/cpp`     | LSP completion data model and popup widget           |
| `completer.h/cpp`      | Line edit with history and autocomplete              |
| `search.cpp`           | Search & replace with regex                          |
| `git.h/cpp`            | libgit2 wrapper: status, diff, history               |
| `pretty.cpp`           | Syntax highlighting (C/C++ tokenizer)               |
| `agent.h/cpp`          | AI agent panel: UI, session, tool orchestration      |
| `tools.cpp`            | AI agent tool definitions and execution              |
| `llm.h/cpp`            | Abstract LLM client interface + factory              |
| `ollama.h/cpp`         | Ollama (local) LLM client                            |
| `gemini.h/cpp`         | Google Gemini LLM client (v1beta)                    |
| `gemini2.h/cpp`        | Google Gemini Interactions API client (v1alpha)      |
| `anthropic.h/cpp`      | Anthropic Claude LLM client                          |
| `openai.h/cpp`         | OpenAI LLM client (untested)                         |
| `session.h/cpp`        | Chat session persistence and token management        |
| `mcp.h/cpp`            | Model Context Protocol: server management, tools     |
| `webview.h/cpp`        | Markdown/HTML rendering via QWebEngineView           |
| `configwebview.h/cpp`  | Configuration UI via QWebEngineView + json-editor    |
| `chatdisplay.h/cpp`    | AI chat display (streaming, markdown, tool calls)    |
| `dashboard.h/cpp`      | Status bar with token count and controls             |
| `model.h/cpp`          | LLM model configuration data structure               |
| `llminfo.h`            | Discovered LLM metadata (`LlmInfo`) + persistent cache (`LlmInfoStore`) |
| `screenshot.h/cpp`     | XDG portal screenshot helper                         |
| `textstyle.h`          | Text style definition (colors, bold, italic)         |
| `filetype.h/cpp`       | File type configuration (extensions, language, tab)  |
| `attachmentbutton.h`   | UI button for AI prompt attachments                  |
| `vectormodel.h`        | Generic `QAbstractListModel` wrapper for `std::vector`|
| `light.qss/dark.qss`   | Qt stylesheets for light/dark themes                 |

---

## 14. Key Constants & Limits

| Constant                     | Value        | Purpose                                    |
|------------------------------|--------------|--------------------------------------------|
| `Session::maxEntries`        | 80           | Hard cap on active messages in chat history  |
| `Session::minEntries`        | 10           | Safety floor for history trimming            |
| `Session::defaultTokenBudget`| 90000        | Fallback token budget (models without a discovered/configured context window) |
| `Agent::kBuildLogMaxChars`   | 200000       | Max build log output to LLM                 |
| `Agent::kWebFetchMaxChars`   | 80000        | Max web fetch output to LLM                 |
| `Agent::kGitDiffMaxChars`    | 10000        | Max git diff output to LLM                  |
| `Agent::kMaxAttachmentSize`  | 2 MB         | Max attachment file size                    |
| `Agent::kSearchMaxChars`     | 10000        | Max search output to LLM                    |
| `Agent::kChatResultMaxChars` | 20000        | Max chat result output                      |
| `Agent::kChatMaxMessages`    | 40           | Max messages in chat context                |
| `Agent::maxRetries`          | 12           | Max LLM API retries                         |

---

_This document is auto-generated for AI agents working on the NPed project. Last updated: 2026-07-26._