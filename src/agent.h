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
#include <QDebug>
#include <QList>
#include <QPlainTextEdit>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDateTime>
#include <QImage>
#include <map>
#include <QQuickWidget>
#include <QVBoxLayout>

#include "types.h"
#include "llm.h"
#include "dashboard.h"

class Editor;
class QTextEdit;
class MarkdownWebView;
// DropAwarePlainTextEdit is defined above – no forward declaration needed
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
class QEventLoop;
class ScreenshotHelper;

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
      // --- Advanced parameters (fully exposed to QML) ---
      Q_PROPERTY(bool supportsThinking MEMBER supportsThinking)
      Q_PROPERTY(double temperature MEMBER temperature)
      Q_PROPERTY(double topP MEMBER topP)
      Q_PROPERTY(int maxTokens MEMBER maxTokens)

    public:
      QString name;
      QString modelIdentifier;
      QString baseUrl;
      QString apiKey;
      QString api;                                       // "ollama", "gemini", "anthropic", "openai"
      bool isLocal                              = false; ///< true für Ollama
      bool supportsThinking                     = false; ///< true: model supports Extended Thinking (e.g. claude-3-7-sonnet)
      double temperature                        = -1.0;  ///< <0: use API default
      double topP                               = -1.0;  ///< <0: use API default
      int maxTokens                             = -1;    ///< <0: use per-client default
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
//   DropAwarePlainTextEdit
//   A QPlainTextEdit that intercepts image drag-and-drop
//   at the virtual-function level and emits imageDropped()
//   instead of inserting content into the text field.
//---------------------------------------------------------

class DropAwarePlainTextEdit : public QPlainTextEdit
      {
      Q_OBJECT
    public:
      explicit DropAwarePlainTextEdit(QWidget* parent = nullptr)
            : QPlainTextEdit(parent) {}

    signals:
      void imageDropped(const QImage& image);

    protected:
      void dragEnterEvent(QDragEnterEvent* e) override {
            const QMimeData* m = e->mimeData();
            qDebug() << "[DropAware] dragEnterEvent — hasImage:" << m->hasImage()
                     << "hasUrls:" << m->hasUrls()
                     << "formats:" << m->formats();
            if (m->hasImage() || (m->hasUrls() && !m->urls().isEmpty()))
                  e->acceptProposedAction();
            else
                  QPlainTextEdit::dragEnterEvent(e);
            }

      void dragMoveEvent(QDragMoveEvent* e) override {
            const QMimeData* m = e->mimeData();
            qDebug() << "[DropAware] dragMoveEvent — hasImage:" << m->hasImage()
                     << "hasUrls:" << m->hasUrls();
            if (m->hasImage() || (m->hasUrls() && !m->urls().isEmpty()))
                  e->acceptProposedAction();
            else
                  QPlainTextEdit::dragMoveEvent(e);
            }

      void dropEvent(QDropEvent* e) override {
            const QMimeData* m = e->mimeData();
            qDebug() << "[DropAware] dropEvent — hasImage:" << m->hasImage()
                     << "hasUrls:" << m->hasUrls()
                     << "formats:" << m->formats();
            QImage image;
            if (m->hasImage()) {
                  qDebug() << "[DropAware] dropEvent — extracting image from imageData";
                  image = qvariant_cast<QImage>(m->imageData());
                  }
            else if (m->hasUrls()) {
                  for (const QUrl& url : m->urls()) {
                        qDebug() << "[DropAware] dropEvent — trying URL:" << url.toString().left(80);
                        if (url.isLocalFile()) {
                              QImage loaded(url.toLocalFile());
                              qDebug() << "[DropAware] dropEvent — loaded local file, null:" << loaded.isNull();
                              if (!loaded.isNull()) { image = loaded; break; }
                              }
                        else {
                              // Handle data: URIs dragged from a browser (e.g. "data:image/jpeg;base64,...")
                              const QString urlStr = url.toString();
                              if (urlStr.startsWith("data:image/")) {
                                    const int commaPos = urlStr.indexOf(',');
                                    if (commaPos != -1) {
                                          const QByteArray raw = QByteArray::fromBase64(
                                                urlStr.mid(commaPos + 1).toUtf8());
                                          if (image.loadFromData(raw)) {
                                                qDebug() << "[DropAware] dropEvent — decoded data: URI image, size:" << image.size();
                                                break;
                                                }
                                          }
                                    }
                              }
                        }
                  }
            if (!image.isNull()) {
                  qDebug() << "[DropAware] dropEvent — emitting imageDropped, size:" << image.size();
                  e->acceptProposedAction();
                  emit imageDropped(image);
                  }
            else {
                  qDebug() << "[DropAware] dropEvent — no image found, delegating to base class";
                  QPlainTextEdit::dropEvent(e);
                  }
            }
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
      Q_PROPERTY(bool filterToolMessages MEMBER filterToolMessages)
      Q_PROPERTY(bool filterThoughts MEMBER filterThoughts)

      // Stylesheet-Konstanten für Plan/Build-Button (Punkt 6)
      static const QString kPlanStyle;
      static const QString kBuildStyle;

      QByteArray streamBuffer;
      std::vector<json> mcpTools;
      Editor* _editor;
      Dashboard* dashboard;
      QToolButton* newSessionButton;
      QToolButton* deleteSessionButton;
      QToolButton* renameSessionButton;
      QComboBox* sessionComboBox;
      QComboBox* modelMenu;
      QToolButton* statusLabel;
      QToolButton* modeButton;
      QToolButton* configButton;
      QToolButton* screenshotButton;
      QWidget* dataPanel{nullptr};          ///< narrow vertical icon panel left of prompt input
      void updateDataPanel();               ///< rebuilds thumbnail labels for all pending images

      QTimer* spinnerTimer;
      int spinnerFrame{0};

      // Screenshot
      ScreenshotHelper* screenshotHelper{nullptr};
      QList<QString>    _pendingImages;       ///< base64-encoded JPEG strings, one per attached image/screenshot
      QVBoxLayout*      _dataPanelLayout{nullptr};
      QList<QLabel*>    _imageIconLabels;     ///< one thumbnail label per pending image

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
      size_t savedEntries{0};
      //      bool commitGitChanges(const QString& commitMessage);
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
      DropAwarePlainTextEdit* userInput;

      // ask_user tool: non-modal blocking via QEventLoop
      bool _waitingForUserInput{false};
      QString _userInputAnswer;

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
      void renameCurrentSession();
      void onSessionSelected(int index);
      void onScreenshotReady(const QImage& image);
      void onScreenshotFailed(const QString& reason);

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
      QAction* showToolMessageAction = nullptr;
      QAction* showThoughtsAction     = nullptr;
      HistoryManager* historyManager;

      bool filterToolMessages = false;
      bool filterThoughts = false;

    private:
      bool _manifestsLoaded = false;
      std::string _manifestPlan;
      std::string _manifestBuild;

    public:
      std::string getManifest();

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
      static constexpr int kChatMaxMessages    = 40;
      std::string truncateOutput(const std::string& text, int maxChars);
      };
