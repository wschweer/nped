//=================================================================================
//  nped Program Editor
//
//  Copyright (C) 2025-2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================
#pragma once

#include <QString>
#include <QObject>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QList>
#include <QMetaType>
#include <QDataStream>
#include <map>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
class Editor;

//---------------------------------------------------------
//   MCPToolBuilder
//---------------------------------------------------------

class MCPToolBuilder
      {
      json tool;

    public:
      MCPToolBuilder(const std::string& name, const std::string& description) {
            tool["name"]                      = name;
            tool["description"]               = description;
            tool["inputSchema"]["type"]       = "object"; // MCP liefert "inputSchema"
            tool["inputSchema"]["properties"] = json::object();
            tool["inputSchema"]["required"]   = json::array();
            }
      // Fügt einen Parameter hinzu, der für Gemini optimiert ist
      MCPToolBuilder& add_parameter(const std::string& name, const std::string& type,
                                    const std::string& description, bool required = true,
                                    const std::vector<std::string>& enums = {}) {
            json param = {
                     {       "type",        type},
                     {"description", description}
                  };
            if (!enums.empty())
                  param["enum"] = enums;
            tool["inputSchema"]["properties"][name] = param;

            if (required)
                  tool["inputSchema"]["required"].push_back(name);
            return *this;
            }
      json build() { return tool; }
      };

//---------------------------------------------------------
//   McpServerConfig
//    Data structure representing the configuration of an MCP server.
//---------------------------------------------------------

struct McpServerConfig {
      Q_GADGET
      Q_PROPERTY(QString id MEMBER id)
      Q_PROPERTY(QString command MEMBER command)
      Q_PROPERTY(QString args MEMBER args)
      Q_PROPERTY(QString env MEMBER env)
      Q_PROPERTY(bool enabled MEMBER enabled)
      Q_PROPERTY(QString url MEMBER url)

    public:
      QString id;          // Unique identifier for the server
      QString command;     // The executable command (e.g., 'node', 'python')
      QString args;        // Arguments to pass to the executable
      QString env;         // Specific environment variables (KEY=VAL,KEY2=VAL2)
      bool enabled = true; // Whether this server is active
      QString url;         // URL to an already running server
      bool operator==(const McpServerConfig& other) const {
            return id == other.id && command == other.command && args == other.args && env == other.env &&
                   enabled == other.enabled && url == other.url;
            }
      };

using McpServerConfigs = QList<McpServerConfig>;
extern McpServerConfigs mcpServerConfigsFromJson(const json& array);
extern json mcpServerConfigsToJson(const McpServerConfigs& configs);

//---------------------------------------------------------
//   McpTool
//    Data structure for a Tool exposed by an MCP server.
//---------------------------------------------------------

struct McpTool {
      std::string name;
      std::string description;
      json inputSchema;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE(McpTool, name, description, inputSchema)
      };

//---------------------------------------------------------
//   McpResource
//    Data structure for a Resource exposed by an MCP server.
//---------------------------------------------------------

struct McpResource {
      std::string uri;
      std::string name;
      std::string mimeType;
      std::string description;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE(McpResource, uri, name, mimeType, description)
      };

//---------------------------------------------------------
//   McpRoot
//    Data structure for a Filesystem Root exposed by the MCP client.
//---------------------------------------------------------

struct McpRoot {
      std::string uri;  // URI of the root (e.g. "file:///home/user/project")
      std::string name; // Optional human-readable name

      NLOHMANN_DEFINE_TYPE_INTRUSIVE(McpRoot, uri, name)
      };

using McpRoots = QList<McpRoot>;

//---------------------------------------------------------
//   McpServer
//---------------------------------------------------------

class McpServer : public QObject
      {
      Q_OBJECT

      QString _id;
      QString _url;
      QString _command;
      QString _args;
      QString _env;
      bool _enabled;

      McpServerConfig _config;
      QProcess* m_process;

      QNetworkAccessManager* m_nam;
      QNetworkReply* m_sseReply = nullptr;
      int m_sseRetryCount       = 0;
      void connectSse();
      QString m_postEndpoint;
      std::vector<McpTool> m_tools;
      std::vector<McpResource> m_resources;
      McpRoots m_roots;
      Editor* _editor;

      bool initialized { false };
      bool handleRoot { false };
      int m_nextRequestId = 1;
      std::map<int, std::function<void(const json&)>> m_pendingRequests;

      std::string m_buffer;

      void parseMessage(const std::string& message);
      void initialize();

    private slots:
      void handleReadyReadStandardOutput();
      void handleReadyReadStandardError();
      void handleProcessError(QProcess::ProcessError error);
      void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
      void handleSseReadyRead();
      void handleSseError(QNetworkReply::NetworkError);

    signals:
      void toolsChanged();
      void resourcesChanged();
//      void rootsChanged();

    public:
      explicit McpServer(const McpServerConfig& config, Editor*, QObject* parent = nullptr);
      ~McpServer() override { stop(); }
      bool start();
      void stop();

      const QString& id() const { return _id; }
      const QString& url() const { return _url; }

      // Available capabilities after initialization
      const std::vector<McpTool>& getTools() const { return m_tools; }
      const std::vector<McpResource>& getResources() const { return m_resources; }
      const McpRoots& getRoots() const { return m_roots; }

      // Communication
      // MCP tool call (uses tools/call method per spec)
      void callTool(const std::string& toolName, const json& arguments, std::function<void(const json&)> callback);
      void sendRequest(const std::string& method, const json& params, std::function<void(const json&)> callback);

      // Roots management
      void addRoot(const McpRoot& root);
      void removeRoot(const std::string& uri);

      void sendRootsChanged();
      // SSE
      void disconnectSse();
      };

//---------------------------------------------------------
//   McpManager
//    Manager to hold all configured MCP servers
//---------------------------------------------------------

class McpManager : public QObject
      {
      Q_OBJECT

      std::map<QString, std::unique_ptr<McpServer>> m_servers;
      Editor* _editor;

    signals:
      void toolsChanged();
      void resourcesChanged();
//      void rootsChanged();

    public:
      McpManager(Editor* e, QObject* parent = nullptr) : QObject(parent), _editor(e) {}
      ~McpManager() override { stopAll(); }
      void applyConfigs(const McpServerConfigs& configs);

      void startAll();
      void stopAll();

      McpServer* getServer(const QString& id);
      };

Q_DECLARE_METATYPE(McpServerConfig)
Q_DECLARE_METATYPE(McpServerConfigs)
Q_DECLARE_METATYPE(McpRoot)
Q_DECLARE_METATYPE(McpRoots)
