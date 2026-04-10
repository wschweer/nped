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
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDirIterator>
#include <QPlainTextEdit>
#include <list>
#include <QEventLoop>
#include <QTimer>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "editor.h"
#include "agent.h"
#include "logger.h"
#include "undo.h"
#include "kontext.h"
#include "file.h"
#include "lsclient.h"

//---------------------------------------------------------
//   getMCPTools
//    return list of available tools
//   rw create_file                 createFile(path,content)
//   rw modify_file                 modifyFile(path,content)
//   rw replace_in_file             replaceInFile(path, search, replace)
//   ro list_directory              listDirectory(path)
//   ro search_project              searchProject(query, pattern="")
//   ro read_file                   readFile(path, start, end)
//   ro find_symbol                 findSymbol(name)
//   ro fetch_web_documentation     fetchWebDocumentation(url)
//   rw run_build_command           runBuildCommand(cmd)
//   ro get_git_status              getGitStatus()
//   ro get_git_diff                getGitDiff(path="")
//   ro get_git_log                 getGitLog(limit=5)
//   rw create_git_commit           createGitCommit(msg)
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
            if (isExecuteMode()) {
                  tools.push_back(MCPToolBuilder("read_file", "Reads a file or a specific range of lines from a file.")
                                      .add_parameter("path", "string", "The path to the file.")
                                      .add_parameter("start_line", "integer", "The first line to read (1-indexed).", false)
                                      .add_parameter("end_line", "integer", "The last line to read.", false)
                                      .build());

                  tools.push_back(
                      MCPToolBuilder("write_file",
                                     "Completely overwrites an existing file with new content or creates a new file with content")
                          .add_parameter("path", "string", "The path to the file.")
                          .add_parameter("content", "string", "The content for the file.")
                          .build());

                  tools.push_back(MCPToolBuilder("replace_in_file", "Replaces a specific text block with new content. Use this for "
                                                                    "targeted edits to avoid overwriting the whole file.")
                                      .add_parameter("path", "string", "The path to the file.")
                                      .add_parameter("search", "string", "The exact text string to be replaced.")
                                      .add_parameter("replace", "string", "The new text to insert.")
                                      .build());
                  }

            // 2. Navigation & Search
            tools.push_back(MCPToolBuilder("list_directory", "Lists all files and subdirectories within a given path.")
                                .add_parameter("path", "string", "The directory path to inspect.")
                                .build());

            tools.push_back(MCPToolBuilder("search_project",
                                           "Searches for a text query across all files in the project, including unsaved editor buffers.")
                                .add_parameter("query", "string", "The text to search for.")
                                .add_parameter("file_pattern", "string", "Optional glob pattern to filter files (e.g., '*.cpp').", false)
                                .build());

            tools.push_back(
                MCPToolBuilder("find_symbol", "Uses the Language Server (LSP) to find the definition of a symbol like a class or function.")
                    .add_parameter("symbol", "string", "The name of the symbol to locate (e.g., 'MyClass').")
                    .build());

            // 3. System & External
            tools.push_back(
                MCPToolBuilder("fetch_web_documentation",
                               "Downloads content from a URL via HTTP GET. Use this to read external API docs or technical references.")
                    .add_parameter("url", "string", "The full URL starting with http or https.")
                    .build());

            tools.push_back(MCPToolBuilder("run_build_command",
                                           "Executes a shell command (like 'make' or 'cmake') within the project's build directory.")
                                .add_parameter("command", "string", "The build command to execute.")
                                .build());

            // 4. Git Integration
            tools.push_back(
                MCPToolBuilder("get_git_status", "Returns the current git status of the repository. No parameters needed.").build());

            tools.push_back(MCPToolBuilder("get_git_diff", "Shows uncommitted changes as a diff. Helps review edits before committing.")
                                .add_parameter("path", "string", "Optional: path to a specific file to diff.", false)
                                .build());

            tools.push_back(MCPToolBuilder("get_git_log", "Displays the recent git commit history.")
                                .add_parameter("limit", "integer", "Number of commits to retrieve (default: 5).", false)
                                .build());

            if (isExecuteMode()) {
                  tools.push_back(MCPToolBuilder("create_git_commit", "Stages all current changes (git add .) and creates a new commit.")
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
      Debug("<{}>", functionName);

      // Hilfsfunktion: Limitiert die Zeilenlänge des Feedbacks im chatDisplay
#if 0
      auto trim = [](const QString& s, int maxLen = 80) {
            QString res = s.length() > maxLen ? s.left(maxLen - 3) + "..." : s;
            return res.toHtmlEscaped(); // Gleichzeitig HTML Sonderzeichen maskieren
                                                                                    };
#endif
      // 1. Definiere, welche Tools harmlos sind (Nur-Lese-Zugriff)
      bool isReadOnlyTool =
          (functionName == "read_file" || functionName == "search_project" || functionName == "find_symbol" ||
           functionName == "list_directory" || functionName == "fetch_web_documentation" || functionName == "get_git_status" ||
           functionName == "get_git_diff" || functionName == "get_git_log" || functionName == "run_build_command");

      // 2. Entwurfs-Modus Check
      if (!isExecuteMode() && !isReadOnlyTool) {
            // Schreib-Tools simulieren, um den Denkprozess der KI nicht zu unterbrechen
            return "Plan Mode Active: The tool '" + functionName +
                   "' was NOT executed. This is a read-only simulation. Changes were NOT saved to disk.";
            }

      // ==========================================================
      // Tools OHNE lokales Datei-Pfad Argument
      // ==========================================================

      if (functionName == "run_build_command") {
            if (!arguments.contains("command"))
                  return "Error: Parameter 'command' missing.";
            QString cmd = QString::fromStdString(arguments["command"].get<std::string>());
            return runBuildCommand(cmd);
            }
      else if (functionName == "fetch_web_documentation") {
            if (!arguments.contains("url"))
                  return "Error: Parameter 'url' missing.";
            QString url = QString::fromStdString(arguments["url"].get<std::string>());
            return fetchWebDocumentation(url);
            }
      else if (functionName == "get_git_status") {
            return getGitStatus();
            }
      else if (functionName == "get_git_diff") {
            QString p = arguments.contains("path") ? QString::fromStdString(arguments["path"].get<std::string>()) : "";
            return getGitDiff(p);
            }
      else if (functionName == "get_git_log") {
            int limit = arguments.contains("limit") ? arguments["limit"].get<int>() : 5;
            return getGitLog(limit);
            }
      else if (functionName == "create_git_commit") {
            if (!arguments.contains("message"))
                  return "Error: Parameter 'message' missing.";
            QString msg = QString::fromStdString(arguments["message"].get<std::string>());
            return createGitCommit(msg);
            }

      //==========================================================
      // Tools MIT lokalem Datei-Pfad Argument
      //==========================================================

      else if (functionName == "search_project") {
            if (!arguments.contains("query"))
                  return "Error: Parameter 'query' missing.";
            QString query = QString::fromStdString(arguments["query"].get<std::string>());
            QString pattern =
                arguments.contains("file_pattern") ? QString::fromStdString(arguments["file_pattern"].get<std::string>()) : "";
            return searchProject(query, pattern);
            }
      else if (functionName == "find_symbol") {
            if (!arguments.contains("symbol"))
                  return "Error: Parameter 'symbol' missing.";
            QString symbol = QString::fromStdString(arguments["symbol"].get<std::string>());
            return findSymbol(symbol);
            }
      if (!arguments.contains("path"))
            return "Error: Parameter 'path' missing for local file tool: " + functionName;

      QString path = QString::fromStdString(arguments["path"].get<std::string>());

      // WICHTIG: Pfad-Sicherheitscheck (Sandboxing)
      if (!isPathSafe(path)) {
            Debug("Sicherheitssperre gegriffen für Pfad: {}", path.toStdString());
            return "Security Lock: Access denied! Path is outside of " + _editor->projectRoot().toStdString();
            }

      // Lokale Datei-Operationen ausführen
      if (functionName == "read_file") {
            QString content;
            if (!readFile(path, content))
                  return errorResponse(content.toStdString());
            QStringList lines = content.split('\n');

            // [startLine, endLine] is a closed interval
            // we must transform it to a half closed interval: [startLine, endLine)

            int startLine = arguments.contains("start_line") ? arguments["start_line"].get<int>() - 1 : 0;
            int endLine   = arguments.contains("end_line") ? arguments["end_line"].get<int>() : lines.size();
            int n         = lines.size();

            if (startLine < 0 || startLine >= n)
                  return errorResponse("start_line is outside of valid range");
            if (endLine < 0 || endLine > n)
                  return errorResponse("end_line is outside of valid range");
            startLine     = std::clamp(startLine, 0, n);
            endLine       = std::clamp(endLine, 0, n);

            std::vector<std::string> extractedLines;
            for (int i = startLine; i < endLine; ++i)
                  extractedLines.push_back(lines[i].toStdString());
            json result = {
                     {    "status",      "success"},
                     { "startLine",  startLine + 1},
                     {   "endLine",    endLine + 1},
                     {     "lines", extractedLines},
                     {"totalLines",   lines.size()}
                  };
            return result.dump();
            }

      else if (functionName == "write_file") {
            if (!arguments.contains("content"))
                  return "Error: Parameter 'content' missing.";
            QString content = QString::fromStdString(arguments["content"].get<std::string>());
            return writeFile(path, content);
            }
      else if (functionName == "list_directory") {
            return listDirectory(path);
            }
      else if (functionName == "replace_in_file") {
            if (!arguments.contains("search") || !arguments.contains("replace"))
                  return "Error: Parameters 'search' or 'replace' missing.";
            QString search  = QString::fromStdString(arguments["search"].get<std::string>());
            QString replace = QString::fromStdString(arguments["replace"].get<std::string>());
            return replaceInFile(path, search, replace);
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

//---------------------------------------------------------
// Tool 4: list_directory
//---------------------------------------------------------

string Agent::listDirectory(const QString& ipath) {
      auto path = normalizePath(ipath);

      QDir dir(path);
      if (!dir.exists())
            return "Error: The directory does not exist on disk.";

      string result = std::format("Contents of directory {}:\n", path);

      // Wir holen alle Dateien und Ordner, filtern aber "." und ".." aus
      QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);

      if (list.isEmpty())
            return result + "(Directory is empty)";

      for (const QFileInfo& fileInfo : list) {
            if (fileInfo.fileName().startsWith("."))
                  continue;
            if (fileInfo.isDir())
                  result += std::format("[DIR]    {}\n", fileInfo.fileName());
            else
                  result += std::format("[FILE]   {} ({} bytes)\n", fileInfo.fileName(), fileInfo.size());
            }
      return result;
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
            content  = content.substr(content.length() - kWebFetchMaxChars);
            content += "\n\n... [ATTENTION SYSTEM: Documentation was truncated here because it was too large.]";
            }

      return content;
      }

//---------------------------------------------------------
//   replaceInFile
//---------------------------------------------------------

string Agent::replaceInFile(const QString& ipath, const QString& searchStr, const QString& replaceStr) {
      static constexpr const char notFound[] = "Error: The searched text (search) was not found in the file. "
                                               "Make sure that indentations, spaces, and line breaks match exactly.";
      QString path = normalizePath(ipath);
      if (!QFile::exists(path)) {
            Debug("File <{}> ipath <{}> does not exist", path, ipath);
            return "Error: File does not exist.";
            }

      File* f = _editor->findFile(path);
      if (f) {
            //            Debug("local search/replace in <{}>: <{}> -- <{}>", path, searchStr, replaceStr);
            f->undo()->beginMacro();
            auto rv = f->searchReplace(searchStr, replaceStr);
            f->undo()->endMacro();
            if (!rv)
                  return notFound;
            }
      else {
            QFile file(path);
            // 1. Datei komplett in den Speicher lesen
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                  return std::format("Error: Could not open file for reading: {}", file.errorString());
            QString content = QTextStream(&file).readAll();
            file.close();

            // 2. Prüfen, ob der Suchstring überhaupt existiert
            if (!content.contains(searchStr)) {
                  Debug("not found: <{}>", searchStr);
                  // WICHTIG FÜR DIE KI: Ein klares Feedback, warum es gescheitert ist.
                  return notFound;
                  }

            // 3. Den Text austauschen (ersetzt alle Vorkommen)
            content.replace(searchStr, replaceStr);

            // 4. Datei überschreiben
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
                  return std::format("Error: Could not open file for writing: {}", file.errorString());
            QTextStream out(&file);
            out << content;
            file.close();
            }
      return std::format("success: text in {} replaced.", path);
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
            result  = result.substr(result.length() - kGitDiffMaxChars);
            result += "\n\n... [ATTENTION SYSTEM: Diff truncated because it was too large for the context window.]";
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
            return std::format("Plan Mode Active: Commit '{}' was NOT executed. This is a read-only simulation. No commit was created.",
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

      QByteArray err = process.readAllStandardError();
      QByteArray out = process.readAllStandardOutput();

      if (!err.isEmpty() && !QString::fromUtf8(err).contains("master")) // Git nutzt stderr oft für Warnungen
            return std::format("Warning/Error during commit: {}", err);

      return std::format("Commit successful: {}", out);
      }

//---------------------------------------------------------
//   runBuildCommand
//---------------------------------------------------------

string Agent::runBuildCommand(const QString& command) {
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QString buildDir = projRoot + "/build";

      if (isExecuteMode()) {
            for (auto f : _editor->getFiles()) {
                  f->undo()->beginMacro();
                  f->save();
                  f->undo()->endMacro();
                  }
            }

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
                  return "Plan Mode Active: Build directory does not exist and cannot be created in read-only mode.";
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

      args << "--ro-bind" << "/" << "/" << (isExecuteMode() ? "--bind" : "--ro-bind") << projRoot << projRoot << "--dev" << "/dev"
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
      QByteArray stdOut = process.readAllStandardOutput();
      QByteArray stdErr = process.readAllStandardError();

      string result  = std::format("Command executed: {}\n", command);
      result        += std::format("Exit Code: {}\n\n", process.exitCode());

      if (!stdOut.isEmpty())
            result += std::format("--- Standard Output ---\n{}\n", stdOut);
      if (!stdErr.isEmpty())
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
