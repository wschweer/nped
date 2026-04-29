//=============================================================================
//  nped Program Editor
//
//  Copyright (C) 2025-2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//==============================================================================
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include "mcp.h"
#include "logger.h"
#include "editor.h"

// Conditional Trace:
#define IO true

//---------------------------------------------------------
//   mcpServerConfigsFromJson
//---------------------------------------------------------

McpServerConfigs mcpServerConfigsFromJson(const json& array) {
      McpServerConfigs configs;
      if (!array.is_array())
            return configs;
      for (const auto& item : array) {
            McpServerConfig config;
            config.id      = QString::fromStdString(item.value("id", ""));
            config.command = QString::fromStdString(item.value("command", ""));
            config.args    = QString::fromStdString(item.value("args", ""));
            config.env     = QString::fromStdString(item.value("env", ""));
            config.enabled = item.value("enabled", true);
            config.url     = QString::fromStdString(item.value("url", ""));
            configs.append(config);
            }
      return configs;
      }

//---------------------------------------------------------
//   mcpServerConfigsToJson
//---------------------------------------------------------

json mcpServerConfigsToJson(const McpServerConfigs& configs) {
      json array = json::array();
      for (const auto& config : configs) {
            json item       = json::object();
            item["id"]      = config.id.toStdString();
            item["command"] = config.command.toStdString();
            item["args"]    = config.args.toStdString();
            item["env"]     = config.env.toStdString();
            item["enabled"] = config.enabled;
            item["url"]     = config.url.toStdString();
            array.push_back(item);
            }
      return array;
      }

//---------------------------------------------------------
//   McpServer
//---------------------------------------------------------

McpServer::McpServer(const McpServerConfig& config, Editor* e, QObject* parent)
    : QObject(parent), m_process(new QProcess(this)) {
      _id      = config.id;
      _url     = config.url;
      _command = config.command;
      _args    = config.args;
      _env     = config.env;
      _enabled = config.enabled;

      m_nam   = new QNetworkAccessManager(this);
      _editor = e;
      }

//---------------------------------------------------------
//   connectSse
//---------------------------------------------------------

void McpServer::connectSse() {
      if (m_sseReply) {
            m_sseReply->abort();
            m_sseReply->deleteLater();
            m_sseReply = nullptr;
            }
      QNetworkRequest request(_url);
      request.setRawHeader("Accept", "text/event-stream");
      m_sseReply = m_nam->get(request);
      connect(m_sseReply, &QNetworkReply::readyRead, this, &McpServer::handleSseReadyRead);
      connect(m_sseReply, &QNetworkReply::errorOccurred, this, &McpServer::handleSseError);
      Debug("MCP Server <{}> connecting to URL {}", _id, _url);
      }

//---------------------------------------------------------
//   disconnectSse
//---------------------------------------------------------

void McpServer::disconnectSse() {
      if (m_sseReply) {
            m_sseReply->abort();
            m_sseReply->deleteLater();
            m_sseReply = nullptr;
            }
      m_postEndpoint.clear();
      }

//---------------------------------------------------------
//   start
//---------------------------------------------------------

bool McpServer::start() {
      if (!_enabled)
            return false;

      bool hasUrl     = !_url.isEmpty();
      bool hasCommand = !_command.isEmpty();

      if (hasUrl && !hasCommand)
            connectSse();

      if (!hasCommand)
            return false;

      QString program = _command;
      QStringList argsList;
      if (!_args.isEmpty())
            argsList = QProcess::splitCommand(_args);

      QProcessEnvironment qenv = QProcessEnvironment::systemEnvironment();
      if (!_env.isEmpty()) {
            QStringList envs = _env.split(",", Qt::SkipEmptyParts);
            for (const QString& e : envs) {
                  QStringList kv = e.split("=");
                  if (kv.size() == 2)
                        qenv.insert(kv[0].trimmed(), kv[1].trimmed());
                  }
            }

      m_process->setProgram(program);
      m_process->setArguments(argsList);
      m_process->setProcessEnvironment(qenv);

      connect(m_process, &QProcess::readyReadStandardOutput, this, &McpServer::handleReadyReadStandardOutput);
      connect(m_process, &QProcess::readyReadStandardError, this, &McpServer::handleReadyReadStandardError);
      connect(m_process, &QProcess::errorOccurred, this, &McpServer::handleProcessError);
      connect(m_process, &QProcess::finished, this, &McpServer::handleProcessFinished);
      connect(m_process, &QProcess::started, [this, hasUrl] {
            if (hasUrl) {
                  m_sseRetryCount = 0;
                  QTimer::singleShot(1000, this, &McpServer::connectSse);
                  }
            else {
                  initialize();
                  }
            });
      m_process->start();
      return true;
      }

//---------------------------------------------------------
//   stop
//---------------------------------------------------------

void McpServer::stop() {
      if (m_sseReply) {
            m_sseReply->abort();
            m_sseReply->deleteLater();
            m_sseReply = nullptr;
            }
      if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            if (!m_process->waitForFinished(3000))
                  m_process->kill();
            }
      }

//---------------------------------------------------------
//   initialize
//---------------------------------------------------------

void McpServer::initialize() {
      // Declare our capabilities, including roots (listChanged=true).
      json params = {
               {"protocolVersion",                                                       "2024-11-05"},
               {     "clientInfo",                {{"name", "nped-mcp-client"}, {"version", "1.0.0"}}},
               {   "capabilities", {{"roots", {{"listChanged", true}}}, {"sampling", json::object()}}}
            };

      sendRequest("initialize", params, [this](const json& response) {
            Debug("MCP Server <{}> initialized", _id);

            // If the server sends a roots capability notification, handle it.
            // The server may also send "notifications/roots/list_changed" later;
            // that is handled in parseMessage().

            if (response.contains("capabilities")) {
                  auto caps = response.value("capabilities", json::object());
                  if (caps.contains("roots")) {
                        auto roots = caps["roots"];
                        if (roots.contains("listChanged") && roots["listChanged"].is_boolean())
                              handleRoot = roots["listChanged"];
                        }

                  if (caps.contains("tools")) {
                        sendRequest("tools/list", json::object(), [this](const json& toolResponse) {
                              if (toolResponse.contains("tools") && toolResponse["tools"].is_array()) {
                                    m_tools.clear();
                                    for (const auto& t : toolResponse["tools"])
                                          m_tools.push_back(t.get<McpTool>());
                                    emit toolsChanged();
                                    }
                              });
                        }

                  if (caps.contains("resources")) {
                        sendRequest("resources/list", json::object(), [this](const json& resResponse) {
                              if (resResponse.contains("resources") && resResponse["resources"].is_array()) {
                                    m_resources.clear();
                                    for (const auto& r : resResponse["resources"])
                                          m_resources.push_back(r.get<McpResource>());
                                    emit resourcesChanged();
                                    }
                              });
                        }
                  }
            else {
                  Debug("=== no caps in response");
                  }
            sendRequest("notifications/initialized", json::object(), nullptr);
            initialized = true;
            });
      }

//---------------------------------------------------------
//   callTool
//    Sends a tool call using the MCP 'tools/call' method.
//    Per the MCP spec, tool calls must use:
//    { "method": "tools/call", "params": { "name": "...", "arguments": {...} } }
//---------------------------------------------------------

void McpServer::callTool(const std::string& toolName, const json& arguments,
                         std::function<void(const json&)> callback) {
      json request = {
               {"jsonrpc",                                               "2.0"},
               { "method",                                        "tools/call"},
               { "params", json {{"name", toolName}, {"arguments", arguments}}}
            };

      if (callback) {
            request["id"]                        = m_nextRequestId;
            m_pendingRequests[m_nextRequestId++] = callback;
            }

      if (!_url.isEmpty() && !m_postEndpoint.isEmpty()) {
            QNetworkRequest req(m_postEndpoint);
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            QByteArray data      = request.dump().c_str();
            QNetworkReply* reply = m_nam->post(req, data);
            connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
            return;
            }
      std::string msg = request.dump() + "\n";
      m_process->write(msg.c_str());
      CLog(IO, "write: {} <{}>", _id, request.dump(3));
      }

//---------------------------------------------------------
//   sendRequest
//---------------------------------------------------------

void McpServer::sendRequest(const std::string& method, const json& params,
                            std::function<void(const json&)> callback) {
      json request = {
               {"jsonrpc",  "2.0"},
               { "method", method},
               { "params", params}
            };

      if (callback) {
            request["id"]                        = m_nextRequestId;
            m_pendingRequests[m_nextRequestId++] = callback;
            }

      std::string msg = request.dump() + "\n";
      m_process->write(msg.c_str());
      CLog(IO, "write: {} <{}>", _id, request.dump(3));
      if (!_url.isEmpty() && !m_postEndpoint.isEmpty()) {
            QNetworkRequest req(m_postEndpoint);
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            QByteArray data      = request.dump().c_str();
            QNetworkReply* reply = m_nam->post(req, data);
            connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
            return;
            }
      }

//---------------------------------------------------------
//   addRoot
//---------------------------------------------------------

void McpServer::addRoot(const McpRoot& root) {
      m_roots.append(root);
      //      emit rootsChanged();
      // Notify the server if it has advertised roots capability
      sendRootsChanged();
      }

//---------------------------------------------------------
//   removeRoot
//---------------------------------------------------------

void McpServer::removeRoot(const std::string& uri) {
      for (auto it = m_roots.begin(); it != m_roots.end(); ++it) {
            if (it->uri == uri) {
                  m_roots.erase(it);
                  //                  emit rootsChanged();
                  sendRootsChanged();
                  return;
                  }
            }
      }

//---------------------------------------------------------
//   sendRootsChanged
//    Sends "notifications/roots/list_changed" to the MCP server.
//---------------------------------------------------------

void McpServer::sendRootsChanged() {
      if (!initialized || !handleRoot)
            return;
      // Build the list of roots as JSON array for the notification params
      json rootsArray = json::array();
      for (const auto& root : m_roots) {
            json rootObj;
            rootObj["uri"] = root.uri;
            if (!root.name.empty())
                  rootObj["name"] = root.name;
            rootsArray.push_back(rootObj);
            }

      // Send the notification (no response expected)
      json notification = {
               {"jsonrpc",                              "2.0"},
               { "method", "notifications/roots/list_changed"},
               { "params",            {{"roots", rootsArray}}}
            };

      std::string msg = notification.dump() + "\n";
      CLog(IO, "write roots changed: <{}> <{}>", _id, notification.dump(3));
      m_process->write(msg.c_str());

      if (!_url.isEmpty() && !m_postEndpoint.isEmpty()) {
            QNetworkRequest req(m_postEndpoint);
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            QByteArray data      = notification.dump().c_str();
            QNetworkReply* reply = m_nam->post(req, data);
            connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
            return;
            }
      }

//---------------------------------------------------------
//   handleReadyReadStandardOutput
//---------------------------------------------------------

void McpServer::handleReadyReadStandardOutput() {
      m_buffer += m_process->readAllStandardOutput().toStdString();

      size_t pos;
      while ((pos = m_buffer.find('\n')) != std::string::npos) {
            std::string message = m_buffer.substr(0, pos);
            m_buffer.erase(0, pos + 1);
            if (!message.empty())
                  parseMessage(message);
            }
      }

//---------------------------------------------------------
//   handleReadyReadStandardError
//---------------------------------------------------------

void McpServer::handleReadyReadStandardError() {
      QString data = m_process->readAllStandardError();
      Debug("MCP Server STDERR [{}]: {}", _id, data);
      }

//---------------------------------------------------------
//   parseMessage
//---------------------------------------------------------

void McpServer::parseMessage(const std::string& message) {
      try {
            json response = json::parse(message);
            CLog(IO, "mcp read: {} {}>", _id, response.dump(3));

            // 1) Handle responses to requests (has "id")
            if (response.contains("id") && response["id"].is_number()) {
                  int id  = response["id"].get<int>();
                  auto it = m_pendingRequests.find(id);
                  if (it != m_pendingRequests.end()) {
                        if (response.contains("result"))
                              it->second(response["result"]);
                        else if (response.contains("error"))
                              Warning("MCP RPC Error: {}", response["error"].dump());
                        m_pendingRequests.erase(it);
                        }
                  else {
                        if (response.contains("method") && response["method"] == "roots/list") {
                              addRoot({_editor->projectRoot().toStdString(), "projectRoot"});
                              }
                        else
                              Critical("unhandled", id);
                        }
                  }
            // 2) Handle notifications (has "method", no "id")
            else if (response.contains("method")) {
                  const std::string& method = response["method"].get<std::string>();

                  if (method == "notifications/roots/list_changed") {
                        // The server is telling us its roots have changed.
                        // Store them so tools can discover filesystem roots.
                        if (response.contains("params") && response["params"].contains("roots") &&
                            response["params"]["roots"].is_array()) {
                              m_roots.clear();
                              for (const auto& rootItem : response["params"]["roots"]) {
                                    McpRoot root;
                                    root.uri = rootItem.value("uri", "");
                                    if (rootItem.contains("name") && rootItem["name"].is_string())
                                          root.name = rootItem["name"].get<std::string>();
                                    m_roots.append(root);
                                    }
                              Debug("MCP Server <{}> roots list changed: {} roots", _id, m_roots.size());
                              }
                        }
                  else {
                        // Unknown notification - just log it
                        Debug("<{}> received notification: {}", _id, method);
                        }
                  }
            else {
                  Debug("{}: unknown message", _id);
                  }
            }
      catch (const std::exception& e) {
            Warning("Failed to parse MCP response: {}", e.what());
            }
      }

//---------------------------------------------------------
//   handleProcessError
//---------------------------------------------------------

void McpServer::handleProcessError(QProcess::ProcessError error) {
      const char* em[] = {"FailedToStart", "Crashed", "Timedout", "ReadError", "WriteError", "UnknownError"};
      Warning("MCP Server <{}> process error: {}", _id, em[int(error)]);
      }

//---------------------------------------------------------
//   handleProcessFinished
//---------------------------------------------------------

void McpServer::handleProcessFinished(int, QProcess::ExitStatus) {
      // Debug("MCP Server process finished [{}]: with exit code {}", m_config.id, exitCode);
      }

//---------------------------------------------------------
//   applyConfigs
//---------------------------------------------------------

void McpManager::applyConfigs(const McpServerConfigs& configs) {
      stopAll();
      m_servers.clear();
      for (const auto& c : configs)
            m_servers[c.id] = std::make_unique<McpServer>(c, _editor);
      startAll();

      QString projRoot = _editor->projectRoot();
      if (projRoot.isEmpty())
            return;

      QFileInfo fi(projRoot);
      QString cleanRoot = QDir::cleanPath(fi.absoluteFilePath());
      QString rootUri   = QUrl::fromLocalFile(cleanRoot).toString();
      McpRoot root;
      root.uri  = rootUri.toStdString();
      root.name = QFileInfo(cleanRoot).fileName().toStdString();

      for (auto& [id, server] : m_servers) {
            server->addRoot(root);
            server->sendRootsChanged();
            }
      }

//---------------------------------------------------------
//   startAll
//---------------------------------------------------------

void McpManager::startAll() {
      for (auto& [id, server] : m_servers) {
            McpServer* s = &*server;
            s->start();

            connect(s, &McpServer::toolsChanged, [this] { emit toolsChanged(); });
            connect(s, &McpServer::resourcesChanged, this, [this] { emit resourcesChanged(); });
            //            connect(s, &McpServer::rootsChanged, this, [this] { emit rootsChanged(); });
            }
      }

//---------------------------------------------------------
//   stopAll
//---------------------------------------------------------

void McpManager::stopAll() {
      for (auto& [id, server] : m_servers)
            server->stop();
      }

//---------------------------------------------------------
//   getServer
//---------------------------------------------------------

McpServer* McpManager::getServer(const QString& id) {
      auto it = m_servers.find(id);
      return it != m_servers.end() ? it->second.get() : nullptr;
      }

//---------------------------------------------------------
//   handleSseReadyRead
//---------------------------------------------------------

void McpServer::handleSseReadyRead() {
      if (!m_sseReply)
            return;
      m_buffer += m_sseReply->readAll().toStdString();

      size_t pos;
      static std::string eventType = ""; // Basic handling
      while ((pos = m_buffer.find('\n')) != std::string::npos) {
            std::string line = m_buffer.substr(0, pos);
            m_buffer.erase(0, pos + 1);

            if (line.empty() || line == "\r")
                  continue;
            if (line.back() == '\r')
                  line.pop_back();

            if (line.rfind("event: ", 0) == 0) {
                  eventType = line.substr(7);
                  }
            else if (line.rfind("data: ", 0) == 0) {
                  std::string data = line.substr(6);
                  if (eventType == "endpoint") {
                        QString endpoint = QString::fromStdString(data);
                        if (endpoint.startsWith("http")) {
                              m_postEndpoint = endpoint;
                              }
                        else {
                              QUrl baseUrl(_url);
                              m_postEndpoint = baseUrl.resolved(QUrl(endpoint)).toString();
                              }
                        Debug("MCP Server <{}> SSE endpoint received: {}", _id, m_postEndpoint);
                        initialize();
                        }
                  else {
                        parseMessage(data);
                        }
                  eventType = "";
                  }
            }
      }

//---------------------------------------------------------
//   handleSseError
//---------------------------------------------------------

void McpServer::handleSseError(QNetworkReply::NetworkError error) {
      Warning("MCP Server <{}> SSE error: {}", _id, m_sseReply ? m_sseReply->errorString() : "unknown");
      Warning("MCP Server <{}> target URL: {}", _id, _url);

      bool hasCommand = !_command.isEmpty();
      if (hasCommand) {
            if (m_process->state() == QProcess::NotRunning) {
                  Warning("MCP Server <{}> process is not running. Exit code: {}, Error: {}", _id,
                          m_process->exitCode(), m_process->errorString());
                  }
            else {
                  Warning("MCP Server <{}> process is in state: {}", _id, (int)m_process->state());
                  }
            }

      if (hasCommand && error == QNetworkReply::ConnectionRefusedError) {
            if (m_sseRetryCount < 10) {
                  m_sseRetryCount++;
                  Debug("MCP Server <{}> retrying SSE connection ({}/10)...", _id, m_sseRetryCount);
                  QTimer::singleShot(1000, this, &McpServer::connectSse);
                  }
            else {
                  Warning("MCP Server <{}> SSE connection retries exhausted.", _id);
                  }
            }
      }
