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
#include <map>
#include <QVBoxLayout>
#include <QDateTime>

#include "logger.h"
#include "types.h"
#include "llm.h"
#include "dashboard.h"
#include "model.h"
#include "attachmentbutton.h"

using string = std::string;

class Editor;
class QTextEdit;
class MarkdownWebView;

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
class Session;
class QEventLoop;
class ScreenshotHelper;
class McpManager;
//---------------------------------------------------------
//   AgentRole
//---------------------------------------------------------

struct AgentRole {
      Q_GADGET
      Q_PROPERTY(QString name MEMBER name)
      Q_PROPERTY(QString manifest MEMBER manifest)
      Q_PROPERTY(bool rw MEMBER rw)
      Q_PROPERTY(QStringList mcpServers MEMBER mcpServers)

    public:
      QString name;
      QString manifest;
      bool rw; // true: read/write, false: read only
      QStringList mcpServers;

      bool operator==(const AgentRole& other) const = default;
};
using AgentRoles = QList<AgentRole>;
//---------------------------------------------------------
//   CannedPrompt
//---------------------------------------------------------

struct CannedPrompt {
      Q_GADGET
      Q_PROPERTY(QString name MEMBER name)
      Q_PROPERTY(QString description MEMBER description)
      Q_PROPERTY(QString prompt MEMBER prompt)

    public:
      QString name, description, prompt;
      bool operator==(const CannedPrompt& other) const = default;
};
using CannedPrompts = QList<CannedPrompt>;
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
      explicit DropAwarePlainTextEdit(QWidget* parent = nullptr) : QPlainTextEdit(parent) {}
    signals:
      void imageDropped(const QImage& image);

    protected:
      void dragEnterEvent(QDragEnterEvent* e) override {
            const QMimeData* m = e->mimeData();
            qDebug() << "[DropAware] dragEnterEvent — hasImage:" << m->hasImage()
                     << "hasUrls:" << m->hasUrls() << "formats:" << m->formats();
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
            qDebug() << "[DropAware] dropEvent — hasImage:" << m->hasImage() << "hasUrls:" << m->hasUrls()
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
                              qDebug()
                                  << "[DropAware] dropEvent — loaded local file, null:" << loaded.isNull();
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
                                                qDebug() << "[DropAware] dropEvent — decoded data: URI "
                                                            "image, size:"
                                                         << image.size();
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

      Q_PROPERTY(bool filterToolMessages MEMBER filterToolMessages)
      Q_PROPERTY(bool filterThoughts MEMBER filterThoughts)

      // Stylesheet-Konstanten für Plan/Build-Button (Punkt 6)
      static const QString kPlanStyle;
      static const QString kBuildStyle;

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

      // Hilfsfunktionen
      void processData();
      std::vector<json> getMCPTools() const;
      QString pendingModelName;
      Session* _session;
      void reinitSystemPrompt(); // Punkt 4: implementiert
      QString truncateOutput(const QString& text, int maxChars);

      QString handleRunBuildCommand(const json& args);
      QString handleFetchWebDocumentation(const json& args);
      QString handleGetGitStatus(const json& args);
      QString handleGetGitDiff(const json& args);
      QString handleGetGitLog(const json& args);
      string formatSource(const QString& path);
      QString handleCreateGitCommit(const json& args);
      QString handleSearchProject(const json& args);
      QString handleFindSymbol(const json& args);
      QString handleReadFile(const json& args);
      QString handleReadFileLines(const json& args);
      QString handleListDirectory(const json& args);
      QString handleWriteFile(const json& args);
      QString handleReplaceInFile(const json& args);

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
      static QString configPath();
      QString currentModel() const { return model.name; }
      void setCurrentModel(const QString& s, bool clearChat = true);
      bool isExecuteMode() const { return agentRole()->rw; }
      bool isWorking() const;
      void logContent(const json& part, std::string& text, std::string& thought);
      std::string formatToolCall(const std::string& name, const json& args, const std::string& result = "");
      std::string executeTool(const std::string& functionName, const json& arguments);

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
};

Q_DECLARE_METATYPE(AgentRole)
Q_DECLARE_METATYPE(AgentRoles)

Q_DECLARE_METATYPE(CannedPrompt)
Q_DECLARE_METATYPE(CannedPrompts)
