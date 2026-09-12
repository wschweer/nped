//============================================================================="
//  nped Program Editor
//
//  Copyright (C) 2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#pragma once

#include <QWidget>
#include <QList>
#include <QPlainTextEdit>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QImage>
// #include <map>
#include <QVBoxLayout>
#include <QDateTime>
#include <QProcess>
#include <atomic>
#include <functional>
#include <mutex>

#include "logger.h"
#include "types.h"
#include "llm.h"
#include "llminfo.h"
#include "dashboard.h"
#include "model.h"
#include "attachmentbutton.h"
#include "editor.h"

using string = std::string;

class QTextEdit;
class MarkdownWebView;

class QComboBox;
class QToolButton;
class QMenu;
class QNetworkAccessManager;
class QNetworkReply;
class QLabel;
class QTimer;
class LLMClient;
class ChatDisplay;
class Session;
class QEventLoop;
class ScreenshotHelper;
class McpManager;
class KeyLogger;

//---------------------------------------------------------
//   DropAwarePlainTextEdit
//   A QPlainTextEdit that intercepts image drag-and-drop
//   at the virtual-function level and emits imageDropped()
//   instead of inserting content into the text field.
//---------------------------------------------------------

class DropAwarePlainTextEdit : public QPlainTextEdit
      {
      Q_OBJECT

      std::vector<Action> textActions;
      KeyLogger* kl {nullptr};
      Editor* _editor;

    public:
      explicit DropAwarePlainTextEdit(Editor*, QWidget* parent = nullptr);

    signals:
      void imageDropped(const QImage& image);

    protected:
      void dragEnterEvent(QDragEnterEvent* e) override {
            const QMimeData* m = e->mimeData();
            Debug("[DropAware] dragEnterEvent — hasImage: {} hasUrls: {} formats: {}", m->hasImage(),
                m->hasUrls(), m->formats());
            if (m->hasImage() || (m->hasUrls() && !m->urls().isEmpty()))
                  e->acceptProposedAction();
            else
                  QPlainTextEdit::dragEnterEvent(e);
            }
      void dragMoveEvent(QDragMoveEvent* e) override {
            const QMimeData* m = e->mimeData();
            Debug("[DropAware] dragMoveEvent — hasImage: {} hasUrls: {}", m->hasImage(), m->hasUrls());
            if (m->hasImage() || (m->hasUrls() && !m->urls().isEmpty()))
                  e->acceptProposedAction();
            else
                  QPlainTextEdit::dragMoveEvent(e);
            }
      void dropEvent(QDropEvent* e) override {
            const QMimeData* m = e->mimeData();
            Debug("[DropAware] dropEvent — hasImage: {} hasUrls: {} formats: {}", m->hasImage(), m->hasUrls(),
                m->formats());
            QImage image;
            if (m->hasImage()) {
                  Debug("[DropAware] dropEvent — extracting image from imageData");
                  image = qvariant_cast<QImage>(m->imageData());
                  }
            else if (m->hasUrls()) {
                  for (const QUrl& url : m->urls()) {
                        Debug("[DropAware] dropEvent — trying URL: {}", url.toString().left(80));
                        if (url.isLocalFile()) {
                              QImage loaded(url.toLocalFile());
                              Debug("[DropAware] dropEvent — loaded local file, null: {}", loaded.isNull());
                              if (!loaded.isNull()) {
                                    image = loaded;
                                    break;
                                    }
                              }
                        else {
                              // Handle data: URIs dragged from a browser (e.g. "data:image/jpeg;base64,...")
                              const QString urlStr = url.toString();
                              if (urlStr.startsWith("data:image/")) {
                                    const int commaPos = urlStr.indexOf(',');
                                    if (commaPos != -1) {
                                          const QByteArray raw =
                                              QByteArray::fromBase64(urlStr.mid(commaPos + 1).toUtf8());
                                          if (image.loadFromData(raw)) {
                                                Debug("[DropAware] dropEvent — decoded data: URI image, "
                                                      "size: {}",
                                                    image.size());
                                                break;
                                                }
                                          }
                                    }
                              }
                        }
                  }
            if (!image.isNull()) {
                  Debug("[DropAware] dropEvent — emitting imageDropped, size: {}", image.size());
                  e->acceptProposedAction();
                  emit imageDropped(image);
                  }
            else {
                  Debug("[DropAware] dropEvent — no image found, delegating to base class");
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

      Q_PROPERTY(bool filterToolMessages MEMBER filterToolMessages)
      Q_PROPERTY(bool filterThoughts MEMBER filterThoughts)

      McpManager* _mcpManager;
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
      QToolButton* stopButton;

      QToolButton* configButton;
      QToolButton* screenshotButton;
      QWidget* buttonPanel {nullptr}; ///< narrow vertical icon panel left of prompt input
      QWidget* dataPanel {nullptr};   ///< narrow vertical icon panel right of prompt input
      QHBoxLayout* dataPanelLayout {nullptr};
      QWidget* promptActionPanel {nullptr};
      void updateDataPanel(); ///< rebuilds thumbnail labels for all pending images

      QTimer* spinnerTimer;
      int spinnerFrame {0};

      // Screenshot
      ScreenshotHelper* screenshotHelper {nullptr};
      QList<Attachment> _attachments;                  ///< pending files attached to prompt
      QList<AttachmentButton*> _attachmentIconButtons; ///< one thumbnail button per pending attachment

      QComboBox* agentRoleCombo {nullptr};

      // Netzwerk & Status
      QNetworkAccessManager* networkManager;
      QNetworkReply* currentReply {nullptr};
      Model model;
      LLMClient* llm {nullptr};

      QString _agentRoleName;
      bool isRetrying {false};
      bool _stopRequested {false};
      std::atomic<bool> _toolStopRequested {false}; ///< set by stop() to abort running tool
      QProcess* _currentToolProcess {nullptr};      ///< currently running QProcess (for kill on stop)
      std::mutex _toolProcessMutex;                 ///< protects _currentToolProcess
      int retryPause {2000};
      QToolButton* cannedPromptsButton {nullptr};
      QToolButton* addAttachmentButton {nullptr}; ///< "+" button to add attachments
      void addAttachment();                       ///< opens file dialog to attach any file
      QList<QToolButton*> _attachmentButtons;     ///< all attachment buttons (images and other files)
      int currentRetryCount {0};
      const int maxRetries {12};
      QDateTime rateLimitResetTime;

      bool _manifestsLoaded = false;
      std::string _manifestPlan;
      std::string _manifestBuild;

      ///< Metadata discovered from the LM provider (Ollama), persisted between
      ///< runs in llm_info.json and consulted by Session::contextBudget().
      LlmInfoStore _llmInfo;

      void mergeShowInfo(const QString& modelId, const json& j);
      void fetchModelDetails(const QStringList& ids);
      void fetchRuntimeContext();
      void refreshRuntimeContext();

      // Hilfsfunktionen
      void processData();
      std::vector<json> getMCPTools() const;
      QString pendingModelName;
      Session* _session;
      void reinitSystemPrompt(); // Punkt 4: implementiert
      QString truncateOutput(const QString& text, int maxChars);

      string formatSource(const QString& path);

      std::string extractVideoFrames(const QString& video_file, int start_number, int count, double interval);

      // Agenten-Tools & Pfad-Sicherheit
      std::string errorResponse(const std::string& message) const;
      bool isPathSafe(const QString& path);
      bool readFile(const QString& path, QString& result);
      string writeFile(const QString& path, const QString& content);
      std::string getFileOutline(const QString& file);
      std::string getDiagnostics(const QString& file);
      std::string findReferences(const QString& file, int line, int column);
      string listDirectory(const QString& path);
      string listFilesRecursive(const QString& path, int depth);
      string searchProject(const QString& query, const QString& filePattern);
      string findSymbol(const QString& symbol);
      string fetchWebDocumentation(const QString& urlString);
      std::string runValgrindCommand(const QString& executable, const QString& tool, const QString& args);
      std::string compressValgrindOutput(const QString& xmlPath);
      string runBashCommand(const QString& command);
      string replaceLines(const QString& path, int startLine, int linesToDelete, const QString& replaceText);
      string getGitStatus();
      string getGitDiff(const QString& path = "");
      string getGitLog(int limit = 5);
      string createGitCommit(const QString& message);
      QString normalizePath(const QString& path) const;

      void setInputEnabled(bool enabled);
      DropAwarePlainTextEdit* userInput;

      // ask_user tool: non-modal blocking via QEventLoop
      QString _userInputAnswer;

    protected:
      bool eventFilter(QObject* obj, QEvent* event) override;

    private slots:
      void fetchModels();
      void refreshLlmInfo();
      void handleChatReadyRead();
      void handleChatFinished();
      void updateSpinner();
      void startNewSession();
      void deleteCurrentSession();
      void renameCurrentSession();
      void onSessionSelected(int index);
      void onScreenshotReady(const QImage& image);
      void onScreenshotFailed(const QString& reason);
      void removeAttachment(int index);

      void stop();

    public slots:
      void sendMessage(QString);
      void sendMessage2();
      void updateStyle();
      void updateIcons();

    signals:
      void modelChanged();
      void attachmentClicked(int);

    public:
      explicit Agent(Editor* e, QWidget* parent = nullptr);
      ~Agent() {}
      ChatDisplay* chatDisplay;
      QAction* showToolMessageAction = nullptr;
      QAction* showThoughtsAction    = nullptr;

      void onAttachmentClicked();
      void onAttachmentSelected(int index);
      bool filterToolMessages = true;
      bool filterThoughts     = false;
      std::string getManifest() { return agentRole()->manifest.toStdString(); }
      std::string getProjectInstructions() { return _editor->getProjectInstructions(); }
      static QString configPath();
      QString currentModel() const { return model.name; }
      const Model& currentModelObj() const { return model; }
      ///< Discovered provider metadata for a model (invalid if unknown).
      LlmInfo llmInfoFor(const QString& modelId) const { return _llmInfo.get(modelId); }
      ///< Discovered provider metadata for the currently selected model.
      LlmInfo currentLlmInfo() const { return _llmInfo.get(model.modelIdentifier); }
      void setCurrentModel(const QString& s, bool clearChat = true);
      bool isExecuteMode() const { return agentRole()->rw; }
      bool isProtected() const {
            return model.protected_;
            } ///< true: tools run in sandbox; false: tools run on host
      bool isWorking() const;
      void logContent(const json& part, std::string& text, std::string& thought);
      std::string formatToolCall(const std::string& name, const json& args, const std::string& result = "");
      std::string executeTool(const std::string& functionName, const json& arguments);
      std::string executeToolImpl(const std::string& functionName, const json& arguments);
      std::string runInGuiThread(std::function<std::string()> fn);
      bool isToolStopped() const { return _toolStopRequested.load(); }
      // Output-Limits für LLM-Context-Window (Punkt 11)
      static constexpr int kBuildLogMaxChars  = 20000 * 10;
      static constexpr int kWebFetchMaxChars  = 80000;
      static constexpr int kGitDiffMaxChars   = 10000;
      static constexpr int kMaxAttachmentSize = 1024 * 1024 * 2; // 2MB limit

      static constexpr int kSearchMaxChars     = 10000;
      static constexpr int kChatResultMaxChars = 20000;
      static constexpr int kChatMaxMessages    = 40;

      std::string compressBuildLog(const std::string& rawLog);
      std::string truncateOutput(const std::string& text, int maxChars);
      Editor* editor() const { return _editor; }
      Session* session() const { return _session; }
      void updateSessionList();
      void updateChatDisplay(bool scrollToBottom = false);
      void addMessage(const std::string& role, const std::string& text);
      const AgentRole* agentRole() const;
      McpManager* mcpManager() const { return _mcpManager; }
      void startAgent();
      void stopAgent();
      void ensureMcpServersReady();
      };
