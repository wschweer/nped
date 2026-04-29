//=============================================================================
//  nped Program Editor
//
//  Copyright (C) 2025-2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include <QTextEdit>
#include <QDir>
#include <QProcess>
#include <sstream>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDirIterator>
#include <QPlainTextEdit>
#include <QXmlStreamReader>

// #include <list>
#include <functional>
#include <QEventLoop>
#include <QTimer>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "editor.h"
#include "mcp.h"
#include "agent.h"
#include "logger.h"
#include "undo.h"
#include "kontext.h"
#include "file.h"
#include "lsclient.h"

//---------------------------------------------------------
//   getMCPTools
//    return list of available tools
//
//    {
//    "name": "get_weather",
//    "description": "Ruft das aktuelle Wetter ab",
//    "parameters": {
//    "type": "OBJECT",
//    "properties": {
//          "location": { "type": "STRING" }
//          },
//     "required": ["location"]
//     }
//---------------------------------------------------------

std::vector<json> Agent::getMCPTools() const {
      try {
            std::vector<json> tools = json::array();

            // 1. File Operations
            tools.push_back(
                      MCPToolBuilder("format_source", "Formats a source file using the Language Server.")
                          .add_parameter("path", "string", "The path to the file to format.")
                          .build());

            // 2. Navigation & Search
            tools.push_back(MCPToolBuilder("search_project", "Searches for a text query across all files in "
                                                             "the project, including unsaved editor buffers.")
                                .add_parameter("query", "string", "The text to search for.")
                                .add_parameter("file_pattern", "string",
                                               "Optional glob pattern to filter files (e.g., '*.cpp').",
                                               false)
                                .build());

            tools.push_back(
                MCPToolBuilder("find_symbol", "Uses the Language Server (LSP) to find the definition of a "
                                              "symbol like a class or function.")
                    .add_parameter("symbol", "string", "The name of the symbol to locate (e.g., 'MyClass').")
                    .build());

            tools.push_back(MCPToolBuilder("get_file_outline",
                                           "Uses the Language Server (LSP) to get a hierarchical "
                                           "outline of classes, methods, and functions in a file.")
                                .add_parameter("path", "string", "The path to the file to analyze.")
                                .build());

            tools.push_back(
                MCPToolBuilder("get_diagnostics",
                               "Retrieve Language Server diagnostics (errors, warnings) for a file.")
                    .add_parameter("path", "string", "The path to the file to get diagnostics for.")
                    .build());

            // 5. MCP Tools
            for (const auto& config : _editor->mcpServersConfig()) {
                  if (!config.enabled)
                        continue;
                  McpServer* server = _mcpManager->getServer(config.id);
                  if (server) {
                        for (const auto& tool : server->getTools()) {
                              json jtool           = json::object();
                              jtool["name"]        = tool.name;
                              jtool["description"] = tool.description;
                              jtool["inputSchema"] = tool.inputSchema;
                              tools.push_back(jtool);
                              }
                        }
                  }

            tools.push_back(
                MCPToolBuilder("find_references", "Uses the Language Server to find all references to a "
                                                  "symbol at a specific file and position.")
                    .add_parameter("path", "string", "The path to the file containing the symbol.")
                    .add_parameter("line", "integer", "The 1-based line number of the symbol.")
                    .add_parameter("column", "integer", "The 1-based column number of the symbol.")
                    .build());

            tools.push_back(
                MCPToolBuilder(
                    "run_valgrind",
                    "Executes a compiled C/C++ program under Valgrind and returns a compressed error report.")
                    .add_parameter("executable", "string",
                                   "The path to the executable (e.g. ./build/my_app).")
                    .add_parameter("args", "string", "Command line arguments for the target program.", false)
                    .add_parameter("tool", "string",
                                   "The Valgrind tool to use: 'memcheck' (default), 'helgrind', 'massif'.",
                                   false)
                    .build());

            // 3. System & External
            tools.push_back(
                MCPToolBuilder("bash_command", "Executes a shell command. Use this for general bash tasks.")
                    .add_parameter("command", "string", "The bash command to execute.")
                    .build());
            tools.push_back(
                MCPToolBuilder(
                    "build_project",
                    "Builds the project. An optional parameter can be given to build a specific target.")
                    .add_parameter("target", "string", "The optional target to build.", false)
                    .build());

            // 4. Git Integration
            tools.push_back(
                MCPToolBuilder("get_git_status",
                               "Returns the current git status of the repository. No parameters needed.")
                    .build());

            tools.push_back(
                MCPToolBuilder("get_git_diff",
                               "Shows uncommitted changes as a diff. Helps review edits before committing.")
                    .add_parameter("path", "string", "Optional: path to a specific file to diff.", false)
                    .build());

            tools.push_back(
                MCPToolBuilder("get_git_log", "Displays the recent git commit history.")
                    .add_parameter("limit", "integer", "Number of commits to retrieve (default: 5).", false)
                    .build());

            if (isExecuteMode()) {
                  tools.push_back(
                      MCPToolBuilder("create_git_commit",
                                     "Stages all current changes (git add .) and creates a new commit.")
                          .add_parameter("message", "string", "A clear and concise commit message.")
                          .build());
                  }
            return tools;
            }
      catch (const json::parse_error& e) {
            Debug("Parse Error: {}", e.what());
            }
      return std::vector<json>();
      }

//---------------------------------------------------------
//   executeTool
//---------------------------------------------------------

std::string Agent::executeTool(const std::string& functionName, const json& arguments) {
      // --- MCP Tool Check ---
      for (const auto& config : _editor->mcpServersConfig()) {
            if (!config.enabled)
                  continue;
            McpServer* server = _mcpManager->getServer(config.id);
            if (server) {
                  for (const auto& tool : server->getTools()) {
                        if (tool.name == functionName) {
                              QEventLoop loop;
                              std::string result;
                              server->callTool(functionName, arguments, [&](const json& res) {
                                    result = res.dump();
                                    loop.quit();
                                    });
                              loop.exec();
                              return result;
                              }
                        }
                  }
            }

      // ==========================================================
      // Tools OHNE lokales Datei-Pfad Argument
      // ==========================================================

      if (functionName == "bash_command") {
            if (!arguments.contains("command") || !arguments["command"].is_string())
                  return "Error: Parameter 'command' missing.";
            QString cmd = QString::fromStdString(arguments["command"].get<std::string>());
            return runBashCommand(cmd);
            }
      else if (functionName == "build_project") {
            QString cmd = "cd build; cmake --build . --parallel 32";
            if (arguments.contains("target") && arguments["target"].is_string()) {
                  QString target  = QString::fromStdString(arguments["target"].get<std::string>());
                  cmd            += " --target " + target + ";";
                  }
            return compressBuildLog(runBashCommand(cmd));
            }
      else if (functionName == "run_valgrind") {
            if (!arguments.contains("executable") || !arguments["executable"].is_string())
                  return "Error: Parameter 'executable' missing.";
            QString executable = QString::fromStdString(arguments["executable"].get<std::string>());
            QString args       = arguments.contains("args") && arguments["args"].is_string()
                                     ? QString::fromStdString(arguments["args"].get<std::string>())
                                     : "";
            QString tool       = arguments.contains("tool") && arguments["tool"].is_string()
                                     ? QString::fromStdString(arguments["tool"].get<std::string>())
                                     : "memcheck";
            return runValgrindCommand(executable, tool, args);
            }
      else if (functionName == "get_git_status") {
            return getGitStatus();
            }
      else if (functionName == "get_git_diff") {
            QString p = (arguments.contains("path") && arguments["path"].is_string())
                            ? QString::fromStdString(arguments["path"].get<std::string>())
                            : "";
            return getGitDiff(p);
            }
      else if (functionName == "get_git_log") {
            int limit = (arguments.contains("limit") && arguments["limit"].is_number())
                            ? arguments["limit"].get<int>()
                            : 5;
            return getGitLog(limit);
            }
      else if (functionName == "create_git_commit") {
            if (!arguments.contains("message") || !arguments["message"].is_string())
                  return "Error: Parameter 'message' missing.";
            QString msg = QString::fromStdString(arguments["message"].get<std::string>());
            return createGitCommit(msg);
            }

      //==========================================================
      // Tools MIT lokalem Datei-Pfad Argument
      //==========================================================

      else if (functionName == "search_project") {
            if (!arguments.contains("query") || !arguments["query"].is_string())
                  return "Error: Parameter 'query' missing.";
            QString query   = QString::fromStdString(arguments["query"].get<std::string>());
            QString pattern = (arguments.contains("file_pattern") && arguments["file_pattern"].is_string())
                                  ? QString::fromStdString(arguments["file_pattern"].get<std::string>())
                                  : "";
            return searchProject(query, pattern);
            }
      else if (functionName == "find_symbol") {
            if (!arguments.contains("symbol") || !arguments["symbol"].is_string())
                  return "Error: Parameter 'symbol' missing.";
            QString symbol = QString::fromStdString(arguments["symbol"].get<std::string>());
            return findSymbol(symbol);
            }

      if (!arguments.contains("path") || !arguments["path"].is_string())
            return "Error: Parameter 'path' missing for local file tool: " + functionName;

      QString path = QString::fromStdString(arguments["path"].get<std::string>());
      if (!isPathSafe(path)) {
            Debug("Sicherheitssperre gegriffen für Pfad: {}", path.toStdString());
            return "Security Lock: Access denied! Path is outside of " + _editor->projectRoot().toStdString();
            }

      // Lokale Datei-Operationen ausführen
      if (functionName == "get_file_outline")
            return getFileOutline(path);
      else if (functionName == "get_diagnostics")
            return getDiagnostics(path);
      else if (functionName == "find_references") {
            if (!arguments.contains("line") || !arguments["line"].is_number() ||
                !arguments.contains("column") || !arguments["column"].is_number())
                  return "Error: Parameters 'line' and 'column' are missing.";
            int line   = arguments["line"].get<int>();
            int column = arguments["column"].get<int>();
            return findReferences(path, line, column);
            }
      else if (functionName == "format_source") {
            return formatSource(path);
            }
      return "Error: Unknown tool (" + functionName + ").";
      }

//---------------------------------------------------------
//   errorResponse
//---------------------------------------------------------

std::string Agent::errorResponse(const std::string& message) const {
      json error = {
               { "status", "error"},
               {"message", message}
            };
      return error.dump();
      }

//---------------------------------------------------------
//   isPathSafe
//    WICHTIG: Stellt sicher, dass das Modell keine Systemdateien
//    außerhalb des Projekts verändert
//---------------------------------------------------------

bool Agent::isPathSafe(const QString& path) {
      if (!_editor)
            return false;
      QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
      QString cleanRoot = QDir::cleanPath(QFileInfo(_editor->projectRoot()).absoluteFilePath());

      // Der Zielpfad muss zwingend mit dem Projekt-Root beginnen
      return cleanPath.startsWith(cleanRoot);
      }

//---------------------------------------------------------
//   normalizePath
//    makes sure path is always relative to projectRoot
//---------------------------------------------------------

QString Agent::normalizePath(const QString& path) const {
      if (path.startsWith("/"))
            return path;
      return _editor->projectRoot() + "/" + path;
      }

//---------------------------------------------------------
//   readFile
//    on error returns false
//---------------------------------------------------------

bool Agent::readFile(const QString& ipath, QString& result) {
      QString path = normalizePath(ipath);
      File* f      = _editor->findFile(path);
      if (f) {
            result = f->plainText();
            return true;
            }
      QFile file(path);
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result = "Error: Could not read file (" + file.errorString() + ")";
            return false;
            }
      result = QTextStream(&file).readAll();
      return true;
      }

//---------------------------------------------------------
//   searchProject
//---------------------------------------------------------

string Agent::searchProject(const QString& query, const QString& filePattern) {
      string result;
      QDir dir(_editor->projectRoot());
      QStringList filters;
      if (!filePattern.isEmpty())
            filters << filePattern;
      else
            filters << "*.cpp" << "*.h" << "*.c" << "*.hpp" << "CMakeLists.txt";
      QDirIterator it(_editor->projectRoot(), filters, QDir::Files, QDirIterator::Subdirectories);
      while (it.hasNext()) {
            QString path = it.next();
            if (path.startsWith(".") || path.contains("/build/"))
                  continue;
            QString relativePath = dir.relativeFilePath(path);
            QString content;
            if (!readFile(relativePath, content))
                  continue;
            QStringList lines = content.split('\n');
            for (int i = 0; i < lines.size(); ++i)
                  if (lines[i].contains(query))
                        result += std::format("{}:{}:{}\n", relativePath, i + 1, lines[i].trimmed());
            }
      if (result.empty())
            return std::format("No matches found for '{}'", query);
#if 0
      if (result.length() > 4000) {
            result.resize(4000);
            result += "\n... [Too many results, output truncated]";
                                                                                                                              }
#endif
      return result;
      }

//---------------------------------------------------------
//   findSymbol
//---------------------------------------------------------

string Agent::findSymbol(const QString& symbol) {
      LSclient* client = _editor->getLSclient("clangd");
      if (!client)
            return "Error: No Language Server available.";
      if (!client->initialized())
            return "Error: Language Server not ready, try again later";

      QEventLoop loop;
      string result;

      auto connection = connect(client, &LSclient::symbolSearchResult, [&](const string& res) {
            result = res;
            loop.quit();
            });

      // Timeout in case the server takes too long or fails
      QTimer timer;
      timer.setSingleShot(true);
      connect(&timer, &QTimer::timeout, [&]() {
            result = "Error: Timeout (5s) while waiting for Language Server symbol search result.";
            loop.quit();
            });

      client->symbolRequest(symbol);
      timer.start(5000); // 5 seconds

      loop.exec();

      disconnect(connection);
      return result;
      }

//---------------------------------------------------------
//   findReferences
//---------------------------------------------------------

string Agent::findReferences(const QString& file, int line, int column) {
      LSclient* client = _editor->getLSclient("clangd");
      if (!client)
            return "Error: No Language Server available.";
      if (!client->initialized())
            return "Error: Language Server not ready, try again later";

      QEventLoop loop;
      string result;

      auto connection = connect(client, &LSclient::referencesSearchResult, [&](const string& res) {
            result = res;
            loop.quit();
            });

      QTimer timer;
      timer.setSingleShot(true);
      connect(&timer, &QTimer::timeout, [&]() {
            result = "Error: Timeout (5s) while waiting for Language Server references result.";
            loop.quit();
            });

      client->referencesRequest(normalizePath(file), line, column);
      timer.start(5000); // 5 seconds

      loop.exec();

      disconnect(connection);
      return result;
      }

//---------------------------------------------------------
//   getDiagnostics
//---------------------------------------------------------

string Agent::getDiagnostics(const QString& file) {
      QString normPath = normalizePath(file);
      Kontext* k       = _editor->lookupKontext(normPath);
      if (!k)
            return "Error: File not found, so no diagnostics are available.";
      File* f = k->file();

      const Lines& bugs = f->bugs();
      if (bugs.isEmpty())
            return "No diagnostics (errors/warnings) found for this file.";

      string result;
      for (int i = 0; i < bugs.size(); ++i)
            result += bugs[i].qstring().toStdString() + "\n";
      return result;
      }

//---------------------------------------------------------
//   writeFile
//---------------------------------------------------------

string Agent::writeFile(const QString& ipath, const QString& content) {
      QString path = normalizePath(ipath);

      File* f = _editor->findFile(path);
      if (f) {
            int n = f->plainText().size();
            if (f->readOnly())
                  return "Error: File is only readable";
            f->undo()->beginMacro();
            f->undo()->push(new Patch(f, {0, 0}, n, content, Cursor(), Cursor()));
            f->undo()->endMacro();
            _editor->update();
            }
      else {
            QFile file(path);
            // QIODevice::Truncate löscht den alten Inhalt der Datei
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
                  return std::format("Error opening or creating file ({})", file.errorString());
            QTextStream out(&file);
            out << content;
            }
      return std::format("Success: File {} successfully written.", path);
      }

// ---------------------------------------------------------
// Tool 5: fetch_web_documentation
// ---------------------------------------------------------

string Agent::fetchWebDocumentation(const QString& urlString) {
      QUrl url(urlString);
      if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https"))
            return "Error: Invalid URL. Must start with http:// or https://.";

      QNetworkRequest request(url);
      QNetworkReply* reply = networkManager->get(request);

      // WICHTIGER TRICK: Da executeTool einen String zurückgeben MUSS, blockieren
      // wir hier die weitere Ausführung der Funktion, bis das Netzwerk antwortet,
      // ohne dabei das Qt-UI komplett einzufrieren (QEventLoop).
      QEventLoop loop;
      connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
      loop.exec(); // Wartet hier, bis "finished" gefeuert wird

      if (reply->error() != QNetworkReply::NoError) {
            QString errorStr = reply->errorString();
            reply->deleteLater();
            return std::format("Network error while fetching URL: {}", errorStr);
            }

      string content = reply->readAll().toStdString();
      reply->deleteLater();

      // SICHERHEITS-LIMIT: Webseiten (HTML) können absurd groß sein.
      // Wenn wir 2 Megabyte an HTML-Code an das LLM zurückschicken,
      // sprengt das sofort das "Context Window" von Ollama und es stürzt ab.
      // Wir kappen die Antwort sicherheitshalber bei 8000 Zeichen.
      if (content.length() > kWebFetchMaxChars) {
            content = content.substr(content.length() - kWebFetchMaxChars);
            content +=
                "\n\n... [ATTENTION SYSTEM: Documentation was truncated here because it was too large.]";
            }

      return content;
      }

//---------------------------------------------------------
//   replaceLines
//---------------------------------------------------------

string Agent::replaceLines(const QString& ipath, int startLine, int linesToDelete,
                           const QString& replaceText) {
      QString path = normalizePath(ipath);
      if (!QFile::exists(path)) {
            Debug("File <{}> ipath <{}> does not exist", path, ipath);
            return "Error: File does not exist.";
            }
      startLine -= 1; // index is zero based

      if (startLine < 0)
            return "Error: start_line out of bounds.";
      if (linesToDelete < 0)
            return "Error: lines_to_delete must be >= 0";

      int endLine = startLine + linesToDelete;

      Kontext* kontext = _editor->lookupKontext(path);
      File* f          = kontext->file();
      if (startLine > f->fileRows())
            startLine = f->fileRows(); // append if beyond end
      if (endLine > f->fileRows())
            endLine = f->fileRows();

      Pos start(0, startLine);
      Pos end(0, endLine);
      int charsToRemove = f->distance(start, end);
      QString insertStr = replaceText;
      if (!insertStr.isEmpty())
            insertStr += "\n";

      f->undo()->beginMacro();
      f->undo()->push(new Patch(f, start, charsToRemove, insertStr, Cursor(), Cursor()));
      f->undo()->endMacro();
      return std::format("success: replaced {} lines at line {} in {}.", endLine - startLine, startLine + 1,
                         path);
      }

//---------------------------------------------------------
// Tool: getGitStatus
//---------------------------------------------------------

string Agent::getGitStatus() {
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QProcess process;
      process.setWorkingDirectory(projRoot);
      process.start("git", QStringList() << "status");
      process.waitForFinished();

      string out(process.readAllStandardOutput());
      if (out.empty())
            return "No changes (working tree clean).";
      return out;
      }

//---------------------------------------------------------
// Tool: getGitDiff
//---------------------------------------------------------

string Agent::getGitDiff(const QString& path) {
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QProcess process;
      process.setWorkingDirectory(projRoot);

      QStringList args;
      args << "diff";
      if (!path.isEmpty())
            args << path;

      process.start("git", args);
      process.waitForFinished();

      string result(process.readAllStandardOutput());
      if (result.empty())
            return "There are no uncommitted changes.";

      // SICHERHEITS-LIMIT: Diffs können gigantisch werden!
      if (result.length() > kGitDiffMaxChars) {
            result = result.substr(result.length() - kGitDiffMaxChars);
            result +=
                "\n\n... [ATTENTION SYSTEM: Diff truncated because it was too large for the context window.]";
            }
      return result;
      }

//---------------------------------------------------------
// Tool: getGitLog
//---------------------------------------------------------

string Agent::getGitLog(int limit) {
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QProcess process;
      process.setWorkingDirectory(projRoot);

      // Nutzt das kompakte oneline Format, um Kontext zu sparen
      process.start("git", QStringList() << "log" << "-n" << QString::number(limit) << "--oneline");
      process.waitForFinished();

      string out(process.readAllStandardOutput());
      if (out.empty())
            return "Error reading git log or repository is empty.";
      return out;
      }

//---------------------------------------------------------
// Tool: createGitCommit
//---------------------------------------------------------

string Agent::createGitCommit(const QString& message) {
      // Wenn der Agent im Entwurfs-Modus ist, blockieren wir Schreibvorgänge
      if (!isExecuteMode())
            return std::format("Plan Mode Active: Commit '{}' was NOT executed. This is a read-only "
                               "simulation. No commit was created.",
                               message);

      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QProcess process;
      process.setWorkingDirectory(projRoot);

      // 1. Stage all changes
      //      process.start("git", QStringList() << "add" << ".");
      //      process.waitForFinished();

      // 2. Commit
      process.start("git", QStringList() << "commit" << "-a" << "-m" << message);
      process.waitForFinished();

      std::string err(process.readAllStandardError());
      std::string out(process.readAllStandardOutput());

      if (!err.empty() &&
          !QString::fromStdString(err).contains("master")) // Git nutzt stderr oft für Warnungen
            return std::format("Warning/Error during commit: {}", err);

      return std::format("Commit successful: {}", out);
      }

//---------------------------------------------------------
//   compressBuildLog
//---------------------------------------------------------

std::string Agent::compressBuildLog(const std::string& rawLog) {
      std::istringstream stream(rawLog);
      std::string line;

      std::vector<std::string> errors;
      std::vector<std::string> warnings;
      std::string currentBlock;
      bool inDiagnostic = false;

      QRegularExpression diagRegex("^(.+):(\\d+):(\\d+):\\s+(error|warning|fatal error):\\s+(.*)");
      QRegularExpression ninjaRegex("^\\[(\\d+)/(\\d+)\\]");

      int lastStep = 0, totalSteps = 0;
      QString projRoot = QDir::cleanPath(_editor->projectRoot()) + "/";

      while (std::getline(stream, line)) {
            QString qline = QString::fromStdString(line);
            qline.replace(projRoot, "");
            line = qline.toStdString();

            auto ninjaMatch = ninjaRegex.match(qline);
            if (ninjaMatch.hasMatch()) {
                  inDiagnostic = false;
                  lastStep     = ninjaMatch.captured(1).toInt();
                  totalSteps   = ninjaMatch.captured(2).toInt();
                  continue;
                  }

            auto diagMatch = diagRegex.match(qline);
            if (diagMatch.hasMatch()) {
                  if (!currentBlock.empty()) {
                        if (currentBlock.find("error:") != std::string::npos ||
                            currentBlock.find("fatal error:") != std::string::npos) {
                              errors.push_back(currentBlock);
                              }
                        else {
                              warnings.push_back(currentBlock);
                              }
                        }
                  inDiagnostic = true;
                  currentBlock = line + "\n";
                  }
            else if (inDiagnostic) {
                  if (line.starts_with(" ") || line.starts_with("\t") ||
                      line.find("^") != std::string::npos || line.find("|") != std::string::npos) {
                        currentBlock += line + "\n";
                        }
                  else {
                        if (!currentBlock.empty()) {
                              if (currentBlock.find("error:") != std::string::npos ||
                                  currentBlock.find("fatal error:") != std::string::npos) {
                                    errors.push_back(currentBlock);
                                    }
                              else {
                                    warnings.push_back(currentBlock);
                                    }
                              currentBlock.clear();
                              }
                        inDiagnostic = false;
                        }
                  }
            else if (line.find("undefined reference to") != std::string::npos ||
                     line.find("ld: error:") != std::string::npos) {
                  errors.push_back(line + "\n");
                  }
            }
      if (!currentBlock.empty()) {
            if (currentBlock.find("error:") != std::string::npos ||
                currentBlock.find("fatal error:") != std::string::npos) {
                  errors.push_back(currentBlock);
                  }
            else {
                  warnings.push_back(currentBlock);
                  }
            }

      std::string output;
      if (lastStep > 0 && totalSteps > 0)
            output += std::format("Build step: {}/{}\n\n", lastStep, totalSteps);

      if (!errors.empty()) {
            size_t displayCount = std::min(errors.size(), size_t(5));
            output += std::format("=== Errors (Showing first {} of {}) ===\n", displayCount, errors.size());
            for (size_t i = 0; i < displayCount; ++i)
                  output += errors[i] + "\n";
            }

      if (errors.empty() && !warnings.empty()) {
            size_t displayCount = std::min(warnings.size(), size_t(5));
            output +=
                std::format("=== Warnings (Showing first {} of {}) ===\n", displayCount, warnings.size());
            for (size_t i = 0; i < displayCount; ++i)
                  output += warnings[i] + "\n";
            }

      if (errors.empty() && warnings.empty()) {
            output += "Build output compressed. No standard format errors found.\n";
            if (rawLog.length() > 2000) {
                  output += "... [TRUNCATED] ...\n";
                  output += rawLog.substr(rawLog.length() - 2000);
                  }
            else {
                  output += rawLog;
                  }
            }

      return output;
      }

//---------------------------------------------------------
//   runBashCommand
//---------------------------------------------------------

string Agent::runBashCommand(const QString& command) {
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QString buildDir = projRoot + "/build";

      // 1. Prüfen und Erzeugen des "build" Verzeichnisses
      QDir dir(buildDir);
      if (!dir.exists()) {
            if (isExecuteMode()) {
                  if (!dir.mkpath(".")) { // Erzeugt das Verzeichnis inkl. übergeordneter Ordner falls nötig
                        Critical("failed creating <{}>", buildDir);
                        return std::format("Error: could not create build directory: {}", buildDir);
                        }
                  }
            else {
                  return "Plan Mode Active: Build directory does not exist and cannot be created in "
                         "read-only mode.";
                  }
            }

      // Evaluation of Sandboxing Technologies:
      // 1. Docker: Provides excellent isolation and native user switching (-u UID:GID). However,
      //    it requires a pre-configured image (e.g., with Qt6, CMake) which might not match the host.
      // 2. Bubblewrap (bwrap): A lightweight, rootless sandbox tool (used by Flatpak). It mounts
      //    the host system read-only (so all host compilers/libraries are available) while strictly
      //    restricting write access to the project root.
      //
      // Implementation: We use Bubblewrap (bwrap) for sandboxing as it perfectly balances host-tool
      // compatibility with strict write restrictions to the project directory. To run as the 'ai'
      // user, we prepend 'sudo -u ai' if the user exists.

      QString program = "bwrap";
      QStringList args;

      struct passwd* pw = getpwnam("ai");
      if (pw) {
            program = "sudo";
            args << "-u" << "ai" << "-n" << "bwrap";
            }

      args << "--ro-bind" << "/" << "/" << (isExecuteMode() ? "--bind" : "--ro-bind") << projRoot << projRoot
           << "--dev" << "/dev"
           << "--proc" << "/proc"
           << "--tmpfs" << "/tmp"
           << "--unshare-all"
           << "--share-net"
           //           << "--chdir" << buildDir
           << "--chdir" << projRoot << "/bin/sh" << "-c" << command;

      QProcess process;
      process.start(program, args);

      // Warten, bis der Build fertig ist (ohne Timeout, da Kompilieren dauern kann)
      process.waitForFinished(-1);

      // 5. Ergebnisse auslesen
      string stdOut(process.readAllStandardOutput());
      string stdErr(process.readAllStandardError());

      string result  = std::format("Command executed: {}\n", command);
      result        += std::format("Exit Code: {}\n\n", process.exitCode());

      if (!stdOut.empty())
            result += std::format("--- Standard Output ---\n{}\n", stdOut);
      if (!stdErr.empty())
            result += std::format("--- Standard Error ---\n{}\n", stdErr);

      // 6. SICHERHEITS-LIMIT: Compiler-Logs können riesig sein!
      // Wenn wir zu viel Text senden, stürzt Ollama wegen "Context Window Overflow" ab.
      if (result.length() > kBuildLogMaxChars) {
            // PRO-TIPP FÜR LLMs: Bei Compiler-Logs stehen die eigentlichen FEHLER (Error Summary)
            // fast immer GANZ UNTEN am Ende der Ausgabe. Wir schneiden also den unwichtigen
            // Anfang (z.B. "Scanning dependencies...") ab und behalten das Ende!

            string truncated  = "[... BEGINNING OF LOG TRUNCATED DUE TO LENGTH ...]\n\n";
            truncated        += result.substr(result.length() - kBuildLogMaxChars);
            return truncated;
            }

      return result;
      }

//--------------------------------------------------------------------
//   listFilesRecursive
//--------------------------------------------------------------------

string Agent::listFilesRecursive(const QString& ipath, int depth) {
      QString path = normalizePath(ipath);
      QDir dir(path);
      if (!dir.exists())
            return std::format("Error: The directory {} does not exist.", path);

      // Recursive lambda to build the tree
      std::function<json(const QDir&, int)> scanDir = [&](const QDir& currentDir, int level) -> json {
            json node        = json::object();
            node["name"]     = currentDir.dirName().toStdString();
            node["type"]     = "directory";
            node["children"] = json::array();

            QFileInfoList list = currentDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
            for (const QFileInfo& info : list) {
                  if (info.fileName().startsWith("."))
                        continue;
                  std::string fn = info.fileName().toStdString();
                  json fileNode;
                  fileNode["name"] = fn;
                  fileNode["size"] = info.size();
                  if (info.isDir()) {
                        QDir subDir(info.absoluteFilePath());
                        if (level >= depth) {
                              fileNode["type"] = "directory";
                              node["children"].push_back(fileNode);
                              }
                        else
                              node["children"].push_back(scanDir(subDir, ++level));
                        }
                  else {
                        fileNode["type"] = "file";
                        node["children"].push_back(fileNode);
                        }
                  }
            return node;
            };

      json result        = scanDir(dir, 0);
      result["rootPath"] = path.toStdString();
      return result.dump(2);
      }

//---------------------------------------------------------
//   getFileOutline
//---------------------------------------------------------

string Agent::getFileOutline(const QString& file) {
      QString normPath = normalizePath(file);
      Kontext* k       = _editor->lookupKontext(normPath);
      if (!k)
            return "Error: File not found, so no diagnostics are available.";
      File* f = k->file();

      QEventLoop loop;

      auto connection = connect(f, &File::symbolsReady, [&]() { loop.quit(); });

      QTimer timer;
      timer.setSingleShot(true);

      string result = f->symbols();
      connect(&timer, &QTimer::timeout, [&]() {
            result = "Error: Timeout (5s) while waiting for Language Server symbol outline result.";
            loop.quit();
            });
      if (result.empty()) {
            timer.start(5000); // 5 seconds
            loop.exec();
            }
      disconnect(connection);

      return result;
      }

//---------------------------------------------------------
//   compressValgrindOutput
//---------------------------------------------------------

std::string Agent::compressValgrindOutput(const QString& xmlPath) {
      QFile file(xmlPath);
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return "Error: Could not read Valgrind XML output.";
      QXmlStreamReader xml(&file);
      json outputArray = json::array();
      json currentError;
      json currentStackFrames = json::array();
      bool inError            = false;
      bool inStack            = false;
      bool inFrame            = false;
      QString currentText;
      QString kind;
      QString what;
      QString obj, fileStr, line, fn;

      while (!xml.atEnd() && !xml.hasError()) {
            QXmlStreamReader::TokenType token = xml.readNext();
            if (token == QXmlStreamReader::StartElement) {
                  QString name = xml.name().toString();
                  if (name == "error") {
                        inError            = true;
                        currentError       = json::object();
                        currentStackFrames = json::array();
                        kind               = "";
                        what               = "";
                        }
                  else if (inError && name == "stack") {
                        inStack = true;
                        }
                  else if (inStack && name == "frame") {
                        inFrame = true;
                        obj     = "";
                        fileStr = "";
                        line    = "";
                        fn      = "";
                        }
                  currentText = "";
                  }
            else if (token == QXmlStreamReader::Characters) {
                  currentText += xml.text().toString().trimmed();
                  }
            else if (token == QXmlStreamReader::EndElement) {
                  QString name = xml.name().toString();
                  if (name == "error") {
                        inError = false;
                        if (!kind.isEmpty())
                              currentError["kind"] = kind.toStdString();
                        if (!what.isEmpty())
                              currentError["what"] = what.toStdString();
                        if (!currentStackFrames.empty())
                              currentError["stack"] = currentStackFrames;
                        outputArray.push_back(currentError);
                        }
                  else if (inError && name == "kind") {
                        kind = currentText;
                        }
                  else if (inError && name == "what") {
                        what = currentText;
                        }
                  else if (inStack && name == "stack") {
                        inStack = false;
                        }
                  else if (inFrame && name == "frame") {
                        inFrame = false;
                        if (!obj.contains("/usr/") && !obj.contains("/lib/")) {
                              json frameJson;
                              if (!fileStr.isEmpty())
                                    frameJson["file"] = (fileStr + ":" + line).toStdString();
                              if (!fn.isEmpty())
                                    frameJson["func"] = fn.toStdString();
                              currentStackFrames.push_back(frameJson);
                              }
                        }
                  else if (inFrame && name == "obj") {
                        obj = currentText;
                        }
                  else if (inFrame && name == "file") {
                        fileStr = currentText;
                        }
                  else if (inFrame && name == "line") {
                        line = currentText;
                        }
                  else if (inFrame && name == "fn") {
                        fn = currentText;
                        }
                  }
            }

      file.close();
      if (xml.hasError())
            return "Error parsing Valgrind XML: " + xml.errorString().toStdString();

      if (outputArray.empty())
            return "Success: Valgrind did not report any errors.";

      return outputArray.dump(2);
      }

//---------------------------------------------------------
//   runValgrindCommand
//---------------------------------------------------------

std::string Agent::runValgrindCommand(const QString& executable, const QString& tool, const QString& args) {
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QString xmlFile  = projRoot + "/build/valgrind_out.xml";

      QString cmd = "valgrind --tool=" + tool + " --xml=yes --xml-file=" + xmlFile + " ";
      if (tool == "memcheck")
            cmd += "--leak-check=full --show-leak-kinds=definite ";
      cmd += executable + " " + args;

      QProcess process;
      process.setWorkingDirectory(projRoot);
      process.start("sh", QStringList() << "-c" << cmd);
      process.waitForFinished(-1);

      return compressValgrindOutput(xmlFile);
      }

//---------------------------------------------------------
// Tool: formatSource
//---------------------------------------------------------

string Agent::formatSource(const QString& ipath) {
      QString normPath = normalizePath(ipath);
      Kontext* kontext = _editor->lookupKontext(normPath);
      if (!kontext) {
            kontext = _editor->addFile(normPath);
            if (!kontext)
                  return "Error: Could not load file into editor.";
            }

      LSclient* client = kontext->file()->languageClient();
      if (!client)
            return "Error: No Language Server available for this file.";
      if (!client->initialized())
            return "Error: Language Server not ready, try again later";

      QEventLoop loop;
      auto connection = connect(client, &LSclient::formatCompleted, [&]() { loop.quit(); });

      QTimer timer;
      timer.setSingleShot(true);
      connect(&timer, &QTimer::timeout, [&]() { loop.quit(); });

      client->formattingRequest(kontext);
      timer.start(5000); // 5 seconds
      loop.exec();
      disconnect(connection);

      return std::format("Success: File {} successfully formatted.", normPath.toStdString());
      }
