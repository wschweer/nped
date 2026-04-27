//=============================================================================
//  nped Program Editor
//  MCP Server Integration
//=============================================================================

#pragma once

#include <QString>
#include <QObject>
#include <QProcess>
#include <QList>
#include <QMetaType>
#include <map>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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

    public:
      QString id;          // Unique identifier for the server
      QString command;     // The executable command (e.g., 'node', 'python')
      QString args;        // Arguments to pass to the executable
      QString env;         // Specific environment variables (KEY=VAL,KEY2=VAL2)
      bool enabled = true; // Whether this server is active

      bool operator==(const McpServerConfig&) const = default;
      };

using McpServerConfigs = QList<McpServerConfig>;

extern McpServerConfigs mcpServerConfigsFromJson(const json& array);
extern json mcpServerConfigsToJson(const McpServerConfigs& configs);

Q_DECLARE_METATYPE(McpServerConfig)
Q_DECLARE_METATYPE(McpServerConfigs)

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
//   McpServer
//---------------------------------------------------------

class McpServer : public QObject
      {
      Q_OBJECT

      McpServerConfig m_config;
      QProcess* m_process;
      std::vector<McpTool> m_tools;
      std::vector<McpResource> m_resources;

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

    signals:
      void toolsChanged();
      void resourcesChanged();

    public:
      explicit McpServer(const McpServerConfig& config, QObject* parent = nullptr);
      ~McpServer() override;

      bool start();
      void stop();
      const McpServerConfig& getConfig() const { return m_config; }
      // Available capabilities after initialization
      const std::vector<McpTool>& getTools() const { return m_tools; }
      const std::vector<McpResource>& getResources() const { return m_resources; }
      // Communication
      // MCP tool call (uses tools/call method per spec)
      void callTool(const std::string& toolName, const json& arguments,
                    std::function<void(const json&)> callback);
      void sendRequest(const std::string& method, const json& params,
                       std::function<void(const json&)> callback);
      };

//---------------------------------------------------------
//   McpManager
//    Manager to hold all configured MCP servers
//---------------------------------------------------------

class McpManager : public QObject
      {
      Q_OBJECT
      McpManager(QObject* parent = nullptr) : QObject(parent) {}
      ~McpManager() override;

      std::map<QString, std::unique_ptr<McpServer>> m_servers;
      McpServerConfigs m_configs;

    signals:
      void toolsChanged();
      void resourcesChanged();

    public:
      static McpManager& instance();

      void applyConfigs(const McpServerConfigs& configs);

      void startAll();
      void stopAll();

      McpServer* getServer(const QString& id);
      };
