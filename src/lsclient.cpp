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

#include <unistd.h>
#include <iostream>
#include <sys/wait.h>
#include <sstream>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <atomic>
#include <QUrl>
#include "lsclient.h"
#include "logger.h"
#include "editor.h"
#include "undo.h"
#include "kontext.h"
#include "ast.h"
#include "completion.h"
//
enum DiagnosticSeverity { Error = 1, Warning, Information, Hint };
//
//---------------------------------------------------------
//   LServer
//---------------------------------------------------------

struct LServer {
      const char* name;
      const std::string path;
      const std::vector<std::string> arguments;
      };

static std::vector<LServer> lServer = {
         {     "clangd",
    "clangd", {"--query-driver", "--log=error", "--completion-style=detailed", "--compile-commands-dir=build", "--background-index"}                                            },
         {"vscode-html",          "vscode-html-languageserver",                                                                                                            {"--stdio"}},
         {      "pylsp",                               "pylsp",                                                                                                                     {}},
         {      "qmlls", "/home/ws/Qt/6.11.0/gcc_64/bin/qmlls",                                                                                                                     {}}
      };

//---------------------------------------------------------
//   createClient
//---------------------------------------------------------

LSclient* LSclient::createClient(Editor* editor, const std::string& name) {
      for (const auto& s : lServer) {
            Debug("<{}> -- <{}>", s.name, name);
            if (s.name == name) {
                  auto* lc = new LSclient(editor, name);
                  if (!lc->start(s.path, s.arguments)) {
                        Critical("cannot init language server <{}>", s.path);
                        delete lc;
                        lc = nullptr;
                        }
                  return lc;
                  }
            }
      return nullptr;
      }

//---------------------------------------------------------
//   setRange
//---------------------------------------------------------

void PatchItem::setRange(const Range& r, File* f) {
      startPos = r.start;
      Pos p2   = r.end;
      toRemove = f->distance(startPos, p2);
      }

//---------------------------------------------------------
//   gotoRequest
//---------------------------------------------------------

void LSclient::gotoRequest(const char* req, File* file, const Pos& cursor) {
      callbacks[id] = [this](const json& msg) {
            Debug("<{}>", msg.dump(3));
            if (!msg.contains("result") || msg["result"].is_null())
                  return;
            const json& result = msg["result"];

            // LSP erlaubt Location | Location[] | null
            auto handleLocation = [this](const json& r) {
                  Range range(r["range"]);
                  std::string uri = r["uri"];
                  if (uri.starts_with("file://"))
                        uri = uri.substr(7);
                  editor->gotoKontext(QString::fromStdString(uri), Pos(range.start.col, range.start.row));
                  };

            if (result.is_array()) {
                  if (!result.empty())
                        handleLocation(result[0]);
                  }
            else if (result.is_object()) {
                  handleLocation(result);
                  }
            };
      json textDocument;
      textDocument["uri"]     = "file://" + file->path().toStdString();
      textDocument["version"] = file->version();

      json params;
      params["textDocument"] = textDocument;
      json position;
      position["line"]      = cursor.row;
      position["character"] = cursor.col;
      params["position"]    = position;
      request(req, params);
      }

//---------------------------------------------------------
//   gotoDefinition
//   gotoTypeDefinition
//   gotoImplementation
//---------------------------------------------------------

void LSclient::gotoDefinition(File* file, const Pos& cursor) {
      Debug("=====");
      gotoRequest("textDocument/definition", file, cursor);
      }

void LSclient::gotoTypeDefinition(File* file, const Pos& cursor) {
      Debug("=====");
      gotoRequest("textDocument/typeDefinition", file, cursor);
      }

void LSclient::gotoImplementation(File* file, const Pos& cursor) {
      Debug("=====");
      gotoRequest("textDocument/implementation", file, cursor);
      }

//---------------------------------------------------------
//   hover
//---------------------------------------------------------

void LSclient::hover(File* file, const Pos& cursor) {
      callbacks[id] = [this](const json& msg) {
            const json& result = msg["result"];
            Debug("{}", result.dump(4));
            if (result.contains("contents")) {
                  const json& contents = result["contents"];
                  std::string kind     = contents.value("kind", "");
                  std::string value    = contents.value("value", "");
                  Debug("hover <{}> <{}>", kind, value);
                  editor->setInfoText(QString::fromStdString(value));
                  }
            else
                  editor->setInfoText("");
            };
      Debug("==");
      json textDocument;
      textDocument["uri"]     = "file://" + file->path().toStdString();
      textDocument["version"] = file->version();
      json params;
      params["textDocument"] = textDocument;
      json position;
      position["line"]      = cursor.row;
      position["character"] = cursor.col;
      params["position"]    = position;
      request("textDocument/hover", params);
      }

//---------------------------------------------------------
//   sendCompletionRequest
//---------------------------------------------------------

void LSclient::completionRequest(Kontext* k) {
      File* file    = k->file();
      callbacks[id] = [k](const json& msg) {
            const json& result = msg["result"];
            Debug("{}", result.dump(4));
            if (result.contains("items")) {
                  Completions list;
                  for (const json& item : result["items"]) {
                        Completion c;
                        c.label            = QString::fromStdString(item.value("label", ""));
                        c.patch.insertText = QString::fromStdString(item.value("insertText", ""));
                        if (item.contains("textEdit")) {
                              const json& textEdit = item["textEdit"];
                              Range r              = textEdit["range"];
                              c.patch.setRange(r, k->file());
                              }
                        list.push_back(c);
                        }
                  k->editor->showCompletions(list);
                  }
            };
      json textDocument;
      textDocument["uri"]     = "file://" + file->path().toStdString();
      textDocument["version"] = file->version();
      json params;
      params["textDocument"] = textDocument;
      json position;
      //      Pos cursor = k->cursor().filePos;
      position["line"]      = k->fileRow();
      position["character"] = k->fileCol();
      params["position"]    = position;
      request("textDocument/completion", params);
      }

//---------------------------------------------------------
//   prepareRenameRequest
//---------------------------------------------------------

void LSclient::prepareRenameRequest(Kontext* k) {
      callbacks[id] = [this, k](const json& msg) {
            const json& result = msg["result"];
            Debug("{}", result.dump(4));
            QString placeholder = QString::fromStdString(result.value("placeholder", ""));
            Range range(result["range"]);
            editor->rename(k, placeholder, range.start.row, range.start.col, range.end.col);
            };
      File* file = k->file();
      json textDocument;
      textDocument["uri"]     = "file://" + file->path().toStdString();
      textDocument["version"] = file->version();
      json params;
      params["textDocument"] = textDocument;
      json position;
      position["line"]      = k->fileRow();
      position["character"] = k->fileCol();
      params["position"]    = position;
      request("textDocument/prepareRename", params);
      }

//---------------------------------------------------------
//   renameRequest
//---------------------------------------------------------

void LSclient::renameRequest(Kontext* k, const QString& newName, int row, int col) {
      callbacks[id] = [this](const json& msg) {
            const json& result = msg["result"];
            // Debug("{}", result.dump(4));
            const json& changes = result["changes"];
            for (auto it = changes.begin(); it != changes.end(); ++it) {
                  std::string filePath = it.key().substr(7);
                  // Debug("file: <{}>", filePath);

                  Kontext* kontext = editor->lookupKontext(QString::fromStdString(filePath));
                  if (!kontext) {
                        Fatal("no kontext");
                        return; // for compiler analysis
                        }
                  kontext->file()->undo()->beginMacro();
                  const auto& c = kontext->cursor();
                  auto patch    = new Patch(kontext->file(), c, c);
                  for (const auto& change : it.value()) {
                        QString newText = QString::fromStdString(change["newText"]);
                        Debug("rename {} {}", kontext->file()->path(), newText);
                        Range range(change["range"]);
                        PatchItem pi;
                        pi.setRange(range, kontext->file());
                        pi.insertText = newText;
                        patch->add(pi);
                        }
                  if (!patch->empty())
                        kontext->file()->undo()->push(patch);
                  else
                        delete patch;
                  kontext->file()->undo()->endMacro();
                  }
            };
      File* file = k->file();
      json textDocument;
      textDocument["uri"]     = "file://" + file->path().toStdString();
      textDocument["version"] = file->version();
      json params;
      params["textDocument"] = textDocument;
      json position;
      position["line"]      = row;
      position["character"] = col;
      params["position"]    = position;
      params["newName"]     = newName.toStdString();
      request("textDocument/rename", params);
      }

//---------------------------------------------------------
//   readerLoop
//---------------------------------------------------------

void LSclient::readerLoop() {
      std::vector<char> buffer(1024 * 128);
      ssize_t bytesRead;
      std::string message;
      size_t contentLength;
      bool readHeader = true;

      struct pollfd fds[3];
      fds[0].fd     = stdoutPipe[0];
      fds[0].events = POLLIN;
      fds[1].fd     = stderrPipe[0];
      fds[1].events = POLLIN;
      fds[2].fd     = stopFd;
      fds[2].events = POLLIN;

      int flags = fcntl(stdoutPipe[0], F_GETFL, 0);
      if (flags >= 0)
            fcntl(stdoutPipe[0], F_SETFL, flags | O_NONBLOCK);

      flags = fcntl(stderrPipe[0], F_GETFL, 0);
      if (flags >= 0)
            fcntl(stderrPipe[0], F_SETFL, flags | O_NONBLOCK);

      running = true;
      while (running) {
            int ret = poll(fds, 3, -1);
            if (ret < 0) {
                  if (errno == EINTR)
                        continue;
                  break;
                  }
            if (fds[2].revents & POLLIN)
                  break;
            else if (fds[1].revents & POLLIN) {
                  char buffer2[4096];
                  for (;;) {
                        ssize_t n = read(stderrPipe[0], buffer2, sizeof(buffer2) - 1);
                        if (n > 0) {
                              buffer2[n] = '\0';
                              QString s(buffer2);
                              Log("{}: <{}>", _name, s.trimmed());
                              }
                        else
                              break;
                        }
                  }
            else if (fds[0].revents & POLLIN) {

                  //*********************************************************************
                  //    wait until there is something to read
                  //          while (there is something to read)
                  //                while (there is something to process in message)
                  //                      process message
                  //*********************************************************************

                  for (;;) {
                        bytesRead = read(stdoutPipe[0], buffer.data(), buffer.size() - 1);
                        if (bytesRead <= 0) {
                              if (bytesRead < 0 && errno != EAGAIN)
                                    Debug("error({}): {}", errno, strerror(errno));
                              break;
                              }
                        buffer.data()[bytesRead]  = '\0';
                        message           += buffer.data();
                        //            Debug("{} {}", bytesRead, message.length());
                        for (;;) {
                              if (readHeader) {
                                    size_t end = message.find("\r\n\r\n");
                                    if (end != std::string::npos) {
                                          size_t pos = message.find("Content-Length: ");
                                          if (pos == std::string::npos) {
                                                // start over
                                                Debug("garbage input: <{}>", message);
                                                message.clear();
                                                continue;
                                                }
                                          size_t start  = pos + 16;
                                          contentLength = std::stoi(message.substr(start, end - start));
                                          message       = message.substr(end + 4, std::string::npos);
                                          readHeader    = false;
                                          }
                                    else
                                          break; // read more
                                    }
                              if (!readHeader) {
                                    if (message.length() >= contentLength) {
                                          if (processMessage(message.substr(0, contentLength))) {
                                                message    = message.substr(contentLength, std::string::npos);
                                                readHeader = true;
                                                }
                                          }
                                    else
                                          break; // read more
                                    }
                              }
                        }
                  }
            }
      close(fds[0].fd);
      close(fds[1].fd);
      close(fds[2].fd);
      }

//---------------------------------------------------------
//   start
//---------------------------------------------------------

bool LSclient::start(const std::string& path, const std::vector<std::string>& args) {
      // Erstellen der drei Pipes
      if (pipe(stdinPipe) == -1 || pipe(stdoutPipe) == -1 || pipe(stderrPipe) == -1) {
            perror("pipe");
            return false;
            }

      pid_t pid = fork();
      if (pid == -1) {
            perror("fork");
            return false;
            }

      if (pid == 0) { // --- KINDPROZESS ---
            // Umleiten der Standard-Streams
            dup2(stdinPipe[0], STDIN_FILENO);
            dup2(stdoutPipe[1], STDOUT_FILENO);
            dup2(stderrPipe[1], STDERR_FILENO);

            // Schließen der nicht benötigten Enden im Kind
            close(stdinPipe[1]);
            close(stdoutPipe[0]);
            close(stderrPipe[0]);

            // Argument-Vektor vorbereiten (execvp erwartet char* const*)
            std::vector<char*> c_args;
            c_args.push_back(const_cast<char*>(path.c_str()));
            for (const auto& arg : args)
                  c_args.push_back(const_cast<char*>(arg.c_str()));
            c_args.push_back(nullptr);

            execvp(path.c_str(), c_args.data());

            // Falls execvp fehlschlägt:
            perror("execvp");
            _exit(EXIT_FAILURE);
            }
      else { // --- ELTERNPROZESS ---
            // Schließen der Enden, die der Kindprozess nutzt
            close(stdinPipe[0]);
            close(stdoutPipe[1]);
            close(stderrPipe[1]);

            reader = new std::thread(&LSclient::readerLoop, this);
            initializeRequest();
            }
      return true;
      }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

bool LSclient::write(const std::string& txt) {
      size_t total = 0;
      while (total < txt.size()) {
            ssize_t n = ::write(stdinPipe[1], txt.c_str() + total, txt.size() - total);
            if (n == -1) {
                  if (errno == EINTR)
                        continue;
                  Critical("write failed, errno: {} ({})", errno, strerror(errno));
                  return false;
                  }
            total += static_cast<size_t>(n);
            }
      return true;
      }

//---------------------------------------------------------
//   writeMessage
//---------------------------------------------------------

bool LSclient::writeMessage(const std::string& jsonPayload) {
      return write(std::format("Content-Length: {}\r\n"
                               "Content-Type: application/vscode-jsonrpc; charset=utf-8\r\n"
                               "\r\n"
                               "{}",
                               jsonPayload.length(), jsonPayload));
      }

//---------------------------------------------------------
//   initializeRequest
//---------------------------------------------------------

bool LSclient::initializeRequest() {
      callbacks[id] = [this](const json& msg) {
            if (msg.contains("result") && msg["result"].contains("capabilities"))
                  ; // scap.read(msg["result"]["capabilities"]);
            notification("initialized");
            _initialized = true;
            emit initializedChanged();
            };

      json capabilities;
      json window;
      window["workDoneProgress"] = true;
      window["showMessage"]      = true;
      capabilities["window"]     = window;
      json textDocument;
      json rename;
      rename["dynamicRegistration"] = false;
      rename["prepareSupport"]      = true;
      textDocument["rename"]        = rename;
      capabilities["textDocument"]  = textDocument;
      json params;
      params["capabilities"] = capabilities;
      params["processId"]    = getpid(); // Die PID unseres eigenen Programms
      params["rootUri"]      = "file://" + editor->projectRoot().toStdString();
      params["trace"]        = "off";

      Debug("rootUri <{}>", "file://" + editor->projectRoot().toStdString());
      return request("initialize", params);
      }

//---------------------------------------------------------
//   notification
//---------------------------------------------------------

bool LSclient::notification(const char* method, const json& params) {
      json msg;
      msg["jsonrpc"] = "2.0";
      msg["method"]  = method;
      msg["params"]  = params;
      //      Debug("<{}>", method);
      return writeMessage(msg.dump());
      }

//---------------------------------------------------------
//   request
//---------------------------------------------------------

bool LSclient::request(const char* method, const json& params) {
      json msg;
      msg["jsonrpc"] = "2.0";
      msg["method"]  = method;
      msg["params"]  = params;
      msg["id"]      = id++;
      return writeMessage(msg.dump());
      }

//---------------------------------------------------------
//   didOpenNotification
//---------------------------------------------------------

bool LSclient::didOpenNotification(File* file) {
      //      Debug("<{}>", file->path());
      json textDocument;
      textDocument["uri"]        = "file://" + file->path().toStdString();
      textDocument["languageId"] = file->languageId().toStdString();
      textDocument["version"]    = file->version();
      textDocument["text"]       = file->fileText().join('\n').toStdString();
      json params;
      params["textDocument"] = textDocument;
      //      Debug("{}", textDocument["uri"].dump());
      return notification("textDocument/didOpen", params);
      }

//---------------------------------------------------------
//   didCloseNotification
//---------------------------------------------------------

bool LSclient::didCloseNotification(File* file) {
      //      Debug("<{}>", file->path());
      json textDocument;
      textDocument["uri"]        = "file://" + file->path().toStdString();
      textDocument["languageId"] = file->languageId().toStdString();
      textDocument["version"]    = file->version();
      json params;
      params["textDocument"] = textDocument;
      //      Debug("{}", textDocument["uri"].dump());
      return notification("textDocument/didClose", params);
      }

//---------------------------------------------------------
//   LSclient
//---------------------------------------------------------

LSclient::LSclient(Editor* e, const std::string& n) {
      _name  = n;
      editor = e;
      stopFd = eventfd(0, EFD_NONBLOCK);
      connect(this, &LSclient::notificationReceived, this, &LSclient::handleNotification, Qt::QueuedConnection);
      connect(this, &LSclient::responseReceived, this, &LSclient::handleResponse, Qt::QueuedConnection);
      }

LSclient::~LSclient() {
      stop();
      auto closefd = [](int& fd) {
            if (fd != -1) {
                  ::close(fd);
                  fd = -1;
                  }
            };
      closefd(stdoutPipe[0]);
      closefd(stderrPipe[0]);
      closefd(stopFd);
      }

//---------------------------------------------------------
//   stop
//---------------------------------------------------------

void LSclient::stop() {
      if (running) {
            running = false;
            uint64_t u{1};
            ::write(stopFd, &u, sizeof(uint64_t));
            }
      if (reader && reader->joinable())
            reader->join();
      }

//---------------------------------------------------------
//   formattingRequest
//---------------------------------------------------------

void LSclient::formattingRequest(Kontext* kontext) {
      callbacks[id] = [this, kontext](const json& msg) {
            //      Debug("{}", f->path());
            File* file         = kontext->file();
            auto patch         = new Patch(file, kontext->cursor(), kontext->cursor());
            const json& result = msg["result"];

            for (const auto& p : result) {
                  PatchItem pi;
                  pi.insertText = QString::fromStdString(p["newText"]);
                  Range range(p["range"]);
                  pi.startPos = range.start;
                  pi.toRemove = file->distance(pi.startPos, range.end);
                  patch->add(pi);
                  }
            editor->startCmd();
            if (!patch->empty())
                  file->undo()->push(patch);
            else
                  delete patch;
            file->postprocessFormat(); // hack
            editor->endCmd();
            };
      File* file = kontext->file();
      json textDocument;
      textDocument["uri"]     = "file://" + file->path().toStdString();
      textDocument["version"] = file->version();
      json options;
      options["tabSize"]                = file->tab();
      options["trimTrailingWhitespace"] = true;
      options["insertFinalNewline"]     = false;
      options["trimFinalNewlines"]      = true;
      json params;
      params["textDocument"] = textDocument;
      params["options"]      = options;
      request("textDocument/formatting", params);
      }

//---------------------------------------------------------
//   readRange
//---------------------------------------------------------

static Range readRange(const json& msg) {
      Range r;
      const auto& range = msg["range"];
      r.start.col       = range["start"]["character"];
      r.start.row       = range["start"]["line"];
      r.end.col         = range["end"]["character"];
      r.end.row         = range["end"]["line"];
      return r;
      }

//---------------------------------------------------------
//   handleDiagnostics
//---------------------------------------------------------

void LSclient::handleDiagnostics(const json& params) {
      std::string s = params["uri"];
      //      Debug("string: <{}>", s);
      auto path = QUrl(QString::fromStdString(s)).path();
      File* f   = editor->findFile(path);
      if (!f) {
            // Debug("cannot find file <{}>", path);
            return;
            }
      Lines map;
      f->clearLabel();

      auto& diagnostics = params["diagnostics"];
      for (auto& d : diagnostics) {
            Range r = readRange(d);

            int y = r.start.row;
            if (y >= f->rows()) {
                  // Debug("bad row {} rows {}", pt.y(), f->rows());
                  y = f->rows() - 1;
                  }
            QChar marker{' '};
            QColor color;
            if (d.contains("severity")) {
                  switch (DiagnosticSeverity(d["severity"])) {
                        case DiagnosticSeverity::Error:
                              marker = 'E';
                              color  = QColorConstants::Red;
                              break;
                        case DiagnosticSeverity::Warning:
                              marker = 'W';
                              color  = QColorConstants::Green;
                              break;
                        case DiagnosticSeverity::Information:
                              marker = 'I';
                              color  = QColorConstants::Black;
                              break;
                        case DiagnosticSeverity::Hint:
                              marker = 'H';
                              color  = QColorConstants::Black;
                              break;
                        }
                  }
            else {
                  // this is the default type
                  marker = 'H';
                  color  = QColorConstants::Black;
                  }
            f->setLabel(y, marker, color);

            QString message  = QString::fromStdString(d["message"]);
            QStringList list = message.split("\n");
            for (int k = 0; k < list.size(); ++k) {
                  auto s = list[k];
                  if (s == "")
                        s = "|";
                  if (k == 0)
                        map.push_back(QString("%1: %2").arg(y + 1).arg(s), QPoint(0, y));
                  else
                        map.push_back(QString("    %1").arg(s), QPoint(0, y));
                  }
            if (list.empty())
                  map.push_back("???", QPoint(0, y));
            }
      f->setBugs(map);
      }

//---------------------------------------------------------
//   processMessage
//    runs in LSclient outThread context
//    return false on error
//---------------------------------------------------------

bool LSclient::processMessage(const std::string& message) {
      json response;
      try {
            response = json::parse(message);
            }
      catch (json::parse_error& e) {
            Critical("json::parse failed: {}", e.what());
            }
      if (response.contains("error")) {
            Debug("Server error: {}", response["error"]["message"].dump(4));
            return false;
            }
      if (response.contains("id")) {
            //*********************************************
            //    handle responses
            //*********************************************
            try {
                  int id = response["id"];
                  emit responseReceived(id, response);
                  }
            catch (...) {
                  Debug("Server error: {}", response.dump(4));
                  return false;
                  }
            }
      else {
            //*********************************************
            //    handle notifications
            //*********************************************
            emit notificationReceived(response);
            }
      return true;
      }

//---------------------------------------------------------
//   handleNotification
//    runs in gui thread context
//---------------------------------------------------------

void LSclient::handleNotification(json response) {
      auto method = response.value("method", "");
      if (method == "textDocument/publishDiagnostics")
            handleDiagnostics(response["params"]);
      else if (method == "$/progress") {
            //            Debug("{}", response.dump(2));
            const auto& params = response["params"];
            //            const auto& token  = params["token"];
            const auto& value = params["value"];
            const auto& kind  = value["kind"];
            double percentage = value.value("percentage", 0.0);
            if (kind == "report")
                  editor->showProgress(percentage);
            else if (kind == "end")
                  editor->showProgress(false);
            else if (kind == "begin") {
                  editor->showProgress(true);
                  editor->showProgress(percentage);
                  }
            }
      else
            Debug("not handled: {}", response.dump(4));
      }

//---------------------------------------------------------
//   handleResponse  - handle server request
//    runs in gui thread context
//---------------------------------------------------------

void LSclient::handleResponse(int id, json response) {
      if (callbacks.contains(id)) {
            callbacks[id](response);
            callbacks.erase(id);
            }
      else if (response.contains("method")) {
            const auto method = response["method"];
            if (method == "window/workDoneProgress/create") {
                  json msg;
                  msg["id"]      = id;
                  msg["jsonrpc"] = "2.0";
                  writeMessage(msg.dump());
                  editor->showProgress(true);
                  }
            else {
                  Debug("unhandled response id {} {}", id, response.dump(4));
                  }
            }
      else {
            Debug("unhandled response id {} {}", id, response.dump(4));
            }
      }

//---------------------------------------------------------
//   didChangeNotification
//---------------------------------------------------------

bool LSclient::didChangeNotification(File* file, const Patches& patches) {
      json textDocument;
      textDocument["uri"]     = "file://" + file->path().toStdString();
      textDocument["version"] = file->version();
      json options;
      json params;
      params["textDocument"] = textDocument;
      json changes           = json::array();

      for (const auto& patch : patches) {
            json change;
            if (patch.insertText.isEmpty() && patch.toRemove == 0) {
                  Critical("empty patch");
                  continue;
                  }
            Range range;
            range.end       = file->advance(patch.startPos, patch.toRemove);
            range.start     = patch.startPos;
            change["range"] = range.toJson();
            change["text"]  = patch.insertText.toStdString();
            changes.push_back(change);
            }
      params["contentChanges"] = changes;
      //      Debug("{}", params.dump(4));
      return notification("textDocument/didChange", params);
      }

//---------------------------------------------------------
//   astRequest
//---------------------------------------------------------

void LSclient::astRequest(File* file) {
      callbacks[id] = [this, file](const json& msg) { astResponse(file, msg); };
      json textDocument;
      textDocument["uri"]     = "file://" + file->path().toStdString();
      textDocument["version"] = file->version();
      Range range;
      range.start           = QPoint(0, 0);
      range.end             = QPoint(0, file->rows());
      textDocument["range"] = range.toJson();

      json params;
      params["textDocument"] = textDocument;
      request("textDocument/ast", params);
      }

//---------------------------------------------------------
//   astResponse
//---------------------------------------------------------

void LSclient::astResponse(File* f, const json& msg) {
      //      Debug("{}", msg.dump(4));
      ASTNode node;
      node.read(msg["result"]);
      f->setAST(node);
      }

//---------------------------------------------------------
//   symbolRequest
//---------------------------------------------------------

void LSclient::symbolRequest(const QString& symbol) {
      Debug("<{}>", symbol);
      callbacks[id] = [this](const json& msg) {
            QString res = "";
            Debug("result <{}>", msg.dump(3));
            if (msg.contains("result") && msg["result"].is_array()) {
                  for (const auto& item : msg["result"]) {
                        if (item.contains("location")) {
                              const json& loc = item["location"];
                              QString uri     = QString::fromStdString(loc["uri"].get<std::string>());
                              if (uri.startsWith("file://"))
                                    uri = uri.mid(7);
                              Range range(loc["range"]);
                              res += uri + ":" + QString::number(range.start.row + 1) + "\n";
                              }
                        }
                  }
            if (res.isEmpty())
                  res = "Symbol not found.";
            Debug("result <{}>", res);
            emit symbolSearchResult(res);
            };
      json params;
      params["query"] = symbol.toStdString();
      request("workspace/symbol", params);
      }
