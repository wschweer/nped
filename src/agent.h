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

class Editor;
class ConfigDialog;
class QTextEdit;
class QPlainTextEdit;
class QToolBar;
class QToolButton;
class QMenu;
class QNetworkAccessManager;
class QNetworkReply;
class QLabel;
class QTimer;
class QAction;

//---------------------------------------------------------
//   Model
//---------------------------------------------------------

struct Model {
      QString name;
      QString modelIdentifier;
      QString baseUrl;
      QString apiKey;
      QString interface; // "ollama", "gemini", "anthropic"
      bool isLocal;      // true für Ollama
      };

using Models = QList<Model>;

//---------------------------------------------------------
//   Agent
//---------------------------------------------------------

class Agent : public QWidget
      {
      Q_OBJECT

      // Output-Limits für LLM-Context-Window (Punkt 11)
      static constexpr int kBuildLogMaxChars   = 4000;
      static constexpr int kWebFetchMaxChars   = 8000;
      static constexpr int kGitDiffMaxChars    = 6000;
      static constexpr int kSearchMaxChars     = 4000;
      static constexpr int kChatResultMaxChars = 500;

      // Stylesheet-Konstanten für Plan/Build-Button (Punkt 6)
      static const QString kPlanStyle;
      static const QString kBuildStyle;

      Editor* _editor;
      ConfigDialog* configDialog;
      QToolBar* toolBar;
      QToolButton* modelButton;
      QToolButton* newSessionButton;
      QMenu* modelMenu;
      QTextEdit* chatDisplay;
      QLabel* statusLabel;
      QPlainTextEdit* userInput;
      QAction* modeToggleAction;
      QToolButton* configButton;
      QTimer* spinnerTimer;
      int spinnerFrame{0};

      // Netzwerk & Status
      QNetworkAccessManager* networkManager;
      QNetworkReply* currentReply;
      Model model;

      bool isExecuteMode{false};
      bool isRetrying{false};
      int retryPause{2000};
      int currentRetryCount{0};
      const int maxRetries{5};
      QDateTime rateLimitResetTime;

      // Agenten-Daten & Streaming-Puffer
      nlohmann::json chatHistory;
      nlohmann::json toolsDefinition;
      QByteArray streamBuffer;

      std::string currentAssistantMessage;
      json currentToolCalls;
      std::map<int, nlohmann::json> openAIToolCallAssembler;
      std::map<int, json> anthropicToolCallAssembler;
      QByteArray currentAnthropicEventType;

      Models _models; // Punkt 9: private mit Getter

      // Hilfsfunktionen
      void setInputEnabled(bool enabled);
      void sendChatRequest();
      json getToolsDefinition();
      QString currentSessionFileName;
      QString generateSessionFileName();
      bool commitGitChanges(const QString& commitMessage);
      void reinitSystemPrompt(); // Punkt 4: implementiert
      void updateChatDisplay();
      void trimHistory();

      // Tool-Handler (Punkt 8: aus executeTool ausgelagert)
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
      QString executeTool(const std::string& functionName, const json& arguments);
      QString readFile(const QString& path);
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

    protected:
      bool eventFilter(QObject* obj, QEvent* event) override;

    private slots:
      void fetchModels();
      void sendMessage();
      void handleChatReadyRead();
      void handleChatFinished();
      void updateSpinner();
      void startNewSession();
      void openConfigDialog();

    signals:
      void modelChanged();

    public:
      explicit Agent(Editor* e, QWidget* parent = nullptr);

      // Punkt 5: configPath als static Methode
      static QString configPath();
      // Punkt 9: Getter/Setter für _models
      Models& models() { return _models; }
      const Models& models() const { return _models; }
      void connectToServer(const QString& url = "http://localhost:11434");
      QString currentModel() const { return model.name; }
      void setCurrentModel(const QString& s);
      void saveSettings();
      void loadSettings();
      void saveStatus();
      void loadStatus();
      bool isWorking() const;
      };
