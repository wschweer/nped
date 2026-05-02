//=============================================================================
//  nped Program Editor
//
//  Copyright (C) 2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkRequest>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QFile>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QComboBox>
#include <QProxyStyle>
#include <QStyleOptionComboBox>
#include <QPainter>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLabel>
#include <QTimer>
#include <QAction>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QTextStream>
#include <QEventLoop>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStyle>
#include <QFileDialog>
#include <QImageReader>
#include <QMimeDatabase>
#include <QDebug>
#include <QBuffer>
#include <QPixmap>
#include <QDragEnterEvent>
#include "mcp.h"
#include <QDropEvent>
#include <QMimeData>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>

#include <QApplication>
#include <QGuiApplication>
#include "agent.h"
#include "logger.h"
#include "editor.h"
#include "llm.h"
#include "chatdisplay.h"
#include "session.h"
#include "screenshot.h"
#include "undo.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define AI true // dump AI input/output for debugging

//---------------------------------------------------------
//   Agent (Constructor)
//---------------------------------------------------------

Agent::Agent(Editor* e, QWidget* parent) : QWidget(parent), _editor(e) {
      networkManager = new QNetworkAccessManager(this);
      _mcpManager    = new McpManager(_editor, this);
      _session       = new Session(this, this);

      QVBoxLayout* mainLayout = new QVBoxLayout(this);

      // Add Dashboard
      dashboard = new Dashboard(this);
      dashboard->setObjectName("dashboardWidget");
      mainLayout->addWidget(dashboard);

      // Run button – leftmost item in the toolbar
      statusLabel = new QToolButton(this);
      statusLabel->setText(">");
      statusLabel->setMinimumWidth(30);
      statusLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
      connect(statusLabel, &QToolButton::clicked, [this] {
            QString s = userInput->toPlainText();
            if (!s.isEmpty()) {
                  sendMessage(s);
                  userInput->clear();
                  }
            });

      stopButton = new QToolButton(this);
      stopButton->setIcon(QIcon(_editor->darkMode() ? ":images/stop_white.svg" : ":images/stop.svg"));
      stopButton->setToolTip("Stop AI");
      stopButton->setMinimumWidth(30);
      stopButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
      connect(stopButton, &QToolButton::clicked, this, &Agent::stop);

      modelMenu = new QComboBox(this);
      modelMenu->setMinimumWidth(210);
      connect(_editor, &Editor::modelsChanged, [this] {
            bool blocked = modelMenu->blockSignals(true);
            modelMenu->clear();
            for (const auto& m : _editor->models())
                  modelMenu->addItem(m.name);
            if (!model.name.isEmpty())
                  modelMenu->setCurrentText(model.name);
            modelMenu->blockSignals(blocked);
            });

      connect(modelMenu, &QComboBox::activated,
              [this](int index) { setCurrentModel(_editor->models()[index].name); });

      sessionComboBox = new QComboBox(this);
      sessionComboBox->setMinimumWidth(166);
      connect(sessionComboBox, &QComboBox::activated, this, &Agent::onSessionSelected);
      updateSessionList();

      newSessionButton = new QToolButton(this);
      newSessionButton->setText("+");
      newSessionButton->setToolTip("New Session");
      connect(newSessionButton, &QToolButton::clicked, this, &Agent::startNewSession);

      deleteSessionButton = new QToolButton(this);
      deleteSessionButton->setObjectName("deleteSessionButton");
      deleteSessionButton->setToolTip("Delete Session");
      connect(deleteSessionButton, &QToolButton::clicked, this, &Agent::deleteCurrentSession);

      renameSessionButton = new QToolButton(this);
      renameSessionButton->setText("✎");
      renameSessionButton->setToolTip("Rename Session");
      connect(renameSessionButton, &QToolButton::clicked, this, &Agent::renameCurrentSession);

      agentRoleCombo = new QComboBox(this);
      for (const auto& r : _editor->agentRoles())
            agentRoleCombo->addItem(r.name);
      agentRoleCombo->setToolTip("Agent Role");
      auto updateAgentRoleCombo = [this] {
            agentRoleCombo->clear();
            int idx          = 0;
            int currentIndex = 0;
            for (const auto& r : _editor->agentRoles()) {
                  agentRoleCombo->addItem(r.name);
                  if (r.name == _editor->agentRoleName())
                        currentIndex = idx;
                  ++idx;
                  }
            agentRoleCombo->setCurrentIndex(currentIndex);
            };
      connect(_editor, &Editor::agentRolesChanged, updateAgentRoleCombo);
      updateAgentRoleCombo();

      dashboard->addWidget(stopButton);
      dashboard->addWidget(statusLabel);
      dashboard->addWidget(agentRoleCombo, 0);
      dashboard->addStretch(0);
      dashboard->addWidget(renameSessionButton, 0);
      dashboard->addWidget(deleteSessionButton, 0);
      dashboard->addWidget(newSessionButton, 0);
      dashboard->addWidget(sessionComboBox, 0);
      dashboard->addWidget(modelMenu, 0);

      connect(agentRoleCombo, &QComboBox::activated, [this] {
            _editor->setAgentRoleName(agentRole()->name);
            // if we change the agent role the available tools may change
            mcpTools = getMCPTools();
            if (llm)
                  llm->setTools(mcpTools);
            addMessage("system", std::format("<br><i>[System: Role changed to <b>{}</b>]</i>",
                                             _editor->agentRoleName()));
            });
      connect(_mcpManager, &McpManager::toolsChanged, [this] {
            mcpTools = getMCPTools();
            if (llm)
                  llm->setTools(mcpTools);
            });
      mcpTools = getMCPTools();

      // Filter toggle buttons (icon-only, no pulldown menu needed)
      showToolMessageAction = new QAction(this);
      showToolMessageAction->setCheckable(true);
      showToolMessageAction->setChecked(!filterToolMessages);
      showToolMessageAction->setToolTip("Show Tool Messages");
      showToolMessageAction->setIcon(
          QIcon(_editor->darkMode() ? ":images/tool-dark.svg" : ":images/tool.svg"));
      connect(showToolMessageAction, &QAction::toggled, [this](bool checked) {
            filterToolMessages = !checked;
            session()->save();
            updateChatDisplay();
            chatDisplay->scrollToBottom();
            });
      dashboard->addAction(showToolMessageAction, 1);

      showThoughtsAction = new QAction(this);
      showThoughtsAction->setIcon(
          QIcon(_editor->darkMode() ? ":images/thinking-dark.svg" : ":images/thinking.svg"));
      showThoughtsAction->setCheckable(true);
      showThoughtsAction->setChecked(!filterThoughts);
      showThoughtsAction->setToolTip("Show Thoughts");
      connect(showThoughtsAction, &QAction::toggled, [this](bool checked) {
            filterThoughts = !checked;
            session()->save();
            updateChatDisplay();
            chatDisplay->scrollToBottom();
            });
      dashboard->addAction(showThoughtsAction, 1);

      // Screenshot button
      screenshotButton = new QToolButton(this);
      connect(_editor, &Editor::fontChanged, [this](QFont f) { screenshotButton->setFont(f); });
      screenshotButton->setToolTip("Take Screenshot and attach to next prompt");
      screenshotButton->setCheckable(true);
      screenshotButton->setChecked(false);
      screenshotButton->setIcon(
          QIcon(_editor->darkMode() ? ":images/camera-dark.svg" : ":images/camera.svg"));
      dashboard->addWidget(screenshotButton, 1);

      screenshotHelper = new ScreenshotHelper(this);
      connect(screenshotHelper, &ScreenshotHelper::screenshotReady, this, &Agent::onScreenshotReady);
      connect(_editor, &Editor::screenshotReady, this, &Agent::onScreenshotReady);
      connect(screenshotHelper, &ScreenshotHelper::screenshotFailed, this, &Agent::onScreenshotFailed);

      connect(screenshotButton, &QToolButton::clicked, [this](bool) {
            // If images are already attached, discard all of them
            if (!_attachments.isEmpty()) {
                  _attachments.clear();
                  screenshotButton->setChecked(false);
                  screenshotButton->setToolTip("Take Screenshot and attach to next prompt");
                  addMessage("system", "<i>[All attached files discarded]</i><br>");
                  updateDataPanel();
                  return;
                  }
            screenshotHelper->takeScreenshot();
            });

      // Initialize token count
      dashboard->setTokenCount(session()->totalTokens);
      // Connect token updates
      connect(session(), &Session::tokensChanged,
              [this](size_t tokens) { dashboard->setTokenCount(tokens); });

      auto updateIcons = [this]() {
            QColor fg = _editor->textStyle(TextStyle::Normal).fg;
            showThoughtsAction->setIcon(Editor::createStatefulIcon(":images/thinking.svg", fg, fg, fg));
            showToolMessageAction->setIcon(Editor::createStatefulIcon(":images/tool.svg", fg, fg, fg));
            screenshotButton->setIcon(Editor::createStatefulIcon(":images/camera.svg", fg, fg, fg));
            stopButton->setIcon(Editor::createStatefulIcon(":images/stop.svg", fg, fg, fg));
            };

      connect(_editor, &Editor::darkModeChanged, updateIcons);
      connect(_editor, &Editor::textStylesLightChanged, updateIcons);
      connect(_editor, &Editor::textStylesDarkChanged, updateIcons);
      updateIcons();

      // --- 2. Chat Display ---
      chatDisplay = new ChatDisplay(_editor, parent);
      connect(_editor, &Editor::scaleChanged, [this] { chatDisplay->setZoomFactor(1.2 * _editor->scale()); });
      chatDisplay->setZoomFactor(1.2 * _editor->scale());
      chatDisplay->setDarkMode(_editor->darkMode());
      chatDisplay->setup();
      mainLayout->addWidget(chatDisplay->widget(), 1); // stretch=1: nimmt den gesamten verbleibenden Platz
      connect(_editor, &Editor::darkModeChanged, chatDisplay, &ChatDisplay::setDarkMode);

      // 3b. Prompt input field (zuerst anlegen, damit die Höhe bekannt ist)
      userInput = new DropAwarePlainTextEdit(this);
      userInput->setAcceptDrops(true);
      userInput->setCursorWidth(8);
      userInput->setPlaceholderText("enter message to LLM... Ctrl+Enter for send");
      userInput->setStyleSheet("QPlainTextEdit { padding: 5px; }");
      QFontMetrics fm(userInput->font());
      const int inputHeight = (fm.lineSpacing() * 4) + 20;
      userInput->setFixedHeight(inputHeight);
      userInput->installEventFilter(this);
      connect(userInput, &DropAwarePlainTextEdit::imageDropped, this, &Agent::onScreenshotReady);

      // 3a. Horizontale Leiste für Attachements
      dataPanel = new QWidget(this);
      dataPanel->setFixedHeight(48); // Feste Höhe
      dataPanelLayout = new QHBoxLayout(dataPanel);
      dataPanelLayout->setContentsMargins(2, 2, 2, 2);
      dataPanelLayout->setSpacing(4);
      dataPanelLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

      // "+" button to add attachments
      addAttachmentButton = new QToolButton(dataPanel);
      addAttachmentButton->setText("+");
      addAttachmentButton->setFixedSize(36, 36);
      addAttachmentButton->setStyleSheet("QToolButton { border: 1px solid #555; border-radius: 4px; "
                                         "background: transparent; font-size: 18px; }"
                                         "QToolButton:hover { background: #3a3a3a; }");
      addAttachmentButton->setToolTip("Add attachment");
      connect(addAttachmentButton, &QToolButton::clicked, this, &Agent::addAttachment);
      dataPanelLayout->addWidget(addAttachmentButton);

      dataPanelLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

      // 3a. Schmales vertikales Icon-Panel links neben dem Prompt-Eingabefeld
      buttonPanel = new QWidget(this);
      buttonPanel->setFixedHeight(inputHeight); // exakt so hoch wie das Eingabefeld
      QVBoxLayout* buttonPanelLayout = new QVBoxLayout(buttonPanel);
      buttonPanelLayout->setContentsMargins(2, 2, 2, 2);
      buttonPanelLayout->setSpacing(4);
      buttonPanelLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

      // New summary buttons (vertical bar)
      button1 = new QToolButton(this);
      button1->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); // kein vertikales Wachstum
      button1->setText("F1");
      button1->setToolTip("Canned prompt F1");
      connect(button1, &QToolButton::clicked, [this] { handleCannedPrompt("F1"); });
      buttonPanelLayout->addWidget(button1);

      button2 = new QToolButton(this);
      button2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); // kein vertikales Wachstum
      button2->setText("F2");
      button2->setToolTip("Canned prompt F2");
      connect(button2, &QToolButton::clicked, [this] { handleCannedPrompt("F2"); });
      buttonPanelLayout->addWidget(button2);

      button3 = new QToolButton(this);
      button3->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); // kein vertikales Wachstum
      button3->setText("F3");
      button3->setToolTip("Canned prompt F3");
      connect(button3, &QToolButton::clicked, [this] { handleCannedPrompt("F3"); });
      buttonPanelLayout->addWidget(button3);

      // 3c. Widgets in einer vertikalen Box kombinieren
      QWidget* inputContainer = new QWidget(this);
      inputContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      QVBoxLayout* inputContainerLayout = new QVBoxLayout(inputContainer);
      inputContainerLayout->setContentsMargins(0, 0, 0, 0);
      inputContainerLayout->setSpacing(0);

      // Input row
      QWidget* inputRow = new QWidget(this);
      inputRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      QHBoxLayout* inputRowLayout = new QHBoxLayout(inputRow);
      inputRowLayout->setContentsMargins(0, 0, 0, 0);
      inputRowLayout->setSpacing(2);

      inputRowLayout->addWidget(buttonPanel);
      inputRowLayout->addWidget(userInput);
      inputContainerLayout->addWidget(dataPanel);
      inputContainerLayout->addWidget(inputRow);

      mainLayout->addWidget(inputContainer); // kein Stretch: nimmt nur den nötigen Platz

      connect(_editor, &Editor::fontChanged, [this](QFont f) {
            chatDisplay->setFont(f);
            userInput->setFont(f);
            statusLabel->setFont(f);
            modelMenu->setFont(f);
            newSessionButton->setFont(f);
            deleteSessionButton->setFont(f);
            sessionComboBox->setFont(f);
            agentRoleCombo->setFont(f);
            button1->setFont(f);
            button2->setFont(f);
            button3->setFont(f);
            dashboard->setFont(f);
            });

      connect(
          chatDisplay, &QWebEngineView::loadFinished, this, [this] { session()->load(QString()); },
          Qt::QueuedConnection | Qt::SingleShotConnection);
      fetchModels();
      spinnerTimer = new QTimer(this);
      connect(spinnerTimer, &QTimer::timeout, this, &Agent::updateSpinner);

      _mcpManager->applyConfigs(_editor->mcpServersConfig());
      updateCannedPrompts();
      }

//---------------------------------------------------------
//   getConfigPath
// Helper function for the path
//---------------------------------------------------------

QString Agent::configPath() {
      QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
      QDir().mkpath(path);
      return path + "/models.json";
      }

//---------------------------------------------------------
//   setCurrentModel
//---------------------------------------------------------

void Agent::setCurrentModel(const QString& s, bool clearChat) {
      if (model.name == s)
            return;
      for (const auto& m : _editor->models()) {
            if (m.name == s) {
                  pendingModelName.clear();
                  model = m;
                  delete llm;
                  llm = llmFactory(this, &model, mcpTools);
                  connect(llm, &LLMClient::incomingChunk, this,
                          [this](const std::string& thoughtChunk, const std::string& textChunk) {
                                if (filterThoughts)
                                      chatDisplay->handleIncomingChunk("", textChunk);
                                else
                                      chatDisplay->handleIncomingChunk(thoughtChunk, textChunk);
                                });
                  currentRetryCount  = 0;
                  retryPause         = 2000;
                  rateLimitResetTime = QDateTime();

                  if (clearChat)
                        startNewSession();
                  if (modelMenu)
                        modelMenu->setCurrentText(s);
                  emit modelChanged();
                  return;
                  }
            }
      pendingModelName = s;
      Critical("model <{}> not found, setting as pending", s.toStdString());
      }

//---------------------------------------------------------
//   fetchModels
//    request list of local available ollama models
//---------------------------------------------------------

void Agent::fetchModels() {
      QNetworkRequest request(QUrl("http://localhost:11434/api/tags"));
      QNetworkReply* reply = networkManager->get(request);
      connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                  QString errorStr = reply->errorString();
                  Critical("network error: {}", errorStr);
                  return;
                  }
            try {
                  auto j = json::parse(reply->readAll().toStdString());
                  for (const auto& model : j["models"]) {
                        std::string name = model["name"];
                        bool found       = false;
                        for (auto& em : _editor->models()) {
                              if (em.modelIdentifier == name) {
                                    found = true;
                                    break;
                                    }
                              }
                        if (found)
                              continue;

                        Model m;
                        m.name            = QString::fromStdString(name);
                        m.modelIdentifier = m.name;
                        m.baseUrl         = "http://localhost:11434/api/chat";
                        m.api             = "ollama";
                        m.dynamic         = true;
                        _editor->addModel(m);
                        }
                  // Now we can select the last used model as saved in settings

                  if (!pendingModelName.isEmpty()) {
                        QString pending = pendingModelName;
                        pendingModelName.clear();
                        setCurrentModel(pending, false);
                        }
                  emit _editor->modelsChanged();
                  }
            catch (const json::parse_error& e) {
                  Debug("Parse Error: {}", e.what());
                  }
            catch (const json::type_error& e) {
                  Debug("TypeError: {}", e.what());
                  }
            catch (...) {
                  Critical("Unexpected error");
                  }
            });
      }

//---------------------------------------------------------
//   sendMessage
//---------------------------------------------------------

void Agent::sendMessage(QString qtext) {
      _stopRequested   = false;
      std::string text = qtext.trimmed().toStdString();
      if (text.empty() || model.name == "")
            return;

      json msg;
      msg["role"] = "user";
      if (model.api == "gemini")
            msg["parts"] = json::array({{{"text", text}}});
      else // ollama / anthropic / openai
            msg["content"] = text;
      // Attach all pending attachments (if any)
      if (!_attachments.isEmpty()) {
            // Handle text attachments: decode base64 content and append to user message
            for (const auto& att : _attachments) {
                  if (att.type == AttachmentType::Text && !att.data.isEmpty()) {
                        text += "\n\n";
                        text += att.data.toStdString();
                        }
                  }

            // Collect images for the LLM clients
            json imagesArray = json::array();
            for (const auto& att : _attachments)
                  if (att.type == AttachmentType::Image && !att.data.isEmpty())
                        imagesArray.push_back(att.data.toStdString());
            if (!imagesArray.empty())
                  msg["images"] = imagesArray;

            _attachments.clear();
            screenshotButton->setChecked(false);
            updateDataPanel();
            }
      std::string logText;
      std::string thought;
      logContent(msg, logText, thought);
      addMessage("User", logText);

      // Approximate token count: 4 chars per token
      session()->addRequest(msg, text.length() / 4);
      sendMessage2();
      }

//---------------------------------------------------------
//   truncateOutput
//---------------------------------------------------------

QString Agent::truncateOutput(const QString& text, int maxChars) {
      if (text.length() <= maxChars)
            return text;

      // Wir behalten die ersten maxChars und hängen einen Hinweis an
      int removed = text.length() - maxChars;
      return text.left(maxChars) +
             QString("\n\n... [Output truncated. %1 characters omitted for brevity]").arg(removed);
      }

//---------------------------------------------------------
//   truncateOutput
//---------------------------------------------------------

std::string Agent::truncateOutput(const std::string& text, int maxChars) {
      if (text.length() <= static_cast<size_t>(maxChars))
            return text;

      int removed = text.length() - maxChars;
      // Force line break if the string is just one long line
      std::string s = text.substr(0, maxChars);
      if (s.find('\n') == std::string::npos && s.length() > 100)
            s.insert(100, "\n");
      return s + std::format("\n\n... [Output truncated. {} characters omitted for brevity]", removed);
      }

//---------------------------------------------------------
//   stop
//---------------------------------------------------------

void Agent::stop() {
      _stopRequested = true;
      if (currentReply) {
            currentReply->disconnect();
            currentReply->abort();
            currentReply->deleteLater();
            currentReply = nullptr;
            }
      // If there is an active local event loop from a tool (e.g., ask_user), it needs to be exited.
      // But typically ask_user blocks processTools, and since tools are run sequentially,
      // an abort of network is mostly enough, but let's allow enableInput on stop
      stopAgent();
      }

//---------------------------------------------------------
//   sendMessage2
//---------------------------------------------------------

void Agent::sendMessage2() {
      if (_stopRequested) {
            _stopRequested = false;
            startAgent();
            return;
            }
      startAgent();
      retryPause = 2000;

      streamBuffer.clear();
      QNetworkRequest request;
      json jsonPayLoad   = llm->prompt(&request);
      QByteArray payload = QString::fromStdString(jsonPayLoad.dump()).toUtf8();

      CLog(AI, "send <{}>", jsonPayLoad.dump(3));
      request.setTransferTimeout(60000 * 10);

      chatDisplay->startNewStreamingMessage(model.name.toStdString());

      if (currentReply) {
            Critical("request already running? aborting currentReply={}", (void*)currentReply);
            currentReply->disconnect();
            currentReply->abort();
            currentReply->deleteLater();
            currentReply = nullptr;
            }
      currentReply = networkManager->post(request, payload);
      connect(currentReply, &QNetworkReply::readyRead, this, &Agent::handleChatReadyRead);
      connect(currentReply, &QNetworkReply::finished, this, &Agent::handleChatFinished);
      }

//---------------------------------------------------------
//   handleChatReadyRead
//---------------------------------------------------------

void Agent::handleChatReadyRead() {
      if (!currentReply) {
            Critical("no currentReply");
            return;
            }
      QByteArray newData = currentReply->readAll();
      //      CLog(AI, "{}", newData.data());
      streamBuffer.append(newData);
      processData();
      }

//---------------------------------------------------------
//   handleChatFinished
//---------------------------------------------------------

void Agent::handleChatFinished() {
      if (!currentReply) {
            Critical("no currentReply");
            return;
            }
      // --- ERROR HANDLING & BACKOFF LOGIC ---
      if (currentReply->error() != QNetworkReply::NoError) {
            int statusCode = currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // Case 1: Rate Limit (429) or Server Overload (503/500/502/504)
            if (statusCode == 429 || (statusCode >= 500 && statusCode < 600)) {
                  if (currentRetryCount < maxRetries) {
                        int waitMs = 0;
                        // A: Rate Limit (429) -> respect headers
                        if (statusCode == 429) {
                              // CORRECTION: Read "Retry-After" as raw header, as there is no enum for it
                              QByteArray retryAfterRaw = currentReply->rawHeader("Retry-After");

                              if (!retryAfterRaw.isEmpty()) {
                                    // Anthropic/OpenAI usually send seconds as integer here
                                    waitMs = retryAfterRaw.toInt() * 1000;
                                    }

                              // Fallback: Standard Exponential if no headers are there
                              if (waitMs <= 0)
                                    waitMs = 1000 * std::pow(2, currentRetryCount);

                              // Safety margin (jitter)
                              waitMs += (rand() % 500);

                              addMessage("system",
                                         std::format("<br><font color='orange'><b>[Rate Limit]:</b> Pause "
                                                     "for {} seconds...</font><br>",
                                                     waitMs / 1000.0));
                              }
                        // B: Server Error (5xx) -> Exponential Backoff
                        else {
                              waitMs = 2000 * std::pow(2, currentRetryCount);
                              addMessage("system", std::format("<br><font color='orange'><b>[Server Error "
                                                               "{}]:</b> Retry {}/{} in {}s...</font><br>",
                                                               statusCode, currentRetryCount + 1, maxRetries,
                                                               waitMs / 1000.0));
                              }

                        currentReply->deleteLater();
                        currentReply = nullptr;

                        currentRetryCount++;
                        isRetrying = true; // Set flag

                        // Start timer for next attempt
                        QTimer::singleShot(waitMs, this, &Agent::sendMessage2);
                        return;
                        }
                  else {
                        Debug("too many reply's");
                        addMessage("system", std::format("<br><font color='red'><b>[Abort]:</b> "
                                                         "Too many attempts ({}).</font><br>",
                                                         maxRetries));
                        }
                  }

            QString errorMessage = currentReply->errorString();
            // Generate specific messages
            switch (currentReply->error()) {
                  case QNetworkReply::TimeoutError:
                        errorMessage += "\nTimeout: The LL server did not respond within 60 "
                                        "seconds. Maybe the "
                                        "model is still loading or the server is hanging.";
                        break;
                  case QNetworkReply::ContentNotFoundError:
                        errorMessage += "\nModell not found (bad baseUrl configured?)";
                        break;
                  default: break;
                  }
            Debug("Network/API error {}: {}", int(currentReply->error()), errorMessage);

            // Show the error to the user in the UI
            addMessage("system", std::format("<br><font color='red'><b>[Connection abort]:</b> {}</font><br>",
                                             errorMessage));
            currentReply->deleteLater();
            currentReply = nullptr;

            stopAgent();
            return;
            }
      QByteArray newData = currentReply->readAll();
      streamBuffer.append(newData);
      processData();

      if (!streamBuffer.isEmpty())
            Critical("there is still data in the streamBuffer: <{}>", streamBuffer.data());

      currentReply->deleteLater();
      currentReply = nullptr;

      llm->dataFinished();

      // Only wipe and rebuild the UI if the tool loop did not
      // start a new request

      if (!currentReply)
            updateChatDisplay();
      session()->save();
      }

//---------------------------------------------------------
//   processData
//---------------------------------------------------------

void Agent::processData() {
      std::string content = streamBuffer.toStdString();

      while (!content.empty()) {
            // Suche den Anfang des nächsten Objekts
            size_t startPos = content.find('{');
            if (startPos == std::string::npos) {
                  streamBuffer.clear(); // Kein Objektanfang gefunden, Puffer verwerfen
                  break;
                  }

            bool foundValidObject = false;

            // Wir versuchen progressiv zu parsen, beginnend von startPos bis zum Ende des Strings
            // nlohmann::json::parse wirft eine Exception, wenn das JSON unvollständig ist.
            for (size_t len = 1; len <= content.length() - startPos; ++len) {
                  std::string potentialJson = content.substr(startPos, len);

                  // Check, ob das Ende plausibel aussieht (schließende Klammer)
                  if (potentialJson.back() != '}')
                        continue;

                  try {
                        // accept() ist schneller als parse(), da es kein Objekt im Speicher baut
                        if (json::accept(potentialJson)) {
                              auto j = json::parse(potentialJson);
                              //                              Debug("received <{}>", j.size());
                              CLog(AI, "received <{}>", j.dump(3));
                              try {
                                    llm->processJsonItem(j);
                                    }
                              catch (const std::exception& e) {
                                    Critical("Exception in processJsonItem: {}", e.what());
                                    }
                              catch (...) {
                                    Critical("Unknown exception in processJsonItem");
                                    }

                              // Puffer aktualisieren
                              size_t consumed = startPos + len;
                              streamBuffer.remove(0, static_cast<int>(consumed));
                              content = streamBuffer.toStdString();

                              foundValidObject = true;
                              break; // Zurück zum Anfang der while-Schleife
                              }
                        }
                  catch (const json::parse_error&) {
                        // Noch nicht vollständig oder korrupt, weitersuchen
                        continue;
                        }
                  }

            if (!foundValidObject) {
                  // Wir haben kein vollständiges Objekt im aktuellen Puffer gefunden
                  // Wir behalten den Rest für den nächsten dataReceived-Call
                  break;
                  }
            }
      }

//---------------------------------------------------------
//   startNewSession
//---------------------------------------------------------

void Agent::startNewSession() {
      stop();
      currentRetryCount = 0;
      isRetrying        = false;
      session()->save();
      session()->clear();
      _attachments.clear();
      updateDataPanel();
      chatDisplay->clear();

      session()->setName(session()->sessionName(true)); // create new session file name
      updateSessionList();
      addMessage("system", format("<i>[System: New session started: <b>{}</b>]</i><br>",
                                  QFileInfo(session()->name()).fileName().toStdString()));
      userInput->setFocus();
      }

//---------------------------------------------------------
//   deleteCurrentSession
//---------------------------------------------------------

void Agent::deleteCurrentSession() {
      if (session()->name().isEmpty())
            return;

      QFile file(session()->name());
      if (file.exists()) {
            file.remove();
            //            chatDisplay->append(
            //                QString("<i>[System: Session deleted: <b>%1</b>]</i><br>").arg(QFileInfo(session()->name()).fileName()));
            }

      session()->setName(QString());
      session()->clear();
      chatDisplay->clear();
      updateSessionList();
      userInput->setFocus();
      }

//---------------------------------------------------------
//   renameCurrentSession
//---------------------------------------------------------

void Agent::renameCurrentSession() {
      if (session()->name().isEmpty())
            return;

      QFileInfo currentInfo(session()->name());
      QString currentBase = currentInfo.completeBaseName(); // ohne ".json"

      bool ok         = false;
      QString newName = QInputDialog::getText(this, tr("Rename Session"), tr("New session name:"),
                                              QLineEdit::Normal, currentBase, &ok);

      if (!ok || newName.trimmed().isEmpty())
            return;

      newName = newName.trimmed();

      // Ungültige Zeichen für Dateinamen entfernen
      static const QRegularExpression invalidChars(R"([\\/:*?"<>|])");
      newName.replace(invalidChars, "_");

      QString dir        = currentInfo.absolutePath();
      QString suffix     = currentInfo.suffix(); // "json"
      QString targetName = newName;

      // Prüfen ob Zieldatei bereits existiert → automatisch nummerieren
      QString targetPath = dir + "/" + targetName + "." + suffix;
      if (targetPath != session()->name() && QFile::exists(targetPath)) {
            int counter = 2;
            do {
                  targetName = newName + "-" + QString::number(counter++);
                  targetPath = dir + "/" + targetName + "." + suffix;
                  } while (QFile::exists(targetPath));
            }

      // Umbenennen
      if (!QFile::rename(session()->name(), targetPath)) {
            QMessageBox::warning(this, tr("Rename failed"), tr("Could not rename session file."));
            return;
            }

      session()->setName(targetPath);
      updateSessionList();
      }

//---------------------------------------------------------
//   updateSessionList
//---------------------------------------------------------

void Agent::updateSessionList() {
      if (!sessionComboBox)
            return;

      bool wasBlocked = sessionComboBox->blockSignals(true);
      sessionComboBox->clear();

      QString root = _editor->projectRoot();
      if (!root.isEmpty()) {
            QString sessionFolder = QDir::cleanPath(root + "/.nped");
            QDir dir(sessionFolder);
            QStringList filters;
            filters << "Session-*.json";
            QFileInfoList files = dir.entryInfoList(filters, QDir::Files,
                                                    QDir::Time); // Sorted by time (newest first)

            for (const QFileInfo& fileInfo : files)
                  sessionComboBox->addItem(fileInfo.baseName(), fileInfo.absoluteFilePath());
            }

      int index = sessionComboBox->findData(session()->name());
      if (index >= 0) {
            sessionComboBox->setCurrentIndex(index);
            }
      else if (!session()->name().isEmpty()) {
            QFileInfo fi(session()->name());
            sessionComboBox->insertItem(0, fi.baseName(), session()->name());
            sessionComboBox->setCurrentIndex(0);
            }

      sessionComboBox->blockSignals(wasBlocked);
      }

//---------------------------------------------------------
//   onSessionSelected
//---------------------------------------------------------

void Agent::onSessionSelected(int index) {
      if (index < 0)
            return;
      QString fileName = sessionComboBox->itemData(index).toString();
      if (fileName != session()->name()) {
            session()->save();
            session()->load(fileName);
            }
      }

//---------------------------------------------------------
//   isWorking
//---------------------------------------------------------

bool Agent::isWorking() const {
      return spinnerTimer->isActive();
      }

//---------------------------------------------------------
//   updateSpinner
//---------------------------------------------------------

void Agent::updateSpinner() {
      const QString frames = "|/-\\";
      statusLabel->setText(QString(frames[spinnerFrame % 4]));
      spinnerFrame++;
      }

//---------------------------------------------------------
//   eventFilter
//   Fängt Enter vs. Shift+Enter im mehrzeiligen Textfeld ab
//---------------------------------------------------------

bool Agent::eventFilter(QObject* obj, QEvent* event) {
      if (obj == userInput || obj == userInput->viewport()) {
            // --- Key handling ---
            if (event->type() == QEvent::KeyPress) {
                  QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
                  if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                        if (keyEvent->modifiers() & Qt::ControlModifier) {
                              sendMessage(userInput->toPlainText());
                              userInput->clear();
                              return true;
                              }
                        // Just insert a newline for normal Return
                        return false;
                        }
                  }
            }
      return QWidget::eventFilter(obj, event);
      }

//---------------------------------------------------------
//   formatToolCall
//    formats a tool call and its output (result)
//---------------------------------------------------------

std::string Agent::formatToolCall(const std::string& name, const json& args, const std::string& result) {
      std::string argsStr = "";
      bool first          = true;
      for (auto& [key, value] : args.items()) {
            if (!first)
                  argsStr += ", ";
            argsStr += std::format("{}={}", key, value.dump());
            first    = false;
            }

      std::string output = std::format("\n\n<i>[System: Run Tool: {}({})]</i>\n\n", name, argsStr);
      if (!result.empty()) {
            std::string truncatedResult  = truncateOutput(result, kChatResultMaxChars);
            output                      += std::format("```\n{}\n```\n\n", truncatedResult);
            }
      return output;
      }

//---------------------------------------------------------
//   logContent
//    gemini
//    {
//    "role": user
//    "parts": [
//          { "text": "mops"},
//          ]
//     }
//
//    ollama:
//    {
//    "role": user
//    "content": "mops"
//    }
//---------------------------------------------------------

void Agent::logContent(const json& content, std::string& msg, std::string& thought) {
      try {
            if (content.contains("images") && content["images"].is_array()) {
                  for (const auto& img : content["images"]) {
                        if (img.is_string()) {
                              msg += std::format("<img src=\"data:image/jpeg;base64,{}\" style=\"max-width: "
                                                 "500px; display: block; margin: "
                                                 "10px 0;\"/><br>",
                                                 img.get<std::string>());
                              }
                        }
                  }

            if (!content.contains("parts")) {
                  // Anthropic Extended Thinking: top-level "thinking" field on assistant messages.
                  // The field is stored as a JSON object {type, thinking, signature} or as a
                  // plain string (legacy sessions).
                  if (content.contains("thinking")) {
                        const auto& th = content["thinking"];
                        if (th.is_string())
                              thought += th.get<std::string>();
                        else if (th.is_object() && th.contains("thinking"))
                              thought += th["thinking"].get<std::string>();
                        }
                  // ollama / openai / anthropic
                  if (content.contains("content")) {
                        json c = content["content"];
                        std::string s;
                        if (c.is_array())
                              for (auto& e : c)
                                    s += e.get<std::string>();
                        else
                              s = truncateOutput(c.get<std::string>(), kChatResultMaxChars);

                        // Parse out <think> and <thought> blocks for models that inline them in content (e.g. Ollama deepseek-r1, gemma4)
                        auto extractTag = [&](const std::string& startTag, const std::string& endTag) {
                              size_t thinkStart = 0;
                              while ((thinkStart = s.find(startTag)) != std::string::npos) {
                                    size_t thinkEnd = s.find(endTag, thinkStart);
                                    if (thinkEnd != std::string::npos) {
                                          thought += s.substr(thinkStart + startTag.length(),
                                                              thinkEnd - (thinkStart + startTag.length()));
                                          s.erase(thinkStart, thinkEnd + endTag.length() - thinkStart);
                                          }
                                    else {
                                          thought += s.substr(thinkStart + startTag.length());
                                          s.erase(thinkStart);
                                          }
                                    }
                              };

                        extractTag("<think>", "</think>");
                        extractTag("<thought>", "</thought>");
                        extractTag("<|channel>thought\n", "<channel|>");
                        extractTag("<|channel>thought", "<channel|>");

                        if (content.contains("role") &&
                            (content["role"] == "function" || content["role"] == "tool")) {
                              if (!filterToolMessages) {
                                    if (content.contains("function")) {
                                          json fc  = content["function"];
                                          msg     += formatToolCall(fc["name"], fc["arguments"], s);
                                          }
                                    }
                              }
                        else
                              msg += s;
                        }
                  if (content.contains("tool_calls")) {
                        if (!filterToolMessages) {
                              for (const auto& tool : content["tool_calls"]) {
                                    if (tool.contains("function")) {
                                          json fc  = tool["function"];
                                          msg     += formatToolCall(fc["name"], fc["arguments"], "");
                                          }
                                    }
                              }
                        }
                  }
            else {
                  // gemini:
                  for (const auto& part : content["parts"]) {
                        if (part.contains("text")) {
                              if (part.contains("thought") && part["thought"] == true)
                                    thought += part["text"];
                              else
                                    msg += part["text"];
                              }
                        if (part.contains("functionResponse")) {
                              if (!filterToolMessages) {
                                    json fr = part["functionResponse"];
                                    std::string output =
                                        std::format("\n\n<i>[System: Tool Response: {}()]</i>\n\n",
                                                    std::string(fr["name"]));
                                    std::string s =
                                        truncateOutput(static_cast<std::string>(fr["response"]["content"]),
                                                       kChatResultMaxChars);
                                    msg += std::format("\n\n```\n{}\n```\n\n", s);
                                    }
                              }
                        if (part.contains("functionCall")) {
                              if (!filterToolMessages) {
                                    json fc  = part["functionCall"];
                                    msg     += formatToolCall(fc["name"], fc["args"]);
                                    }
                              }
                        }
                  }
            }
      catch (const json::parse_error& e) {
            Debug("Parse Error: {}", e.what());
            }
      catch (const json::type_error& e) {
            Debug("TypeError: {}", e.what());
            }
      catch (...) {
            Critical("Unexpected error");
            }
      }

//---------------------------------------------------------
//   updateChatDisplay
//---------------------------------------------------------

void Agent::updateChatDisplay(bool scrollToBottom) {
      chatDisplay->clear();

      size_t totalEntries   = session()->data().size();
      size_t activeCount    = session()->getActiveEntriesCount();
      size_t startActiveIdx = totalEntries > activeCount ? totalEntries - activeCount : 0;

      for (size_t i = 0; i < totalEntries; ++i) {
            const auto& item    = session()->data()[i];
            const auto& content = item.content;
            std::string role;
            if (!content.contains("role")) {
                  Critical("no role in chatHistory: <{}>", content.dump(3));
                  // assume this is a "system" message in the hope something will be displayed
                  role = "system";
                  }
            else
                  role = content["role"];

            std::string msg;
            std::string thought;
            logContent(content, msg, thought);

            if (filterThoughts)
                  thought.clear();

            if (msg.empty() && thought.empty())
                  continue;

            QString s     = chatDisplay->renderMarkdownToHtml(msg);
            QString th    = chatDisplay->renderMarkdownToHtml(thought);
            bool isActive = (i == 0 || i >= startActiveIdx);
            chatDisplay->appendStaticHtml(QString::fromStdString(role), s, th, isActive);
            }
      if (scrollToBottom)
            chatDisplay->scrollToBottom();
      }

//---------------------------------------------------------
//   onScreenshotReady
//    Called when the XDG portal delivers a screenshot image.
//    Converts the image to base64-encoded PNG and stores it.
//    The data will be attached to the next prompt sent to the LLM.
//---------------------------------------------------------

void Agent::onScreenshotReady(const QImage& image) {
      // Encode image as JPEG into a QByteArray, then base64
      QByteArray jpegData;
      QBuffer buf(&jpegData);
      buf.open(QIODevice::WriteOnly);
      image.save(&buf, "JPEG", 70); // quality 70 – good balance between size and fidelity
      buf.close();

      if (jpegData.size() > kMaxAttachmentSize) {
            addMessage("system", "<i>[⚠️ Screenshot too large, discarding.]</i><br>");
            return;
            }
      _attachments.append({AttachmentType::Image, jpegData.toBase64(), "screenshot"});

      const int count = _attachments.size();
      screenshotButton->setChecked(true);
      screenshotButton->setToolTip(QString("%1 attachment(s) attached. Click 📷 to discard all.").arg(count));

      addMessage("system", std::format("<i>[Image #{} attached ({}×{} px) – will be sent with next prompt. "
                                       "Click 📷 to discard all.]</i><br>",
                                       count, image.width(), image.height()));
      updateDataPanel();
      }

//---------------------------------------------------------
//   onScreenshotFailed
//    Called when the portal screenshot fails or is cancelled.
//---------------------------------------------------------

void Agent::onScreenshotFailed(const QString& reason) {
      screenshotButton->setChecked(false);
      addMessage("system", std::string("<font color='orange'><i>[Screenshot failed: ") +
                               reason.toStdString() + "]</i></font><br>");
      updateDataPanel();
      }

//---------------------------------------------------------
//   updateDataPanel
//   Rebuilds thumbnail/preview icons in the narrow left panel next
//   to the prompt input. Creates one icon/button for every
//   currently pending attachment.
//   - Image files → thumbnail preview
//   - Text files → first few chars as text icon
//   - Other files → generic file icon
//   Old buttons are removed from the layout and deleted.
//---------------------------------------------------------

void Agent::updateDataPanel() {
      // Remove and delete all existing attachment buttons
      for (QToolButton* btn : _attachmentButtons) {
            dataPanelLayout->removeWidget(btn);
            delete btn;
            }
      _attachmentButtons.clear();

      // Create one button per pending attachment in a single loop
      for (int i = 0; i < _attachments.size(); ++i) {
            const auto& att        = _attachments[i];
            const QString fileType = QFileInfo(att.label).suffix().toLower();

            QToolButton* btn = new QToolButton(dataPanel);
            btn->setParent(dataPanel);
            btn->setCheckable(true);
            btn->setAutoExclusive(true);
            btn->setFixedSize(36, 36);
            btn->setToolTip(att.label);
            btn->setProperty("attachmentIndex", i);

            // Determine icon based on attachment type
            if (att.type == AttachmentType::Image) {
                  // Show image thumbnail
                  const QByteArray raw = QByteArray::fromBase64(att.data.toUtf8());
                  QImage img;
                  if (img.loadFromData(raw)) {
                        QPixmap pm = QPixmap::fromImage(
                            img.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        btn->setIcon(QIcon(pm));
                        }
                  else {
                        btn->setText("🖼");
                        }
                  }
            else if (att.type == AttachmentType::Text) {
                  // Show text file with document icon + extension
                  btn->setText("📄\n" + fileType);
                  btn->setFont(QFont("Monospace", 7));
                  btn->setStyleSheet(
                      "QToolButton { border: 1px solid #555; border-radius: 4px; "
                      "background: #2d2d2d; font-size: 10px; } "
                      "QToolButton:checked { border: 2px solid #0078d7; background: #3a3a3a; }");
                  }
            else if (att.type == AttachmentType::Audio) {
                  // Audio file icon
                  btn->setText("🔊");
                  }
            else {
                  // Other file type — try to get extension from label (filename)
                  const QString ext = fileType;
                  QString fileIcon  = "📁";
                  if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz")
                        fileIcon = "📦";
                  else if (ext == "pdf")
                        fileIcon = "📕";
                  else if (ext == "exe" || ext == "dll" || ext == "bin")
                        fileIcon = "⚙️";
                  else if (ext == "doc" || ext == "docx" || ext == "odt")
                        fileIcon = "📘";
                  else if (ext == "xls" || ext == "xlsx" || ext == "csv")
                        fileIcon = "📗";
                  btn->setText(fileIcon);
                  }

            // Connect clicked for select/deselect logic
            connect(btn, &QToolButton::clicked, this, &Agent::onAttachmentClicked);
            connect(btn, &QToolButton::clicked, [this, i]() { onAttachmentSelected(i); });

            dataPanelLayout->addWidget(btn);
            _attachmentButtons.append(btn);
            }

      // Add stretch at the end if needed
      if (dataPanelLayout->count() > 0) {
            int itemCount     = dataPanelLayout->count();
            QLayoutItem* last = dataPanelLayout->itemAt(itemCount - 1);
            if (last && last->spacerItem()) {
                  dataPanelLayout->removeItem(last);
                  delete last;
                  }
            }
      dataPanelLayout->addStretch(1);
      }

//---------------------------------------------------------
//   onAttachmentClicked
//---------------------------------------------------------

void Agent::onAttachmentClicked() {
      QToolButton* btn = qobject_cast<QToolButton*>(sender());
      if (!btn)
            return;
      int index = btn->property("attachmentIndex").toInt();
      if (index >= 0 && index < _attachments.size()) {
            if (btn->isChecked()) {
                  // If already checked, uncheck it
                  btn->setChecked(false);
                  }
            else {
                  // Uncheck all other buttons
                  for (QToolButton* otherBtn : _attachmentButtons)
                        otherBtn->setChecked(false);
                  // Check this one
                  btn->setChecked(true);
                  }
            emit attachmentClicked(index);
            }
      }

//---------------------------------------------------------
//   onAttachmentSelected
//---------------------------------------------------------

void Agent::onAttachmentSelected(int index) {
      if (index < 0 || index >= _attachments.size())
            return;
      const Attachment& att = _attachments[index];

      if (att.type == AttachmentType::Text) {
            // Show text file content in chat
            QFile file(att.label);
            if (file.open(QIODevice::ReadOnly)) {
                  QTextStream in(&file);
                  QString text = in.readAll();
                  file.close();
                  addMessage("attachment", std::format("[%1]\n```\n%2\n```", QFileInfo(att.label).fileName(),
                                                       truncateOutput(text.toStdString(), 5000)));
                  }
            }
      else if (att.type == AttachmentType::Image) {
            addMessage("attachment", std::format("[Image: {}]", QFileInfo(att.label).fileName()));
            }
      else {
            addMessage("attachment", std::format("[Attachment: {}] ({})", QFileInfo(att.label).fileName(),
                                                 QFileInfo(att.label).size()));
            }
      }

//---------------------------------------------------------
//   removeAttachment
//---------------------------------------------------------

void Agent::removeAttachment(int index) {
      if (index >= 0 && index < _attachments.size()) {
            _attachments.removeAt(index);
            updateDataPanel();
            }
      }

//---------------------------------------------------------
//   updateCannedPrompts
//---------------------------------------------------------

void Agent::updateCannedPrompts() {
      QString root = _editor->projectRoot();
      if (root.isEmpty())
            return;
      QString npedDir  = root + "/.nped";
      QString filePath = npedDir + "/agent.json";
      QFile file(filePath);
      if (!file.exists()) {
            QDir dir;
            if (!dir.exists(npedDir))
                  dir.mkpath(npedDir);
            if (file.open(QIODevice::WriteOnly)) {
                  json j;
                  j["F1"] = {
                           {       "name",               "F1"},
                           {"description", "Canned prompt F1"},
                           {       "text",                 ""}
                        };
                  j["F2"] = {
                           {       "name",               "F2"},
                           {"description", "Canned prompt F2"},
                           {       "text",                 ""}
                        };
                  j["F3"] = {
                           {       "name",               "F3"},
                           {"description", "Canned prompt F3"},
                           {       "text",                 ""}
                        };
                  std::string s = j.dump(4);
                  file.write(s.c_str(), s.length());
                  file.close();
                  }
            else {
                  Debug("Failed to create agent.json");
                  }
            }

      if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            try {
                  json j = json::parse(data.toStdString());
                  if (j.contains("F1") && j["F1"].is_object()) {
                        button1->setText(QString::fromStdString(j["F1"].value("name", "F1")));
                        button1->setToolTip(
                            QString::fromStdString(j["F1"].value("description", "Canned prompt F1")));
                        }
                  if (j.contains("F2") && j["F2"].is_object()) {
                        button2->setText(QString::fromStdString(j["F2"].value("name", "F2")));
                        button2->setToolTip(
                            QString::fromStdString(j["F2"].value("description", "Canned prompt F2")));
                        }
                  if (j.contains("F3") && j["F3"].is_object()) {
                        button3->setText(QString::fromStdString(j["F3"].value("name", "F3")));
                        button3->setToolTip(
                            QString::fromStdString(j["F3"].value("description", "Canned prompt F3")));
                        }
                  }
            catch (const std::exception& e) {
                  Debug("Exception while parsing agent.json: {}", e.what());
                  }
            catch (...) {
                  Debug("Error parsing agent.json");
                  }
            }
      else {
            Debug("Failed to open agent.json for reading");
            }
      }

//---------------------------------------------------------
//   handleCannedPrompt
//---------------------------------------------------------

void Agent::handleCannedPrompt(const QString& buttonId) {
      QString root = _editor->projectRoot();
      if (root.isEmpty())
            return;
      QString npedDir  = root + "/.nped";
      QString filePath = npedDir + "/agent.json";

      if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            QDir dir;
            if (!dir.exists(npedDir))
                  dir.mkpath(npedDir);
            QFile file(filePath);
            if (!file.exists()) {
                  if (file.open(QIODevice::WriteOnly)) {
                        json j;
                        j["F1"] = {
                                 {       "name",               "F1"},
                                 {"description", "Canned prompt F1"},
                                 {       "text",                 ""}
                              };
                        j["F2"] = {
                                 {       "name",               "F2"},
                                 {"description", "Canned prompt F2"},
                                 {       "text",                 ""}
                              };
                        j["F3"] = {
                                 {       "name",               "F3"},
                                 {"description", "Canned prompt F3"},
                                 {       "text",                 ""}
                              };
                        std::string s = j.dump(4);
                        file.write(s.c_str(), s.length());
                        file.close();
                        }
                  }
            _editor->addFile(filePath);
            }
      else {
            QFile file(filePath);
            if (!file.exists()) {
                  QDir dir;
                  if (!dir.exists(npedDir))
                        dir.mkpath(npedDir);
                  if (file.open(QIODevice::WriteOnly)) {
                        json j;
                        j["F1"] = {
                                 {       "name",               "F1"},
                                 {"description", "Canned prompt F1"},
                                 {       "text",                 ""}
                              };
                        j["F2"] = {
                                 {       "name",               "F2"},
                                 {"description", "Canned prompt F2"},
                                 {       "text",                 ""}
                              };
                        j["F3"] = {
                                 {       "name",               "F3"},
                                 {"description", "Canned prompt F3"},
                                 {       "text",                 ""}
                              };
                        std::string s = j.dump(4);
                        file.write(s.c_str(), s.length());
                        file.close();
                        }
                  }
            if (file.open(QIODevice::ReadOnly)) {
                  QByteArray data = file.readAll();
                  try {
                        json j          = json::parse(data.toStdString());
                        std::string key = buttonId.toStdString();
                        if (j.contains(key) && j[key].is_object()) {
                              QString text = QString::fromStdString(j[key].value("text", ""));
                              if (!text.isEmpty()) {
                                    userInput->insertPlainText(text);
                                    userInput->setFocus();
                                    }
                              }
                        else if (j.contains(key) && j[key].is_string()) {
                              QString text = QString::fromStdString(j[key].get<std::string>());
                              if (!text.isEmpty()) {
                                    userInput->insertPlainText(text);
                                    userInput->setFocus();
                                    }
                              }
                        }
                  catch (...) {
                        addMessage("system", "<i>[System: Error parsing .nped/agent.json]</i><br>");
                        updateChatDisplay();
                        chatDisplay->scrollToBottom();
                        }
                  }
            }
      }

//---------------------------------------------------------
//   addAttachment
//    Opens a file dialog to attach ANY file type.
//    Detects the file type (Image, Text, Audio, Other) and
//    handles it appropriately.
//---------------------------------------------------------

void Agent::addAttachment() {
      // Filter for all files, with a note about supported types
      const QString allFilter   = tr("All Files (*)");
      const QString imageFilter = tr("Images (*.jpg *.jpeg *.png *.bmp *.gif *.svg *.webp)");
      const QString textFilter  = tr("Text Files (*.txt *.md *.json *.xml *.csv *.ini *.cfg *.conf)");
      const QString audioFilter = tr("Audio Files (*.mp3 *.wav *.ogg *.flac *.aac)");

      // Use a custom filter that shows all files but highlights common types
      QFileDialog dialog(this, tr("Attach File"), "",
                         tr("All Files (*)") + ";;" + imageFilter + ";;" + textFilter + ";;" + audioFilter);
      dialog.setFileMode(QFileDialog::ExistingFile);
      dialog.setViewMode(QFileDialog::Detail);
      dialog.setOption(QFileDialog::DontUseNativeDialog, false);

      if (dialog.exec() == QDialog::Accepted) {
            const QStringList selectedFiles = dialog.selectedFiles();
            if (selectedFiles.isEmpty())
                  return;

            const QString filePath = selectedFiles[0];
            QFileInfo fileInfo(filePath);
            if (fileInfo.size() > kMaxAttachmentSize) {
                  addMessage("system",
                             QString("<i>[⚠️ File %1 is too large (%2 KB). Max allowed is %3 KB.]</i><br>")
                                 .arg(fileInfo.fileName())
                                 .arg(fileInfo.size() / 1024)
                                 .arg(kMaxAttachmentSize / 1024)
                                 .toStdString());
                  return;
                  }
            const QString fileName      = fileInfo.fileName();
            const QString fileExtension = fileInfo.suffix().toLower();

            // Create attachment
            Attachment attachment;
            attachment.label = filePath; // Store full path

            // Detect file type and handle accordingly
            if (fileExtension == "jpg" || fileExtension == "jpeg" || fileExtension == "png" ||
                fileExtension == "bmp" || fileExtension == "gif" || fileExtension == "svg" ||
                fileExtension == "webp" || fileExtension == "tiff" || fileExtension == "tif") {
                  // Image file
                  attachment.type = AttachmentType::Image;

                  // Load and convert to base64 JPEG
                  QImage img;
                  if (img.load(filePath)) {
                        QByteArray jpegData;
                        QBuffer buf(&jpegData);
                        buf.open(QIODevice::WriteOnly);
                        img.save(&buf, "JPEG", 80); // quality 80
                        buf.close();
                        attachment.data = jpegData.toBase64();

                        addMessage("system", QString("<i>[🖼 Image #%1 attached (%2×%3 px, %4) – will be "
                                                     "sent with next prompt.]</i><br>")
                                                 .arg(_attachments.size() + 1)
                                                 .arg(img.width())
                                                 .arg(img.height())
                                                 .arg(QByteArray::number(jpegData.size() / 1024) + " KB")
                                                 .toStdString());
                        }
                  else {
                        // Fallback if image can't be loaded
                        attachment.type = AttachmentType::Other;
                        addMessage("system", QString("<i>[⚠️ Could not load image %1 as attachment.]</i><br>")
                                                 .arg(fileName)
                                                 .toStdString());
                        }
                  }
            else if (fileExtension == "mp3" || fileExtension == "wav" || fileExtension == "ogg" ||
                     fileExtension == "flac" || fileExtension == "aac" || fileExtension == "wma" ||
                     fileExtension == "midi") {
                  // Audio file
                  attachment.type = AttachmentType::Audio;
                  attachment.data = filePath.toUtf8().toBase64(); // Store path as base64

                  addMessage(
                      "system",
                      QString("<i>[🔊 Audio #%1 attached (%2 KB) – will be sent with next prompt.]</i><br>")
                          .arg(_attachments.size() + 1)
                          .arg(QByteArray::number(QFileInfo(filePath).size() / 1024) + " KB")
                          .toStdString());
                  }
            else if (fileExtension == "txt" || fileExtension == "md" || fileExtension == "json" ||
                     fileExtension == "xml" || fileExtension == "csv" || fileExtension == "ini" ||
                     fileExtension == "cfg" || fileExtension == "conf" || fileExtension == "yaml" ||
                     fileExtension == "yml" || fileExtension == "html" || fileExtension == "htm" ||
                     fileExtension == "css" || fileExtension == "js" || fileExtension == "ts" ||
                     fileExtension == "cpp" || fileExtension == "h" || fileExtension == "hpp" ||
                     fileExtension == "py" || fileExtension == "java" || fileExtension == "c" ||
                     fileExtension == "cc" || fileExtension == "cxx" || fileExtension == "cs" ||
                     fileExtension == "php" || fileExtension == "rb" || fileExtension == "swift" ||
                     fileExtension == "go" || fileExtension == "rs" || fileExtension == "sh" ||
                     fileExtension == "bat" || fileExtension == "cmd" || fileExtension == "sql" ||
                     fileExtension == "log" || fileExtension == "rtf") {
                  // Text file
                  attachment.type = AttachmentType::Text;

                  // Read the content
                  QFile file(filePath);
                  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        attachment.data = file.readAll();
                        file.close();

                        addMessage("system",
                                   std::format("<i>[📄 Text #{} attached ({} lines, {} KB) – will be "
                                               "sent with next prompt.]</i><br>",
                                               _attachments.size() + 1, attachment.data.count('\n') + 1,
                                               QByteArray::number(attachment.data.size() / 1024) + " KB"));
                        }
                  else {
                        // Fallback if file can't be read
                        attachment.type = AttachmentType::Other;
                        attachment.data = filePath.toUtf8().toBase64();
                        addMessage("system", QString("<i>[⚠️ Could not read text file %1.]</i><br>")
                                                 .arg(fileName)
                                                 .toStdString());
                        }
                  }
            else {
                  // Other file type
                  attachment.type = AttachmentType::Other;
                  attachment.data = filePath.toUtf8().toBase64(); // Store path as base64

                  addMessage(
                      "system",
                      QString("<i>[📁 File #%1 attached (%2 KB) – will be sent with next prompt.]</i><br>")
                          .arg(_attachments.size() + 1)
                          .arg(QByteArray::number(QFileInfo(filePath).size() / 1024) + " KB")
                          .toStdString());
                  }

            _attachments.append(attachment);

            // Update UI
            const int count = _attachments.size();
            screenshotButton->setChecked(true);
            screenshotButton->setToolTip(
                QString("%1 attachment(s) attached. Click 📷 to discard all.").arg(count));

            updateDataPanel();
            }
      }

//---------------------------------------------------------
//   addMessage
//---------------------------------------------------------

void Agent::addMessage(const std::string& role, const std::string& text) {
      chatDisplay->addMessage(role, text);
      }

//---------------------------------------------------------
//   agentRole
//---------------------------------------------------------

const AgentRole* Agent::agentRole() const {
      int idx = agentRoleCombo->currentIndex();
      return &_editor->agentRoles().at(idx);
      }

//---------------------------------------------------------
//   startAgent
//    Called before the agent starts.
//    - write back all dirty files to synchronize external
//      tools
//---------------------------------------------------------

void Agent::startAgent() {
      userInput->setEnabled(false);
      spinnerFrame = 0;
      spinnerTimer->start(100);
      statusLabel->setStyleSheet("color: #ff8800; font-weight: bold;");
      for (auto f : _editor->getFiles()) {
            f->undo()->beginMacro();
            f->release();
            f->undo()->endMacro();
            }
      }

//---------------------------------------------------------
//   stopAgent
//    This is called after the agent stops.
//    - restore all files (potentially modified from agent)
//---------------------------------------------------------

void Agent::stopAgent() {
      for (auto f : _editor->getFiles()) {
            f->undo()->beginMacro();
            f->load();
            f->undo()->endMacro();
            }
      userInput->setEnabled(true);
      spinnerTimer->stop();
      statusLabel->setText(">");
      statusLabel->setStyleSheet("color: normal; font-weight: bold;");
      userInput->setFocus();
      }
