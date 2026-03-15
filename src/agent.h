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

#include "types.h"
#include "llm.h"

class Editor;
class QTextEdit;
class MarkdownWebView;
class QPlainTextEdit;
class QToolBar;
class QToolButton;
class QMenu;
class QNetworkAccessManager;
class QNetworkReply;
class QLabel;
class QTimer;
class QAction;
class LLMClient;
class ChatDisplay;

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
      QString name;
      QString modelIdentifier;
      QString baseUrl;
      QString apiKey;
      QString api;  // "ollama", "gemini", "anthropic", "openai"
      bool isLocal; // true für Ollama
      };

using Models = QList<Model>;

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

      // Stylesheet-Konstanten für Plan/Build-Button (Punkt 6)
      static const QString kPlanStyle;
      static const QString kBuildStyle;

      std::vector<json> mcpTools;
      Editor* _editor;
      QToolBar* toolBar;
      QToolButton* modelButton;
      QToolButton* newSessionButton;
      QMenu* modelMenu;
      QLabel* statusLabel;
      QAction* modeToggleAction;
      QToolButton* configButton;
      QTimer* spinnerTimer;
      int spinnerFrame{0};

      // Netzwerk & Status
      QNetworkAccessManager* networkManager;
      QNetworkReply* currentReply;
      Model model;
      LLMClient* llm{nullptr};

      bool isExecuteMode{false};
      bool isRetrying{false};
      int retryPause{2000};
      int currentRetryCount{0};
      const int maxRetries{5};
      QDateTime rateLimitResetTime;

      Models _models;

      // Hilfsfunktionen
      std::vector<json> getMCPTools() const;
      QString currentSessionFileName;
      bool commitGitChanges(const QString& commitMessage);
      void reinitSystemPrompt(); // Punkt 4: implementiert
      void updateChatDisplay();
      void trimHistory();
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

    private slots:
      void fetchModels();
      void sendMessage();
      void handleChatReadyRead();
      void handleChatFinished();
      void updateSpinner();
      void startNewSession();

    public slots:
      void sendMessage2();

    signals:
      void modelChanged();

    public:
      explicit Agent(Editor* e, QWidget* parent = nullptr);

      ChatDisplay* chatDisplay;
      json chatHistory; // this is an array of Content objects

      std::string getManifest() const;

      static QString configPath();
      Models& models() { return _models; }
      const Models& models() const { return _models; }
      QString currentModel() const { return model.name; }
      void setCurrentModel(const QString& s);

      void saveSettings();
      void loadSettings();
      void saveStatus();
      void loadStatus();
      bool isWorking() const;
      void logContent(const json& part, std::string& text, std::string& thought);
      std::string executeTool(const std::string& functionName, const json& arguments);
      void enableInput(bool);

      // Output-Limits für LLM-Context-Window (Punkt 11)
      static constexpr int kBuildLogMaxChars   = 4000;
      static constexpr int kWebFetchMaxChars   = 8000;
      static constexpr int kGitDiffMaxChars    = 6000;
      static constexpr int kSearchMaxChars     = 4000;
      static constexpr int kChatResultMaxChars = 500;
      static constexpr int kChatMaxMessages    = 50;
      std::string truncateOutput(const std::string& text, int maxChars);
      };
