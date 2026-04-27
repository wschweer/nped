//=============================================================================
//  nped Program Editor
//  MCP Server Integration
//=============================================================================

#include "mcp.h"
#include <fstream>
#include <iostream>

#include "logger.h"

// Conditional Trace:
#define IO true

//---------------------------------------------------------
//   McpServerConfigs
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
            array.push_back(item);
            }
      return array;
      }

//---------------------------------------------------------
//   McpServer
//---------------------------------------------------------

McpServer::McpServer(const McpServerConfig& config, QObject* parent)
    : QObject(parent), m_config(config), m_process(new QProcess(this)) {
      }

McpServer::~McpServer() {
      stop();
      }

//---------------------------------------------------------
//   start
//---------------------------------------------------------

bool McpServer::start() {
      if (!m_config.enabled)
            return false;

      QString program = m_config.command;
      QStringList argsList;
      if (!m_config.args.isEmpty())
            argsList = QProcess::splitCommand(m_config.args);

      QProcessEnvironment qenv = QProcessEnvironment::systemEnvironment();
      if (!m_config.env.isEmpty()) {
            QStringList envs = m_config.env.split(",", Qt::SkipEmptyParts);
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
      connect(m_process, &QProcess::started, [this] { initialize(); });
      m_process->start();

      Debug("MCP Server <{}> started", m_config.id);
      return true;
      }

void McpServer::stop() {
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
      json params = {
               {"protocolVersion",                                                       "2024-11-05"},
               {     "clientInfo",                {{"name", "nped-mcp-client"}, {"version", "1.0.0"}}},
               {   "capabilities", {{"roots", {{"listChanged", true}}}, {"sampling", json::object()}}}
            };

      sendRequest("initialize", params, [this](const json& response) {
            Debug("MCP Server <{}> initialized", m_config.id);
            sendRequest("notifications/initialized", json::object(), nullptr);

            if (response.contains("capabilities")) {
                  auto caps = response.value("capabilities", json::object());

                  if (caps.contains("tools")) {
                        sendRequest("tools/list", json::object(), [this](const json& toolResponse) {
                              if (toolResponse.contains("tools") && toolResponse["tools"].is_array()) {
                                    m_tools.clear();
                                    for (const auto& t : toolResponse["tools"])
                                          m_tools.push_back(t.get<McpTool>());
                                    }
                              emit toolsChanged();
                              });
                        }

                  if (caps.contains("resources")) {
                        sendRequest("resources/list", json::object(), [this](const json& resResponse) {
                              if (resResponse.contains("resources") && resResponse["resources"].is_array()) {
                                    m_resources.clear();
                                    for (const auto& r : resResponse["resources"])
                                          m_resources.push_back(r.get<McpResource>());
                                    }
                              emit resourcesChanged();
                              });
                        }
                  }
            else
                  Debug("===no caps in repsonse");
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

      std::string msg = request.dump() + "\n";
      auto n          = m_process->write(msg.c_str());
      CLog(IO, "write: {} <{}>", n, request.dump(3));
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
      auto n          = m_process->write(msg.c_str());
      CLog(IO, "write: {} <{}>", n, request.dump(3));
      }

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

void McpServer::handleReadyReadStandardError() {
      QString data = m_process->readAllStandardError();
      Debug("MCP Server STDERR [{}]: {}", m_config.id, data);
      }

void McpServer::parseMessage(const std::string& message) {
      try {
            json response = json::parse(message);
            CLog(IO, "read: <{}>", response.dump(3));
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
                  }
            else if (response.contains("method")) {
                  // Handle notifications or incoming requests
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
      Warning("MCP Server <{}> process error: {}", m_config.id, em[int(error)]);
      }

void McpServer::handleProcessFinished(int exitCode, QProcess::ExitStatus) {
      Debug("MCP Server process finished [{}]: with exit code {}", m_config.id, exitCode);
      }

McpManager& McpManager::instance() {
      static McpManager manager;
      return manager;
      }

McpManager::~McpManager() {
      stopAll();
      }

//---------------------------------------------------------
//   applyConfigs
//---------------------------------------------------------

void McpManager::applyConfigs(const McpServerConfigs& configs) {
      stopAll();
      m_servers.clear();
      m_configs = configs;
      for (const auto& c : m_configs)
            m_servers[c.id] = std::make_unique<McpServer>(c);
      startAll();
      }

//---------------------------------------------------------
//   startAll
//---------------------------------------------------------

void McpManager::startAll() {
      Debug("Start Servers {}", m_servers.size());
      for (auto& [id, server] : m_servers) {
            McpServer* s = &*server;
            s->start();
            connect(s, &McpServer::toolsChanged, [this] { emit toolsChanged(); });
            connect(s, &McpServer::resourcesChanged, this, [this] { emit resourcesChanged(); });
            }
      }

//---------------------------------------------------------
//   stopAll
//---------------------------------------------------------

void McpManager::stopAll() {
      Debug("Stop Servers {}", m_servers.size());
      for (auto& [id, server] : m_servers)
            server->stop();
      }

McpServer* McpManager::getServer(const QString& id) {
      auto it = m_servers.find(id);
      return it != m_servers.end() ? it->second.get() : nullptr;
      }
