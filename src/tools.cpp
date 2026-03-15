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

#include "editor.h"
#include "agent.h"
#include "logger.h"
#include "undo.h"
#include "kontext.h"
#include "file.h"
#include "lsclient.h"
#include "webview.h"

//---------------------------------------------------------
//   getMCPTools
//    return list of available tools
//   ro read_file                   readFile(path)
//   rw create_file                 createFile(path,content)
//   rw modify_file                 modifiyFile(path,content)
//   ro list_directory              listDirectory(path)
//   ro search_project              searchProject(query, pattern="")
//   ro read_file_lines             readFileLines(path, start, end)
//   ro find_symbol                 findSymbol(name)
//   ro fetch_web_documentation     fetchWebDocumentation(url)
//   rw run_build_command           runBuildCommand(cmd)
//   rw replace_in_file             replaceInFile(path, search, replace)
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
            tools.push_back(MCPToolBuilder("read_file", "Reads the full content of a file from the local file system.")
                                .add_parameter("path", "string", "The absolute or relative path to the file.")
                                .build());

            tools.push_back(MCPToolBuilder("create_file", "Creates a new file at the specified path with the provided content.")
                                .add_parameter("path", "string", "The path where the file should be created.")
                                .add_parameter("content", "string", "The initial content of the file.")
                                .build());

            tools.push_back(MCPToolBuilder("modify_file", "Completely overwrites an existing file with new content.")
                                .add_parameter("path", "string", "The path to the file to be modified.")
                                .add_parameter("content", "string", "The new content for the file.")
                                .build());

            tools.push_back(
                MCPToolBuilder(
                    "replace_in_file",
                    "Replaces a specific text block with new content. Use this for targeted edits to avoid overwriting the whole file.")
                    .add_parameter("path", "string", "The path to the file.")
                    .add_parameter("search", "string", "The exact text string to be replaced.")
                    .add_parameter("replace", "string", "The new text to insert.")
                    .build());

            tools.push_back(MCPToolBuilder("read_file_lines", "Reads a specific range of lines from a file (useful for large files).")
                                .add_parameter("path", "string", "The path to the file.")
                                .add_parameter("start_line", "integer", "The first line to read (1-indexed).")
                                .add_parameter("end_line", "integer", "The last line to read.")
                                .build());

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

            tools.push_back(MCPToolBuilder("create_git_commit", "Stages all current changes (git add .) and creates a new commit.")
                                .add_parameter("message", "string", "A clear and concise commit message.")
                                .build());
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
          (functionName == "read_file" || functionName == "read_file_lines" || functionName == "search_project" ||
           functionName == "find_symbol" || functionName == "list_directory" || functionName == "fetch_web_documentation" ||
           functionName == "get_git_status" || functionName == "get_git_diff" || functionName == "get_git_log");

      // 2. Entwurfs-Modus Check
      if (!isExecuteMode && !isReadOnlyTool) {
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
            return runBuildCommand(cmd).toStdString();
            }
      else if (functionName == "fetch_web_documentation") {
            if (!arguments.contains("url"))
                  return "Error: Parameter 'url' missing.";
            QString url = QString::fromStdString(arguments["url"].get<std::string>());
            return fetchWebDocumentation(url).toStdString();
            }
      else if (functionName == "get_git_status") {
            return getGitStatus().toStdString();
            }
      else if (functionName == "get_git_diff") {
            QString p = arguments.contains("path") ? QString::fromStdString(arguments["path"].get<std::string>()) : "";
            return getGitDiff(p).toStdString();
            }
      else if (functionName == "get_git_log") {
            int limit = arguments.contains("limit") ? arguments["limit"].get<int>() : 5;
            return getGitLog(limit).toStdString();
            }
      else if (functionName == "create_git_commit") {
            if (!arguments.contains("message"))
                  return "Error: Parameter 'message' missing.";
            QString msg = QString::fromStdString(arguments["message"].get<std::string>());
            return createGitCommit(msg).toStdString();
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
            return searchProject(query, pattern).toStdString();
            }
      else if (functionName == "find_symbol") {
            if (!arguments.contains("symbol"))
                  return "Error: Parameter 'symbol' missing.";
            QString symbol = QString::fromStdString(arguments["symbol"].get<std::string>());
            return findSymbol(symbol).toStdString();
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
            QString result;
            readFile(path, result);
            return result.toStdString();
            }
      else if (functionName == "read_file_lines") {
            if (!arguments.contains("start_line") || !arguments.contains("end_line"))
                  return "Error: Parameters 'start_line' or 'end_line' missing.";
            int startLine = arguments["start_line"].get<int>();
            int endLine   = arguments["end_line"].get<int>();
            return readFileLines(path, startLine, endLine).toStdString();
            }
      else if (functionName == "list_directory") {
            return listDirectory(path).toStdString();
            }
      else if (functionName == "create_file") {
            if (!arguments.contains("content"))
                  return "Error: Parameter 'content' missing.";
            QString content = QString::fromStdString(arguments["content"].get<std::string>());
            return createFile(path, content).toStdString();
            }
      else if (functionName == "modify_file") {
            if (!arguments.contains("content"))
                  return "Error: Parameter 'content' missing.";
            QString content = QString::fromStdString(arguments["content"].get<std::string>());
            return modifyFile(path, content).toStdString();
            }
      else if (functionName == "replace_in_file") {
            if (!arguments.contains("search") || !arguments.contains("replace"))
                  return "Error: Parameters 'search' or 'replace' missing.";
            QString search  = QString::fromStdString(arguments["search"].get<std::string>());
            QString replace = QString::fromStdString(arguments["replace"].get<std::string>());
            return replaceInFile(path, search, replace).toStdString();
            }
      return "Error: Unknown tool (" + functionName + ").";
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
//   readFile
//    on error returns false
//---------------------------------------------------------

bool Agent::readFile(const QString& ipath, QString& result) {
      QString path;
      if (!ipath.startsWith("/"))
            path = _editor->projectRoot() + "/" + ipath;
      else
            path = ipath;
      File* f = _editor->findFile(path);
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
//   readFileLines
//---------------------------------------------------------

QString Agent::readFileLines(const QString& path, int startLine, int endLine) {
      QString content;
      if (!readFile(path, content))
            return content;
      QStringList lines = content.split('\n');
      QStringList result;
      for (int i = startLine - 1; i < endLine && i < lines.size(); ++i)
            if (i >= 0 && i < lines.size())
                  result.append(QString::number(i + 1) + ": " + lines[i]);
      return result.join('\n');
      }

//---------------------------------------------------------
//   searchProject
//---------------------------------------------------------

QString Agent::searchProject(const QString& query, const QString& filePattern) {
      Debug("<{}> <{}>", query, filePattern);
      QString result;
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QDir dir(projRoot);
      QStringList filters;
      if (!filePattern.isEmpty())
            filters << filePattern;
      else
            filters << "*.cpp" << "*.h" << "*.c" << "*.hpp" << "CMakeLists.txt";
      QDirIterator it(projRoot, filters, QDir::Files, QDirIterator::Subdirectories);
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
                        result += relativePath + ":" + QString::number(i + 1) + ": " + lines[i].trimmed() + "\n";
            }
      if (result.isEmpty())
            return "No matches found for '" + query + "'.";
      if (result.length() > 4000) {
            result.truncate(4000);
            result += "\n... [Too many results, output truncated]";
            }
      Debug("result <{}>", result);
      return result;
      }

//---------------------------------------------------------
//   findSymbol
//---------------------------------------------------------

QString Agent::findSymbol(const QString& symbol) {
      LSclient* client = _editor->getLSclient("clangd");
      if (!client)
            return "Error: No Language Server available.";
      if (!client->initialized())
            return "Error: Language Server not ready, try again later";

      QEventLoop loop;
      QString result;

      auto connection = connect(client, &LSclient::symbolSearchResult, [&](const QString& res) {
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
//   createFile
//---------------------------------------------------------

QString Agent::createFile(const QString& path, const QString& content) {
      if (QFile::exists(path))
            return "Error: File already exists. Use modify_file.";

      QFile file(path);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return "Error creating file (" + file.errorString() + ")";
      QTextStream out(&file);
      out << content;
      _editor->addFile(path);
      return "Success: File " + path + " successfully created.";
      }

//---------------------------------------------------------
//   modifyFile
//---------------------------------------------------------

QString Agent::modifyFile(const QString& path, const QString& content) {
      if (!QFile::exists(path))
            return "Error: File does not exist. Use create_file.";

      QFile file(path);
      // QIODevice::Truncate löscht den alten Inhalt der Datei
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            return "Error modifying file (" + file.errorString() + ")";
      QTextStream out(&file);
      out << content;
      return "Success: File " + path + " successfully modified.";
      }

//---------------------------------------------------------
// Tool 4: list_directory
//---------------------------------------------------------

QString Agent::listDirectory(const QString& path) {
      QDir dir(path);
      if (!dir.exists())
            return "Error: The directory does not exist on disk.";

      QString result = "Contents of directory " + path + ":\n";

      // Wir holen alle Dateien und Ordner, filtern aber "." und ".." aus
      QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);

      if (list.isEmpty())
            return result + "(Directory is empty)";

      for (const QFileInfo& fileInfo : list)
            if (fileInfo.isDir())
                  result += "[DIR]    " + fileInfo.fileName() + "\n";
            else
                  result += "[FILE]   " + fileInfo.fileName() + " (" + QString::number(fileInfo.size()) + " bytes)\n";
      return result;
      }

// ---------------------------------------------------------
// Tool 5: fetch_web_documentation
// ---------------------------------------------------------

QString Agent::fetchWebDocumentation(const QString& urlString) {
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
            return "Network error while fetching URL: " + errorStr;
            }

      QString content = reply->readAll();
      reply->deleteLater();

      // SICHERHEITS-LIMIT: Webseiten (HTML) können absurd groß sein.
      // Wenn wir 2 Megabyte an HTML-Code an das LLM zurückschicken,
      // sprengt das sofort das "Context Window" von Ollama und es stürzt ab.
      // Wir kappen die Antwort sicherheitshalber bei 8000 Zeichen.
      if (content.length() > kWebFetchMaxChars) {
            content.truncate(kWebFetchMaxChars);
            content += "\n\n... [ATTENTION SYSTEM: Documentation was truncated here because it was too large.]";
            }

      return content;
      }

//---------------------------------------------------------
//   replaceInFile
//---------------------------------------------------------

QString Agent::replaceInFile(const QString& ipath, const QString& searchStr, const QString& replaceStr) {
      static constexpr const char notFound[] = "Error: The searched text (search) was not found in the file. "
                                               "Make sure that indentations, spaces, and line breaks match exactly.";
      if (!QFile::exists(ipath))
            return "Error: File does not exist. Use create_file instead.";

      QString path;
      if (!ipath.startsWith("/"))
            path = _editor->projectRoot() + "/" + ipath;
      else
            path = ipath;

      File* f = _editor->findFile(path);
      if (f) {
            Debug("local search/replace in <{}>: <{}> -- <{}>", path, searchStr, replaceStr);
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
                  return "Error: Could not open file for reading: " + file.errorString();
            QString content = QTextStream(&file).readAll();
            file.close();

            // 2. Prüfen, ob der Suchstring überhaupt existiert
            if (!content.contains(searchStr)) {
                  // WICHTIG FÜR DIE KI: Ein klares Feedback, warum es gescheitert ist.
                  return notFound;
                  }

            // 3. Den Text austauschen (ersetzt alle Vorkommen)
            content.replace(searchStr, replaceStr);

            // 4. Datei überschreiben
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
                  return "Error: Could not open file for writing: " + file.errorString();
            QTextStream out(&file);
            out << content;
            file.close();
            }
      return "success: text in " + path + " replaced.";
      }

//---------------------------------------------------------
// Tool: getGitStatus
//---------------------------------------------------------

QString Agent::getGitStatus() {
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QProcess process;
      process.setWorkingDirectory(projRoot);
      process.start("git", QStringList() << "status");
      process.waitForFinished();

      QString out = QString::fromUtf8(process.readAllStandardOutput());
      if (out.isEmpty())
            return "No changes (working tree clean).";
      return out;
      }

//---------------------------------------------------------
// Tool: getGitDiff
//---------------------------------------------------------

QString Agent::getGitDiff(const QString& path) {
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QProcess process;
      process.setWorkingDirectory(projRoot);

      QStringList args;
      args << "diff";
      if (!path.isEmpty())
            args << path;

      process.start("git", args);
      process.waitForFinished();

      QString result = QString::fromUtf8(process.readAllStandardOutput());
      if (result.isEmpty())
            return "There are no uncommitted changes.";

      // SICHERHEITS-LIMIT: Diffs können gigantisch werden!
      if (result.length() > kGitDiffMaxChars) {
            result.truncate(kGitDiffMaxChars);
            result += "\n\n... [ATTENTION SYSTEM: Diff truncated because it was too large for the context window.]";
            }
      return result;
      }

//---------------------------------------------------------
// Tool: getGitLog
//---------------------------------------------------------

QString Agent::getGitLog(int limit) {
      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QProcess process;
      process.setWorkingDirectory(projRoot);

      // Nutzt das kompakte oneline Format, um Kontext zu sparen
      process.start("git", QStringList() << "log" << "-n" << QString::number(limit) << "--oneline");
      process.waitForFinished();

      QString out = QString::fromUtf8(process.readAllStandardOutput());
      if (out.isEmpty())
            return "Error reading git log or repository is empty.";
      return out;
      }

//---------------------------------------------------------
// Tool: createGitCommit
//---------------------------------------------------------

QString Agent::createGitCommit(const QString& message) {
      // Wenn der Agent im Entwurfs-Modus ist, blockieren wir Schreibvorgänge
      if (!isExecuteMode)
            return "Plan Mode Active: Commit '" + message + "' was NOT executed. This is a read-only simulation. No commit was created.";

      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QProcess process;
      process.setWorkingDirectory(projRoot);

      // 1. Stage all changes
      process.start("git", QStringList() << "add" << ".");
      process.waitForFinished();

      // 2. Commit
      process.start("git", QStringList() << "commit" << "-m" << message);
      process.waitForFinished();

      QByteArray err = process.readAllStandardError();
      QByteArray out = process.readAllStandardOutput();

      if (!err.isEmpty() && !QString::fromUtf8(err).contains("master")) // Git nutzt stderr oft für Warnungen
            return "Warning/Error during commit: " + QString::fromUtf8(err);

      return "Commit successful: " + QString::fromUtf8(out);
      }

//---------------------------------------------------------
//   runBuildCommand
//---------------------------------------------------------

QString Agent::runBuildCommand(const QString& command) {
      QStringList blacklist = {"cat ", "grep ", "ls ", "find ", "awk ", "sed ", "head ", "tail "};
      for (const QString& blocked : blacklist) {
            if (command.startsWith(blocked.trimmed()) || command.contains(" " + blocked)) {
                  return "Error: Shell commands for file reading/searching are blocked to prevent buffer inconsistency. Please use "
                         "the provided AI tools (search_project, read_file_lines, find_symbol) instead.";
                  }
            }

      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QString buildDir = projRoot + "/build";

      for (auto f : _editor->getFiles()) {
            f->undo()->beginMacro();
            f->save();
            f->undo()->endMacro();
            }

      // 1. Prüfen und Erzeugen des "build" Verzeichnisses
      QDir dir(buildDir);
      if (!dir.exists()) {
            Debug("create build folder");
            if (!dir.mkpath(".")) { // Erzeugt das Verzeichnis inkl. übergeordneter Ordner falls nötig
                  Critical("failed creating <{}>", buildDir);
                  return "Error: could not create build directory: " + buildDir;
                  }
            }

      // 2. Den Prozess vorbereiten
      QProcess process;
      process.setWorkingDirectory(buildDir); // Zwingt den Prozess ins build-Verzeichnis

      // 3. Systemübergreifender Aufruf über die Kommandozeile
      // Damit Befehle mit Leerzeichen ("cmake --build .") korrekt verarbeitet werden,
      // reichen wir sie an die System-Shell weiter.
      process.start("/bin/sh", QStringList() << "-c" << command);

      // 4. Warten, bis der Build fertig ist (ohne Timeout, da Kompilieren dauern kann)
      process.waitForFinished(-1);

      // 5. Ergebnisse auslesen
      QByteArray stdOut = process.readAllStandardOutput();
      QByteArray stdErr = process.readAllStandardError();

      QString result  = "Command executed: " + command + "\n";
      result         += "Exit Code: " + QString::number(process.exitCode()) + "\n\n";

      if (!stdOut.isEmpty())
            result += "--- Standard Output ---\n" + QString::fromUtf8(stdOut) + "\n";
      if (!stdErr.isEmpty())
            result += "--- Standard Error ---\n" + QString::fromUtf8(stdErr) + "\n";

      // 6. SICHERHEITS-LIMIT: Compiler-Logs können riesig sein!
      // Wenn wir zu viel Text senden, stürzt Ollama wegen "Context Window Overflow" ab.
      if (result.length() > kBuildLogMaxChars) {
            // PRO-TIPP FÜR LLMs: Bei Compiler-Logs stehen die eigentlichen FEHLER (Error Summary)
            // fast immer GANZ UNTEN am Ende der Ausgabe. Wir schneiden also den unwichtigen
            // Anfang (z.B. "Scanning dependencies...") ab und behalten das Ende!

            QString truncated  = "[... BEGINNING OF LOG TRUNCATED DUE TO LENGTH ...]\n\n";
            truncated         += result.right(kBuildLogMaxChars);
            return truncated;
            }

      return result;
      }
