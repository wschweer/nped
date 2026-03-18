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

#pragma once

#include <QWidget>
#include <QList>
#include <QDateTime>
#include <map>
#include <QQuickWidget>

#include "types.h"
#include "llm.h"

class Editor;
class QTextEdit;
class MarkdownWebView;
class QPlainTextEdit;
class QToolBar;
class QComboBox;
class QToolButton;
class QMenu;
class QNetworkAccessManager;
class QNetworkReply;
class QLabel;
class QTimer;
class QAction;
class LLMClient;
class ChatDisplay;
class HistoryManager;

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
      MCPToolBuilder& add_parameter(const std::string& name, const std::string& type, const std::string& description, bool required = true,
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
//   Model
//---------------------------------------------------------

struct Model {
      Q_GADGET
      Q_PROPERTY(QString name MEMBER name)
      Q_PROPERTY(QString modelIdentifier MEMBER modelIdentifier)
      Q_PROPERTY(QString baseUrl MEMBER baseUrl)
      Q_PROPERTY(QString apiKey MEMBER apiKey)
      Q_PROPERTY(QString api MEMBER api)
      Q_PROPERTY(bool isLocal MEMBER isLocal)

    public:
      QString name;
      QString modelIdentifier;
      QString baseUrl;
      QString apiKey;
      QString api;  // "ollama", "gemini", "anthropic", "openai"
      bool isLocal; // true für Ollama
      bool operator==(const Model& other) const = default;
      };

using Models = QList<Model>;
Q_DECLARE_METATYPE(Model)

//---------------------------------------------------------
//   SessionInfo
//---------------------------------------------------------

struct SessionInfo {
      QString fileName;
      int lastNumber = 0;
      QDate lastDate;
      };

//---------------------------------------------------------
//   Agent
//---------------------------------------------------------

class Agent : public QWidget
      {
      Q_OBJECT
      QML_ELEMENT
      QML_UNCREATABLE("no")

      Q_PROPERTY(Models models READ models WRITE setModels NOTIFY modelsChanged)
      Q_PROPERTY(Models filteredModels READ filteredModels NOTIFY modelsChanged)

      // Stylesheet-Konstanten für Plan/Build-Button (Punkt 6)
      static const QString kPlanStyle;
      static const QString kBuildStyle;

      QByteArray streamBuffer;
      std::vector<json> mcpTools;
      Editor* _editor;
      QToolBar* toolBar;
      QToolButton* newSessionButton;
      QToolButton* deleteSessionButton;
      QComboBox* sessionComboBox;
      QComboBox* modelMenu;
      QLabel* statusLabel;
      QToolButton* modeButton;
      QToolButton* configButton;
      QTimer* spinnerTimer;
      int spinnerFrame{0};

      // Netzwerk & Status
      QNetworkAccessManager* networkManager;
      QNetworkReply* currentReply{nullptr};
      Model model;
      LLMClient* llm{nullptr};

      bool _isExecuteMode{true};
      bool isRetrying{false};
      int retryPause{2000};
      int currentRetryCount{0};
      const int maxRetries{5};
      QDateTime rateLimitResetTime;

      Models _models;

      // Hilfsfunktionen
      void processData();
      std::vector<json> getMCPTools() const;
      QString currentSessionFileName;
      QString pendingModelName;
      bool commitGitChanges(const QString& commitMessage);
      void reinitSystemPrompt(); // Punkt 4: implementiert
      void updateChatDisplay();
      QString truncateOutput(const QString& text, int maxChars);
      SessionInfo getSessionInfo() const;
      QString sessionName(bool getNext) const;

      QString handleRunBuildCommand(const json& args);
      QString handleFetchWebDocumentation(const json& args);
      QString handleGetGitStatus(const json& args);
      QString handleGetGitDiff(const json& args);
      QString handleGetGitLog(const json& args);
      QString handleCreateGitCommit(const json& args);
      QString handleSearchProject(const json& args);
      QString handleFindSymbol(const json& args);
      QString handleReadFile(const json& args);
      QString handleReadFileLines(const json& args);
      QString handleListDirectory(const json& args);
      QString handleCreateFile(const json& args);
      QString handleModifyFile(const json& args);
      QString handleReplaceInFile(const json& args);

      // Agenten-Tools & Pfad-Sicherheit
      bool isPathSafe(const QString& path);
      bool readFile(const QString& path, QString& result);
      QString readFileLines(const QString& path, int startLine, int endLine);
      QString createFile(const QString& path, const QString& content);
      QString modifyFile(const QString& path, const QString& content);
      QString listDirectory(const QString& path);
      QString searchProject(const QString& query, const QString& filePattern);
      QString findSymbol(const QString& symbol);
      QString fetchWebDocumentation(const QString& urlString);
      QString runBuildCommand(const QString& command);
      QString replaceInFile(const QString& path, const QString& searchStr, const QString& replaceStr);
      QString getGitStatus();
      QString getGitDiff(const QString& path = "");
      QString getGitLog(int limit = 5);
      QString createGitCommit(const QString& message);

      void setInputEnabled(bool enabled);
      QPlainTextEdit* userInput;

    protected:
      bool eventFilter(QObject* obj, QEvent* event) override;
      void updateSessionList();

    private slots:
      void fetchModels();
      void handleChatReadyRead();
      void handleChatFinished();
      void updateSpinner();
      void startNewSession();
      void deleteCurrentSession();
      void onSessionSelected(int index);

    public slots:
      void sendMessage(QString);
      void sendMessage2();

    signals:
      void modelChanged();
      void modelsChanged();

    public:
      explicit Agent(Editor* e, QWidget* parent = nullptr);
      ~Agent();

      ChatDisplay* chatDisplay;
      HistoryManager* historyManager;

      std::string getManifest() const;

      static QString configPath();
      Models& models() { return _models; }
      const Models& models() const { return _models; }
      Models filteredModels() const {
            Models result;
            for (const auto& m : _models)
                  if (!m.isLocal)
                        result.append(m);
            return result;
            }
      void setModels(const Models& m) {
            if (_models == m)
                  return;
            _models = m;
            saveSettings();
            emit modelsChanged();
            }
      QString currentModel() const { return model.name; }
      void setCurrentModel(const QString& s, bool clearChat = true);
      bool isExecuteMode() const { return _isExecuteMode; }
      void setExecuteMode(bool);

      void saveSettings();
      void loadSettings();
      void saveStatus();
      void loadStatus(const QString& sessionPath = QString());
      bool isWorking() const;
      void logContent(const json& part, std::string& text, std::string& thought);
      std::string formatToolCall(const std::string& name, const json& args, const std::string& result = "");
      std::string executeTool(const std::string& functionName, const json& arguments);
      void enableInput(bool);

      // Output-Limits für LLM-Context-Window (Punkt 11)
      static constexpr int kBuildLogMaxChars   = 20000;
      static constexpr int kWebFetchMaxChars   = 80000;
      static constexpr int kGitDiffMaxChars    = 10000;
      static constexpr int kSearchMaxChars     = 10000;
      static constexpr int kChatResultMaxChars = 20000;
      static constexpr int kChatMaxMessages    = 500;
      std::string truncateOutput(const std::string& text, int maxChars);
      };
