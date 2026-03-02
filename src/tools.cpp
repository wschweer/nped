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
#include "editor.h"
#include "agent.h"
#include "logger.h"
#include "undo.h"
#include "kontext.h"
#include "file.h"
#include "lsclient.h"
#include <QEventLoop>
#include <QTimer>

//---------------------------------------------------------
//   getToolsDefinition
//
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
//---------------------------------------------------------

json Agent::getToolsDefinition() {
      std::string jsonStr = R"([
      {
            "type": "function",
            "function": {
                  "name": "read_file",
                  "description": "read the content of a file",
                  "parameters": {
                        "type": "object",
                        "properties": { "path": { "type": "string", "description": "Dateipfad" } },
                        "required": ["path"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "create_file",
                  "description": "Erzeugt eine neue Datei.",
                  "parameters": {
                        "type": "object",
                        "properties": {
                              "path": { "type": "string" },
                              "content": { "type": "string" }
                        },
                        "required": ["path", "content"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "modify_file",
                  "description": "Überschreibt eine existierende Datei mit neuem Inhalt.",
                  "parameters": {
                        "type": "object",
                        "properties": {
                              "path": { "type": "string" },
                              "content": { "type": "string" }
                        },
                        "required": ["path", "content"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "list_directory",
                  "description": "Listet alle Dateien und Ordner in einem Verzeichnis auf.",
                  "parameters": {
                        "type": "object",
                        "properties": { "path": { "type": "string", "description": "Dateipfad des Verzeichnisses" } },
                        "required": ["path"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "search_project",
                  "description": "Durchsucht alle Dateien im Projekt nach einem Text. Beachtet dabei unsichtbare/ungespeicherte Editor-Puffer.",
                  "parameters": {
                        "type": "object",
                        "properties": {
                              "query": { "type": "string", "description": "Der gesuchte Text" },
                              "file_pattern": { "type": "string", "description": "Optional: Dateimuster, z.B. *.cpp" }
                        },
                        "required": ["query"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "read_file_lines",
                  "description": "Liest gezielt bestimmte Zeilen einer Datei (anstatt head/tail).",
                  "parameters": {
                        "type": "object",
                        "properties": {
                              "path": { "type": "string" },
                              "start_line": { "type": "integer" },
                              "end_line": { "type": "integer" }
                        },
                        "required": ["path", "start_line", "end_line"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "find_symbol",
                  "description": "Nutzt den Language Server (LSP) um die Definition eines Symbols (Klasse, Funktion) im Projekt zu finden.",
                  "parameters": {
                        "type": "object",
                        "properties": {
                              "symbol": { "type": "string", "description": "Das zu suchende Symbol, z.B. MeineKlasse" }
                        },
                        "required": ["symbol"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "fetch_web_documentation",
                  "description": "Lädt den Inhalt einer URL per HTTP GET herunter (für API-Docs etc.).",
                  "parameters": {
                        "type": "object",
                        "properties": { "url": { "type": "string", "description": "Die vollständige URL (inkl. http/https) " } },
                        "required": ["url"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "run_build_command",
                  "description": "Führt einen Shell-Befehl (z.B. 'cmake ..' oder 'make') im 'build'-Ordner des CMake-Projekts aus.",
                  "parameters": {
                        "type": "object",
                        "properties": {
                              "command": { "type": "string", "description": "Der Build-Befehl" }
                        },
                        "required": ["command"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "replace_in_file",
                  "description": "Ersetzt einen exakten Textabschnitt (search) durch einen neuen Text (replace) in einer bestehenden Datei. Nutze dies für kleine Änderungen anstatt modify_file.",
                  "parameters": {
                        "type": "object",
                        "properties": {
                              "path": { "type": "string", "description": "Der Dateipfad" },
                              "search": { "type": "string", "description": "Der exakte Text, der gesucht und überschrieben werden soll." },
                              "replace": { "type": "string", "description": "Der neue Text, der eingefügt werden soll." }
                        },
                        "required": ["path", "search", "replace"],
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "get_git_status",
                  "description": "Liefert den aktuellen Git-Status. HINWEIS: Du brauchst und kennst den Projektpfad nicht. Das System führt diesen Befehl automatisch im korrekten Root-Verzeichnis aus. Rufe das Tool einfach ohne Parameter auf.",
                  "parameters": {
                        "type": "object",
                        "properties": {},
                        "additionalProperties": false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "get_git_diff",
                  "description": "Zeigt die uncommitteten Änderungen als Diff an. Nutze dies, um deine eigenen Änderungen oder die des Nutzers zu überprüfen.",
                  "parameters": {
                        "type": "object",
                        "properties": {
                              "path": {
                                    "type": "string",
                                    "description": "Dateipfad (optional, leer lassen für gesamtes Repo ) "}
                                    },
                        "additionalProperties" : false
                        }
                  }
            },
      {
            "type": "function",
            "function": {
                  "name": "get_git_log",
                  "description": "Zeigt die letzten Git-Commits an, um den Verlauf zu verstehen.",
                  "parameters": {
                        "type": "object",
                        "properties": { "limit": { "type": "integer", "description": "Anzahl der angeforderten Commits (Standard: 5 ) "}
                        },
                  "additionalProperties" : false
                  }
            }
      },
      {
            "type": "function",
            "function": {
                  "name": "create_git_commit",
                  "description": "Staged alle Änderungen (git add .) und erstellt einen Commit. Nutze dies, wenn du eine Aufgabe erfolgreich abgeschlossen hast.",
                  "parameters": {
                        "type": "object",
                        "properties": { "message": { "type": "string", "description": "Eine prägnante Commit-Nachricht" } },
                        "required": ["message"],
                        "additionalProperties": false
                        }
                  }
            }
      ])";

      try {
            return json::parse(jsonStr);
            }

      catch (const json::parse_error& e) {
            Critical("FATAL: Fehler beim Parsen der internen toolsDefinition: {}", e.what());
            return json::array();
            }
      }

//---------------------------------------------------------
//   executeTool
//---------------------------------------------------------

QString Agent::executeTool(const std::string& functionName, const json& arguments) {
      Debug("executeTool aufgerufen: {}", functionName);

      // Hilfsfunktion: Limitiert die Zeilenlänge des Feedbacks im chatDisplay
      auto trim = [](const QString& s, int maxLen = 80) {
            QString res = s.length() > maxLen ? s.left(maxLen - 3) + "..." : s;
            return res.toHtmlEscaped(); // Gleichzeitig HTML Sonderzeichen maskieren
            };

      // 1. Definiere, welche Tools harmlos sind (Nur-Lese-Zugriff)
      bool isReadOnlyTool =
          (functionName == "read_file" || functionName == "read_file_lines" || functionName == "search_project" ||
           functionName == "find_symbol" || functionName == "list_directory" || functionName == "fetch_web_documentation" ||
           functionName == "get_git_status" || functionName == "get_git_diff" || functionName == "get_git_log");

      // 2. Entwurfs-Modus Check
      if (!isExecuteMode && !isReadOnlyTool) {
            // Schreib-Tools simulieren, um den Denkprozess der KI nicht zu unterbrechen
            return "Plan Mode Active: The tool '" + QString::fromStdString(functionName) +
                   "' was NOT executed. This is a read-only simulation. Changes were NOT saved to disk.";
            }

      // ==========================================================
      // Tools OHNE lokales Datei-Pfad Argument
      // ==========================================================

      if (functionName == "run_build_command") {
            if (!arguments.contains("command"))
                  return "Error: Parameter 'command' missing.";
            QString cmd = QString::fromStdString(arguments["command"].get<std::string>());
            chatDisplay->append(QString("<b>[Build Command: %1]</b>").arg(trim(cmd)));
            return runBuildCommand(cmd);
            }
      else if (functionName == "fetch_web_documentation") {
            if (!arguments.contains("url"))
                  return "Error: Parameter 'url' missing.";
            QString url = QString::fromStdString(arguments["url"].get<std::string>());
            chatDisplay->append(QString("<b>[Web Fetch: %1]</b>").arg(trim(url)));
            return fetchWebDocumentation(url);
            }
      else if (functionName == "get_git_status") {
            chatDisplay->append("<b>[Git: Status]</b>");
            return getGitStatus();
            }
      else if (functionName == "get_git_diff") {
            QString p = arguments.contains("path") ? QString::fromStdString(arguments["path"].get<std::string>()) : "";
            chatDisplay->append(QString("<b>[Git: Diff %1]</b>").arg(trim(p)));
            return getGitDiff(p);
            }
      else if (functionName == "get_git_log") {
            int limit = arguments.contains("limit") ? arguments["limit"].get<int>() : 5;
            chatDisplay->append(QString("<b>[Git: Log (%1 Commits)]</b>").arg(limit));
            return getGitLog(limit);
            }
      else if (functionName == "create_git_commit") {
            if (!arguments.contains("message"))
                  return "Error: Parameter 'message' missing.";
            QString msg = QString::fromStdString(arguments["message"].get<std::string>());
            chatDisplay->append(QString("<b>[Git: Commit -> '%1']</b>").arg(trim(msg)));
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
            chatDisplay->append(QString("<b>[Search Project: %1]</b>").arg(trim(query)));
            return searchProject(query, pattern);
            }
      else if (functionName == "find_symbol") {
            if (!arguments.contains("symbol"))
                  return "Error: Parameter 'symbol' missing.";
            QString symbol = QString::fromStdString(arguments["symbol"].get<std::string>());
            chatDisplay->append(QString("<b>[Find Symbol: %1]</b>").arg(trim(symbol)));
            return findSymbol(symbol);
            }
      if (!arguments.contains("path"))
            return QString("Error: Parameter 'path' missing for local file tool: %1").arg(functionName);

      QString path = QString::fromStdString(arguments["path"].get<std::string>());

      // WICHTIG: Pfad-Sicherheitscheck (Sandboxing)
      if (!isPathSafe(path)) {
            Debug("Sicherheitssperre gegriffen für Pfad: {}", path.toStdString());
            return "Security Lock: Access denied! Path is outside of " + _editor->projectRoot();
            }

      // Lokale Datei-Operationen ausführen
      if (functionName == "read_file") {
            chatDisplay->append(QString("<b>[Read File: %1]</b>").arg(trim(path)));
            return readFile(path);
            }
      else if (functionName == "read_file_lines") {
            if (!arguments.contains("start_line") || !arguments.contains("end_line"))
                  return "Error: Parameters 'start_line' or 'end_line' missing.";
            int startLine = arguments["start_line"].get<int>();
            int endLine   = arguments["end_line"].get<int>();
            chatDisplay->append(QString("<b>[Read File Lines: %1 (%2-%3)]</b>").arg(trim(path)).arg(startLine).arg(endLine));
            return readFileLines(path, startLine, endLine);
            }
      else if (functionName == "list_directory") {
            chatDisplay->append(QString("<b>[List Directory: %1]</b>").arg(trim(path)));
            return listDirectory(path);
            }
      else if (functionName == "create_file") {
            if (!arguments.contains("content"))
                  return "Error: Parameter 'content' missing.";
            QString content = QString::fromStdString(arguments["content"].get<std::string>());
            chatDisplay->append(QString("<b>[Create File: %1]</b>").arg(trim(path)));
            return createFile(path, content);
            }
      else if (functionName == "modify_file") {
            if (!arguments.contains("content"))
                  return "Error: Parameter 'content' missing.";
            QString content = QString::fromStdString(arguments["content"].get<std::string>());
            chatDisplay->append(QString("<b>[Modify File: %1]</b>").arg(trim(path)));
            return modifyFile(path, content);
            }
      else if (functionName == "replace_in_file") {
            if (!arguments.contains("search") || !arguments.contains("replace"))
                  return "Error: Parameters 'search' or 'replace' missing.";
            QString search  = QString::fromStdString(arguments["search"].get<std::string>());
            QString replace = QString::fromStdString(arguments["replace"].get<std::string>());
            chatDisplay->append(QString("<b>[Replace in File: %1]</b>").arg(trim(path)));
            return replaceInFile(path, search, replace);
            }
      return "Error: Unknown tool (" + QString::fromStdString(functionName) + ").";
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
//---------------------------------------------------------

QString Agent::readFile(const QString& ipath) {
      QString path;
      if (!ipath.startsWith("/"))
            path = _editor->projectRoot() + "/" + ipath;
      else
            path = ipath;
      File* f = _editor->findFile(path);
      if (f)
            return f->plainText();
      QFile file(path);
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return "Error: Could not read file (" + file.errorString() + ")";
      return QTextStream(&file).readAll();
      }

//---------------------------------------------------------
//   readFileLines
//---------------------------------------------------------

QString Agent::readFileLines(const QString& path, int startLine, int endLine) {
      QString content   = readFile(path);
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
            QString content      = readFile(relativePath);
            QStringList lines    = content.split('\n');
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
      const int MAX_CHARS = 8000;
      if (content.length() > MAX_CHARS) {
            content.truncate(MAX_CHARS);
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
//   generateSessionFileName
//---------------------------------------------------------

QString Agent::generateSessionFileName() {
      if (!_editor)
            return "";

      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QDate date       = QDate::currentDate();
      QString dateStr  = date.toString("dd-MM-yyyy");

      QDir dir(projRoot);
      QStringList filters;
      // GEÄNDERT: Suche nach .json
      filters << QString("Session-%1-*.json").arg(dateStr);
      QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

      int maxNum = 0;
      for (const QFileInfo& fi : files) {
            QString name      = fi.baseName();
            QStringList parts = name.split('-');
            if (parts.size() >= 5) {
                  int num = parts.last().toInt();
                  if (num > maxNum)
                        maxNum = num;
                  }
            }

      QString newNum = QString::number(maxNum + 1).rightJustified(2, '0');
      // GEÄNDERT: Generiere .json
      return QString("%1/Session-%2-%3.json").arg(projRoot, dateStr, newNum);
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
      const int MAX_CHARS = 6000;
      if (result.length() > MAX_CHARS) {
            result.truncate(MAX_CHARS);
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
      const int MAX_CHARS = 4000;
      if (result.length() > MAX_CHARS) {
            // PRO-TIPP FÜR LLMs: Bei Compiler-Logs stehen die eigentlichen FEHLER (Error Summary)
            // fast immer GANZ UNTEN am Ende der Ausgabe. Wir schneiden also den unwichtigen
            // Anfang (z.B. "Scanning dependencies...") ab und behalten das Ende!

            QString truncated  = "[... BEGINNING OF LOG TRUNCATED DUE TO LENGTH ...]\n\n";
            truncated         += result.right(MAX_CHARS);
            return truncated;
            }

      return result;
      }
