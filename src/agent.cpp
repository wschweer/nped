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
#include <QDebug>
#include <QBuffer>
#include <QPixmap>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <iostream>
#include <fstream>

#include "agent.h"
#include "logger.h"
#include "editor.h"
// #include "undo.h"
// #include "webview.h"
#include "llm.h"
#include "chatdisplay.h"
#include "historymanager.h"
#include "screenshot.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Default manifests
static const std::string manifestBuildDefault =
    "You are an experienced C++ developer. "
    "Your task is to analyze and write code in the project and to fix build errors.\n\n"
    "Use modern c++. Prefer object oriented design and use modern design patterns.\n"
    "Answer exclusively in JSON format when you call a tool:\n"
    "PROJECT STRUCTURE:\n"
    "Standard Qt6 layout. The build directory is './build'. Use CMake with ninja.\n"
    "Use the run_build_command tool to compile the project and check if errors occur. ";

static const std::string manifestPlanDefault =
    "You are an experienced C++ developer acting as a system architect. "
    "Your task is to analyze the project, read code, and create an implementation plan.\n\n"
    "You are currently in PLAN MODE. This means you have read-only access. "
    "You can search and read files, but you cannot write files or execute build commands.\n"
    "Answer exclusively in JSON format when you call a tool.\n"
    "Focus on deeply understanding the problem and propose a detailed step-by-step solution. ";

//---------------------------------------------------------
//   Agent (Constructor)
//---------------------------------------------------------

Agent::Agent(Editor* e, QWidget* parent) : QWidget(parent), _editor(e) {
      networkManager = new QNetworkAccessManager(this);
      historyManager = new HistoryManager();
      mcpTools       = getMCPTools();

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

      modelMenu = new QComboBox(this);
      modelMenu->setMinimumWidth(210);
      connect(this, &Agent::modelsChanged, [this] {
            bool blocked = modelMenu->blockSignals(true);
            modelMenu->clear();
            for (const auto& m : _models)
                  modelMenu->addItem(m.name);
            if (!model.name.isEmpty())
                  modelMenu->setCurrentText(model.name);
            modelMenu->blockSignals(blocked);
            });
      connect(modelMenu, &QComboBox::activated, [this](int index) { setCurrentModel(_models[index].name); });

      sessionComboBox = new QComboBox(this);
      sessionComboBox->setMinimumWidth(230);
      connect(sessionComboBox, &QComboBox::activated, this, &Agent::onSessionSelected);

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

      modeButton = new QToolButton(this);
      modeButton->setCheckable(true);
      if (!_editor->projectMode()) {
            _isExecuteMode = false;
            modeButton->setEnabled(false);
            }
      modeButton->setChecked(_isExecuteMode);
      modeButton->setEnabled(_editor->projectMode());
      modeButton->setText(_isExecuteMode ? "Build" : "Plan");
      modeButton->setProperty("class", _isExecuteMode ? "errorButton" : "actionButton");


      dashboard->addWidget(statusLabel);
      dashboard->addWidget(modeButton, 0);
      dashboard->addWidget(renameSessionButton, 0);
      dashboard->addWidget(deleteSessionButton, 0);
      dashboard->addWidget(newSessionButton, 0);
      dashboard->addWidget(sessionComboBox, 0);
      dashboard->addWidget(modelMenu, 0);

      connect(modeButton, &QToolButton::toggled, [this](bool checked) {
            if (!_editor->projectMode())
                  return;
            _isExecuteMode = checked;
            modeButton->setText(checked ? "Build" : "Plan");
            modeButton->setProperty("class", checked ? "errorButton" : "actionButton");
            modeButton->style()->unpolish(modeButton);
            modeButton->style()->polish(modeButton);

            mcpTools = getMCPTools();
            if (llm)
                  llm->setTools(mcpTools);

            if (chatDisplay) {
                  chatDisplay->addMessage("system", format("<br><i>[System: Mode changed to <b>{}</b>]</i>",
                                                           (checked ? "Build (read/write)" : "Plan (read only)")));
                  }
            });

      // Filter toggle buttons (icon-only, no pulldown menu needed)
      showToolMessageAction = new QAction("🔧", this);
      showToolMessageAction->setCheckable(true);
      showToolMessageAction->setChecked(!filterToolMessages);
      showToolMessageAction->setToolTip("Show Tool Messages");
      showToolMessageAction->setIcon(QIcon(_editor->darkMode() ? "images/tool-dark.svg" : ":images/tool.svg"));
      connect(showToolMessageAction, &QAction::toggled, [this](bool checked) {
            filterToolMessages = !checked;
            saveStatus();
            updateChatDisplay();
            chatDisplay->scrollToBottom();
            });
      dashboard->addAction(showToolMessageAction, 1);

      showThoughtsAction = new QAction(this);
      showThoughtsAction->setIcon(QIcon(_editor->darkMode() ? "images/thinking-dark.svg" : ":images/thinking.svg"));
      showThoughtsAction->setCheckable(true);
      showThoughtsAction->setChecked(!filterThoughts);
      showThoughtsAction->setToolTip("Show Thoughts");
      connect(showThoughtsAction, &QAction::toggled, [this](bool checked) {
            filterThoughts = !checked;
            saveStatus();
            updateChatDisplay();
            chatDisplay->scrollToBottom();
            });
      dashboard->addAction(showThoughtsAction, 1);

      // Screenshot button
      screenshotButton = new QToolButton(this);
      screenshotButton->setText("📷");
      connect(_editor, &Editor::fontChanged, [this] (QFont f) { screenshotButton->setFont(f); });
      screenshotButton->setToolTip("Take Screenshot and attach to next prompt");
      screenshotButton->setCheckable(true);
      screenshotButton->setChecked(false);
            screenshotButton->setIcon(QIcon(_editor->darkMode() ? "images/camera-dark.svg" : ":images/camera.svg"));
      dashboard->addWidget(screenshotButton, 1);

      screenshotHelper = new ScreenshotHelper(this);
      connect(screenshotHelper, &ScreenshotHelper::screenshotReady, this, &Agent::onScreenshotReady);
      connect(screenshotHelper, &ScreenshotHelper::screenshotFailed, this, &Agent::onScreenshotFailed);

      connect(screenshotButton, &QToolButton::clicked, [this](bool) {
            // If images are already attached, discard all of them
            if (!_pendingImages.isEmpty()) {
                  _pendingImages.clear();
                  screenshotButton->setChecked(false);
                  screenshotButton->setToolTip("Take Screenshot and attach to next prompt");
                  chatDisplay->addMessage("system", "<i>[All attached images discarded]</i><br>");
                  updateDataPanel();
                  return;
                  }
            screenshotHelper->takeScreenshot();
            });

      // Initialize token count
      dashboard->setTokenCount(historyManager->totalTokens);
      // Connect token updates
      connect(historyManager, &HistoryManager::tokensChanged, [this](size_t tokens) {
            dashboard->setTokenCount(tokens);
      });

      connect(_editor, &Editor::darkModeChanged, [this]() {
            showThoughtsAction->setIcon(QIcon(_editor->darkMode() ? "images/thinking-dark.svg" : ":images/thinking.svg"));
            showToolMessageAction->setIcon(QIcon(_editor->darkMode() ? "images/tool-dark.svg" : ":images/tool.svg"));
            screenshotButton->setIcon(QIcon(_editor->darkMode() ? "images/camera-dark.svg" : ":images/camera.svg"));
            });

      // --- 2. Chat Display ---
      chatDisplay = new ChatDisplay(_editor, parent);
      chatDisplay->setZoomFactor(1.2);
      chatDisplay->setDarkMode(_editor->darkMode());
      chatDisplay->setup();
      mainLayout->addWidget(chatDisplay->widget(), 1);   // stretch=1: nimmt den gesamten verbleibenden Platz
      connect(_editor, &Editor::darkModeChanged, chatDisplay, &ChatDisplay::setDarkMode);

      // --- 3. Input Row: [DataPanel | UserInput] ---

      // 3b. Prompt input field (zuerst anlegen, damit die Höhe bekannt ist)
      userInput = new DropAwarePlainTextEdit(this);
      userInput->setAcceptDrops(true);
      userInput->setCursorWidth(8);
      userInput->setPlaceholderText("enter message to LLM...");
      userInput->setStyleSheet("QPlainTextEdit { padding: 5px; }");
      QFontMetrics fm(userInput->font());
      const int inputHeight = (fm.lineSpacing() * 4) + 20;
      userInput->setFixedHeight(inputHeight);
      userInput->installEventFilter(this);
      connect(userInput, &DropAwarePlainTextEdit::imageDropped, this, &Agent::onScreenshotReady);

      // 3a. Schmales vertikales Icon-Panel links neben dem Prompt-Eingabefeld
      dataPanel = new QWidget(this);
      dataPanel->setFixedWidth(28);
      dataPanel->setFixedHeight(inputHeight);            // exakt so hoch wie das Eingabefeld
      QVBoxLayout* dataPanelLayout = new QVBoxLayout(dataPanel);
      dataPanelLayout->setContentsMargins(2, 2, 2, 2);
      dataPanelLayout->setSpacing(4);
      dataPanelLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

      // New summary buttons (vertical bar)
      summaryButton = new QToolButton(this);
      summaryButton->setText("∑");
      summaryButton->setToolTip("Summarize conversation");
      connect(summaryButton, &QToolButton::clicked, [this] {
            historyManager->summaryRequested = true;
            QString prompt = "Please provide a concise technical summary of our conversation so far. Focus specifically on the results obtained from the tool calls and the final conclusions reached. Discard the raw, voluminous data output from the tools, but retain the key facts, parameters used, and the current state of the task. This summary will serve as the new starting point for our context, so ensure no critical logical step is lost.";
            sendMessage(prompt);
            });
      dataPanelLayout->addWidget(summaryButton);

      button2 = new QToolButton(this);
      button2->setText(" ");
      dataPanelLayout->addWidget(button2);

      button3 = new QToolButton(this);
      button3->setText(" ");
      dataPanelLayout->addWidget(button3);

      // Save layout pointer so updateDataPanel() can add/remove thumbnail labels dynamically
      _dataPanelLayout = dataPanelLayout;
      _dataPanelLayout->addStretch(1);

      // 3c. Beide Widgets in einer horizontalen Zeile kombinieren
      QWidget* inputRow = new QWidget(this);
      inputRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);  // kein vertikales Wachstum
      QHBoxLayout* inputRowLayout = new QHBoxLayout(inputRow);
      inputRowLayout->setContentsMargins(0, 0, 0, 0);
      inputRowLayout->setSpacing(2);
      inputRowLayout->addWidget(dataPanel);
      inputRowLayout->addWidget(userInput);

      mainLayout->addWidget(inputRow);                   // kein Stretch: nimmt nur den nötigen Platz

      // Toolbar placed below the prompt input
      // mainLayout->addWidget(toolBar);
      loadSettings();

      connect(_editor, &Editor::fontChanged, [this](QFont f) {
            chatDisplay->setFont(f);
            userInput->setFont(f);
            statusLabel->setFont(f);
            modelMenu->setFont(f);
            newSessionButton->setFont(f);
            deleteSessionButton->setFont(f);
            sessionComboBox->setFont(f);
            modeButton->setFont(f);
            });

      connect(chatDisplay, &QWebEngineView::loadFinished, this, [this] { loadStatus(); }, Qt::QueuedConnection | Qt::SingleShotConnection);
      fetchModels();
      spinnerTimer = new QTimer(this);
      connect(spinnerTimer, &QTimer::timeout, this, &Agent::updateSpinner);
      }

//---------------------------------------------------------
//   Agent
//---------------------------------------------------------

Agent::~Agent() {
      delete historyManager;
      }

//---------------------------------------------------------
//   setExecuteMode
//---------------------------------------------------------

void Agent::setExecuteMode(bool checked) {
      _isExecuteMode = checked;
      modeButton->setChecked(checked);
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
      for (const auto& m : _models) {
            if (m.name == s) {
                  pendingModelName.clear();
                  model = m;
                  llm = llmFactory(this, &model, mcpTools);
                  connect(llm, &LLMClient::incomingChunk, this, [this](const std::string& thoughtChunk, const std::string& textChunk) {
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
//   getManifest
//---------------------------------------------------------

std::string Agent::getManifest() {
      if (!_manifestsLoaded) {
            _manifestsLoaded = true;

            // Load Build Manifest
            QString buildPath = _editor->projectRoot() + "/agents-build.md";
            QFile buildFile(buildPath);
            if (buildFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                  QTextStream in(&buildFile);
                  _manifestBuild = in.readAll().toStdString();
            } else {
                  _manifestBuild = manifestBuildDefault;
            }

            // Load Plan Manifest
            QString planPath = _editor->projectRoot() + "/agents-plan.md";
            QFile planFile(planPath);
            if (planFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                  QTextStream in(&planFile);
                  _manifestPlan = in.readAll().toStdString();
            } else {
                  _manifestPlan = manifestPlanDefault;
            }
      }

      return isExecuteMode() ? _manifestBuild : _manifestPlan;
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
                        Model m;
                        m.name            = QString::fromStdString(model["name"].get<std::string>());
                        m.modelIdentifier = m.name;
                        m.baseUrl         = "http://localhost:11434";
                        m.isLocal         = true;
                        m.api             = "ollama";

                        _models.push_back(m);
                        }
                  // at this point we have a complete list of available LL models
                  // and we can select the last used model as saved in settings
                  if (!pendingModelName.isEmpty()) {
                        QString pending = pendingModelName;
                        pendingModelName.clear();
                        setCurrentModel(pending, false);
                        }
                  else if (model.name.isEmpty()) {
                        setCurrentModel(_editor->settingsLLModel(), false);
                        }
                  emit modelsChanged();
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
      std::string text = qtext.trimmed().toStdString();
      if (text.empty() || model.name == "")
            return;

      // If the agent is waiting for a user answer (ask_user tool),
      // capture the reply and unblock the event loop instead of sending to LLM.
      if (_waitingForUserInput) {
            _waitingForUserInput = false;
            _userInputAnswer     = qtext.trimmed();
            userInput->setEnabled(false);
            chatDisplay->startNewStreamingMessage("User");
            chatDisplay->handleIncomingChunk("", text);
            chatDisplay->scrollToBottom();
            return;
            }

      chatDisplay->startNewStreamingMessage("User");
      chatDisplay->handleIncomingChunk("", text);
      chatDisplay->scrollToBottom();

      json msg;
      msg["role"] = "user";
      if (model.api == "gemini")
            msg["parts"] = json::array({{{"text", text}}});
      else // ollama / anthropic / openai
            msg["content"] = text;

      // Attach all pending images (if any) as a base64-encoded JPEG array
      if (!_pendingImages.isEmpty()) {
            json imagesArray = json::array();
            for (const QString& b64 : _pendingImages)
                  imagesArray.push_back(b64.toStdString());
            msg["images"] = imagesArray;
            _pendingImages.clear();
            screenshotButton->setChecked(false);
            screenshotButton->setToolTip("Take Screenshot and attach to next prompt");
            updateDataPanel();
            }

      // Approximate token count: 4 chars per token
      historyManager->addRequest(msg, text.length() / 4);
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
      return text.left(maxChars) + QString("\n\n... [Output truncated. %1 characters omitted for brevity]").arg(removed);
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
//   sendMessage2
//---------------------------------------------------------

void Agent::sendMessage2() {
      setInputEnabled(false);
      retryPause = 2000;

      streamBuffer.clear();
      QNetworkRequest request;
      json jsonPayLoad   = llm->prompt(&request);
      QByteArray payload = QString::fromStdString(jsonPayLoad.dump()).toUtf8();

      Debug("send <{}>", jsonPayLoad.size());
      Log("send <{}>", jsonPayLoad.dump(3));
      request.setTransferTimeout(60000 * 5);

      chatDisplay->startNewStreamingMessage(model.name.toStdString());

      if (currentReply) {
            currentReply->disconnect();
            currentReply->abort();
            currentReply->deleteLater();
            currentReply = nullptr;
            Critical("request already running?");
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

                               chatDisplay->addMessage(
                                   "system", format("<br><font color='orange'><b>[Rate Limit]:</b> Pause for {} seconds...</font><br>",
                                                    waitMs / 1000.0));
                               }
                         // B: Server Error (5xx) -> Exponential Backoff
                         else {
                               waitMs = 2000 * std::pow(2, currentRetryCount);
                               chatDisplay->addMessage(
                                   "system", format("<br><font color='orange'><b>[Server Error {}]:</b> Retry {}/{} in {}s...</font><br>",
                                                    statusCode, currentRetryCount + 1, maxRetries, waitMs / 1000.0));
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
                        chatDisplay->addMessage(
                            "system", format("<br><font color='red'><b>[Abort]:</b> Too many attempts ({}).</font><br>", maxRetries));
                        }
                  }

            QString errorMessage = currentReply->errorString();
            // Generate specific messages
            switch (currentReply->error()) {
                  case QNetworkReply::TimeoutError:
                        errorMessage += "\nTimeout: The LL server did not respond within 60 seconds. Maybe the "
                                        "model is still loading or the server is hanging.";
                        break;
                  case QNetworkReply::ContentNotFoundError: errorMessage += "\nModell not found (bad baseUrl configured?)"; break;
                  default: break;
                  }
            Debug("Network/API error {}: {}", int(currentReply->error()), errorMessage);

            // Show the error to the user in the UI
            chatDisplay->addMessage("system", format("<br><font color='red'><b>[Connection abort]:</b> {}</font><br>", errorMessage));
            currentReply->deleteLater();
            currentReply = nullptr;

            enableInput(true);
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

      updateChatDisplay();
      saveStatus();
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
                              Debug("received <{}>", j.size());
                              Log("received <{}>", j.dump(3));
                              llm->processJsonItem(j);

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
//   saveSettings
//---------------------------------------------------------

void Agent::saveSettings() {
      // Save as JSON
      QJsonArray array;
      for (const auto& m : _models) {
            if (m.isLocal)
                  continue;
            QJsonObject obj;
            obj["name"]               = m.name;
            obj["url"]                = m.baseUrl;
            obj["key"]                = m.apiKey;
            obj["modelId"]            = m.modelIdentifier;
            obj["api"]                = m.api;
            obj["supportsThinking"]   = m.supportsThinking;
            obj["temperature"]        = m.temperature;
            obj["topP"]               = m.topP;
            obj["maxTokens"]          = m.maxTokens;
            array.append(obj);
            }

      QFile file(configPath());
      if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(array).toJson());
            file.close();
            }
      }

//---------------------------------------------------------
//   loadSettings
//---------------------------------------------------------

void Agent::loadSettings() {
      QFile file(configPath());
      if (!file.open(QIODevice::ReadOnly)) {
            Debug("config file <{}> not found", configPath());
            return;
            }

      QByteArray s     = file.readAll();
      QJsonArray array = QJsonDocument::fromJson(s).array();
      _models.clear();

      for (int i = 0; i < array.size(); ++i) {
            QJsonObject obj = array[i].toObject();
            Model m;
            m.name            = obj["name"].toString();
            m.baseUrl         = obj["url"].toString();
            m.apiKey          = obj["key"].toString();
            m.modelIdentifier = obj["modelId"].toString();
            m.api             = obj["api"].toString();
            m.isLocal         = false;
            // Optional fields – graceful fallback to struct defaults if absent
            if (obj.contains("supportsThinking"))
                  m.supportsThinking = obj["supportsThinking"].toBool();
            if (obj.contains("temperature"))
                  m.temperature = obj["temperature"].toDouble(-1.0);
            if (obj.contains("topP"))
                  m.topP = obj["topP"].toDouble(-1.0);
            if (obj.contains("maxTokens"))
                  m.maxTokens = obj["maxTokens"].toInt(-1);

            _models.push_back(m);
            }
      emit modelsChanged();
      }

//---------------------------------------------------------
//   getLastSessionInfo
//    Sucht die aktuellste Datei und liefert Metadaten zurück
//---------------------------------------------------------

SessionInfo Agent::getSessionInfo() const {
      SessionInfo info;
      QString root = _editor->projectRoot();
      if (root.isEmpty()) { // no session, no session info
            Debug("no editor root");
            return info;
            }

      QString sessionFolder = QDir::cleanPath(root + "/.nped");
      QDir dir(sessionFolder);
      // create path if it does not exist
      QDir().mkpath(sessionFolder);

      //      Debug("session folder <{}>", sessionFolder);

      QStringList filters;
      filters << "Session-*.json";
      QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::NoSort);
      QFileInfo latestFileInfo;
      for (const QFileInfo& fileInfo : files) {
            QStringList parts = fileInfo.baseName().split('-');
            if (parts.size() < 5) {
                  Debug("bad filename <{}>", fileInfo.baseName());
                  continue;
                  }
            //            Debug("<{}> {} {} {} {}", fileInfo.baseName(), parts[0], parts[1], parts[2], parts[3], parts[4]);
            QDate currentDate(parts[3].toInt(), parts[2].toInt(), parts[1].toInt());
            int currentNumber = parts[4].toInt();

            if (!info.lastDate.isValid() || currentDate > info.lastDate ||
                (currentDate == info.lastDate && currentNumber > info.lastNumber)) {
                  info.lastDate   = currentDate;
                  info.lastNumber = currentNumber;
                  latestFileInfo  = fileInfo;
                  info.fileName   = fileInfo.absoluteFilePath();
                  }
            }
      return info;
      }

//---------------------------------------------------------
//   sessionName
//---------------------------------------------------------

QString Agent::sessionName(bool getNext) const {
      SessionInfo info = getSessionInfo();
      QDate today      = QDate::currentDate();

      int nextNumber = info.lastNumber + 1; // default: increment
      if (!getNext && info.lastNumber != 0)
            nextNumber = info.lastNumber; // reuse existing number
      else
            nextNumber = info.lastNumber + 1;

      // Format: Session-dd-MM-yyyy-n.json
      // 'z' sorgt dafür, dass führende Nullen bei Tag/Monat erhalten bleiben (dd-MM)

      QString root = _editor->projectRoot();
      if (root.isEmpty()) // no session, no session info
            return QString();

      QString sessionFolder = QDir::cleanPath(root + "/.nped");
      return QString("%5/Session-%1-%2-%3-%4.json")
          .arg(today.day(), 2, 10, QChar('0'))
          .arg(today.month(), 2, 10, QChar('0'))
          .arg(today.year())
          .arg(nextNumber)
          .arg(sessionFolder);
      }

//---------------------------------------------------------
//   startNewSession
//---------------------------------------------------------

void Agent::startNewSession() {
      saveStatus();
      historyManager->clear();
      savedEntries = 0;
      chatDisplay->clear();

      currentSessionFileName = sessionName(true); // create new session file name
      updateSessionList();
      chatDisplay->addMessage("system", format("<i>[System: New session started: <b>{}</b>]</i><br>",
                                               QFileInfo(currentSessionFileName).fileName().toStdString()));
      userInput->setFocus();
      }

//---------------------------------------------------------
//   deleteCurrentSession
//---------------------------------------------------------

void Agent::deleteCurrentSession() {
      if (currentSessionFileName.isEmpty())
            return;

      QFile file(currentSessionFileName);
      if (file.exists()) {
            file.remove();
            //            chatDisplay->append(
            //                QString("<i>[System: Session deleted: <b>%1</b>]</i><br>").arg(QFileInfo(currentSessionFileName).fileName()));
            }

      currentSessionFileName = QString();
      historyManager->clear();
      savedEntries = 0;
      chatDisplay->clear();
      updateSessionList();
      userInput->setFocus();
      }

//---------------------------------------------------------
//   renameCurrentSession
//---------------------------------------------------------

void Agent::renameCurrentSession() {
      if (currentSessionFileName.isEmpty())
            return;

      QFileInfo currentInfo(currentSessionFileName);
      QString   currentBase = currentInfo.completeBaseName(); // ohne ".json"

      bool    ok      = false;
      QString newName = QInputDialog::getText(
            this,
            tr("Rename Session"),
            tr("New session name:"),
            QLineEdit::Normal,
            currentBase,
            &ok);

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
      if (targetPath != currentSessionFileName && QFile::exists(targetPath)) {
            int counter = 2;
            do {
                  targetName = newName + "-" + QString::number(counter++);
                  targetPath = dir + "/" + targetName + "." + suffix;
                  } while (QFile::exists(targetPath));
            }

      // Umbenennen
      if (!QFile::rename(currentSessionFileName, targetPath)) {
            QMessageBox::warning(this, tr("Rename failed"),
                                 tr("Could not rename session file."));
            return;
            }

      currentSessionFileName = targetPath;
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
            QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time); // Sorted by time (newest first)

            for (const QFileInfo& fileInfo : files)
                  sessionComboBox->addItem(fileInfo.baseName(), fileInfo.absoluteFilePath());
            }

      int index = sessionComboBox->findData(currentSessionFileName);
      if (index >= 0) {
            sessionComboBox->setCurrentIndex(index);
            }
      else if (!currentSessionFileName.isEmpty()) {
            QFileInfo fi(currentSessionFileName);
            sessionComboBox->insertItem(0, fi.baseName(), currentSessionFileName);
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
      if (fileName != currentSessionFileName) {
            saveStatus();
            loadStatus(fileName);
            }
      }

//---------------------------------------------------------
//   saveStatus
//---------------------------------------------------------

void Agent::saveStatus() {
      if (currentSessionFileName.isEmpty() || historyManager->empty())
            return;

      QString path = QFileInfo(currentSessionFileName).absolutePath();
      QDir dir;
      if (!dir.exists(path)) {
            bool created = dir.mkpath(path);
            if (!created) {
                  Critical("Could not create directory {}", path);
                  return;
                  }
            }

      std::ios_base::openmode mode = (savedEntries == 0) ? std::ios::out : std::ios::app;
      std::ofstream f(currentSessionFileName.toStdString(), mode);

      if (f.is_open()) {
            if (savedEntries == 0) {
                  json header;
                  header["model"]              = currentModel().toStdString();
                  header["activeEntries"]      = historyManager->getActiveEntriesCount();
                  header["filterToolMessages"] = filterToolMessages;
                  header["filterThoughts"]     = filterThoughts;
                  f << header.dump() << "\n";
                  }
            const auto& data = historyManager->data();
            if (savedEntries < data.size()) {
                  if (savedEntries > 0) {
                        json meta;
                        meta["activeEntries"]      = historyManager->getActiveEntriesCount();
                        meta["filterToolMessages"] = filterToolMessages;
                        meta["filterThoughts"]     = filterThoughts;
                        f << meta.dump() << "\n";
                        }
                  for (size_t i = savedEntries; i < data.size(); ++i)
                        f << data[i].content.dump() << "\n";
                  savedEntries = data.size();
                  }
            f.close();
            }
      else {
            Critical("Could not open session file for writing: {}", currentSessionFileName.toStdString());
            }
      }

//---------------------------------------------------------
//   loadStatus
//    load last session
//---------------------------------------------------------

void Agent::loadStatus(const QString& sessionPath) {
      if (!_editor)
            return;

      QString fileToLoad = sessionPath;
      if (fileToLoad.isEmpty()) {
            auto info  = getSessionInfo();
            fileToLoad = info.fileName;
            }

      if (!fileToLoad.isEmpty()) {
            std::ifstream i(fileToLoad.toStdString());
            if (i.is_open()) {
                  std::string content((std::istreambuf_iterator<char>(i)), std::istreambuf_iterator<char>());
                  try {
                        json root = json::parse(content);
                        if (root.is_object() && root.contains("history")) {
                              if (root.contains("model")) {
                                    std::string modelName = root["model"];
                                    QString m             = QString::fromStdString(modelName);
                                    setCurrentModel(m, false);
                                    }
                               if (root.contains("filterToolMessages")) {
                                     filterToolMessages = root["filterToolMessages"];
                                     if (showToolMessageAction)
                                           showToolMessageAction->setChecked(!filterToolMessages);
                                     }
                               if (root.contains("filterThoughts")) {
                                     filterThoughts = root["filterThoughts"];
                                     if (showThoughtsAction)
                                           showThoughtsAction->setChecked(!filterThoughts);
                                     }
                               const json& history = root["history"];
                               historyManager->setHistory(history);
                               if (root.contains("activeEntries"))
                                     historyManager->setActiveEntries(root["activeEntries"]);
                               }
                         else if (root.is_array()) {
                               historyManager->setHistory(root);
                               }
                         savedEntries           = historyManager->data().size();
                         currentSessionFileName = fileToLoad;
                         updateSessionList();
                         updateChatDisplay();
                         chatDisplay->scrollToBottom();
                         return;
                         }
                   catch (const json::parse_error& e) {
                         // Not a valid full JSON object? Try JSON Lines fallback
                         std::istringstream iss(content);
                         std::string line;
                         json h            = json::array();
                         size_t actEntries = 0;
                         while (std::getline(iss, line)) {
                               if (line.empty())
                                     continue;
                               try {
                                     json obj = json::parse(line);
                                     if (obj.contains("model")) {
                                           std::string modelName = obj["model"];
                                           setCurrentModel(QString::fromStdString(modelName), false);
                                           }
                                     if (obj.contains("activeEntries"))
                                           actEntries = obj["activeEntries"];
                                     if (obj.contains("filterToolMessages")) {
                                           filterToolMessages = obj["filterToolMessages"];
                                           if (showToolMessageAction)
                                                 showToolMessageAction->setChecked(!filterToolMessages);
                                           }
                                     if (obj.contains("filterThoughts")) {
                                           filterThoughts = obj["filterThoughts"];
                                           if (showThoughtsAction)
                                                 showThoughtsAction->setChecked(!filterThoughts);
                                           }
                                     if (obj.contains("role") || obj.contains("parts") || obj.contains("content"))
                                           h.push_back(obj);
                                     }
                               catch (...) {
                                     }
                               }
                         historyManager->setHistory(h);
                         // Only override activeEntries when an explicit value was found in the file.
                         // If actEntries == 0 (old session format without activeEntries metadata),
                         // setHistory() has already set activeEntries = data.size(), which is correct.
                         if (actEntries > 0)
                               historyManager->setActiveEntries(actEntries);
                         savedEntries           = historyManager->data().size();
                         currentSessionFileName = fileToLoad;
                         updateSessionList();
                         updateChatDisplay();
                         chatDisplay->scrollToBottom();
                         return;
                         }
                   }
             else {
                   Debug("no session file found <{}>", fileToLoad);
                   }
             }

      // No last session found -> Start new session
      currentSessionFileName = sessionName(true);
      savedEntries           = 0;
      updateSessionList();
      chatDisplay->addMessage("system", "<i>[System: No previous session found. New session started.]</i><br>");
      historyManager->clear();
      savedEntries = 0;
      }

//---------------------------------------------------------
//   isWorking
//---------------------------------------------------------

bool Agent::isWorking() const {
      return spinnerTimer->isActive();
      }

//---------------------------------------------------------
//   setInputEnabled
//---------------------------------------------------------

void Agent::setInputEnabled(bool enabled) {
      userInput->setEnabled(enabled);
      if (enabled) {
            spinnerTimer->stop();
            statusLabel->setText(">");
            statusLabel->setStyleSheet("color: normal; font-weight: bold;");
            }
      else {
            spinnerFrame = 0;
            spinnerTimer->start(100);
            statusLabel->setStyleSheet("color: #ff8800; font-weight: bold;");
            }
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
                        if (content.contains("role") && (content["role"] == "function" || content["role"] == "tool")) {
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
                                        std::format("\n\n<i>[System: Tool Response: {}()]</i>\n\n", std::string(fr["name"]));
                                    std::string s =
                                        truncateOutput(static_cast<std::string>(fr["response"]["content"]), kChatResultMaxChars);
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

void Agent::updateChatDisplay() {
      chatDisplay->clear();

      size_t totalEntries = historyManager->data().size();
      size_t activeCount = historyManager->getActiveEntriesCount();
      size_t startActiveIdx = totalEntries > activeCount ? totalEntries - activeCount : 0;

      for (size_t i = 0; i < totalEntries; ++i) {
            const auto& item = historyManager->data()[i];
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

            QString s  = chatDisplay->renderMarkdownToHtml(msg);
            QString th = chatDisplay->renderMarkdownToHtml(thought);
            bool isActive = (i == 0 || i >= startActiveIdx);
            chatDisplay->appendStaticHtml(QString::fromStdString(role), s, th, isActive);
            }
      }

//---------------------------------------------------------
//   enableInput
//---------------------------------------------------------

void Agent::enableInput(bool flag) {
      setInputEnabled(flag);
      if (flag)
            userInput->setFocus();
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

      _pendingImages.append(jpegData.toBase64());

      const int count = _pendingImages.size();
      screenshotButton->setChecked(true);
      screenshotButton->setToolTip(
          QString("%1 image(s) attached. Click 📷 to discard all.").arg(count));

      chatDisplay->addMessage(
          "system",
          QString("<i>[Image #%1 attached (%2×%3 px) – will be sent with next prompt. "
                  "Click 📷 to discard all.]</i><br>")
              .arg(count)
              .arg(image.width())
              .arg(image.height())
              .toStdString());
      updateDataPanel();
      }

//---------------------------------------------------------
//   onScreenshotFailed
//    Called when the portal screenshot fails or is cancelled.
//---------------------------------------------------------

void Agent::onScreenshotFailed(const QString& reason) {
      screenshotButton->setChecked(false);
      chatDisplay->addMessage(
          "system",
          std::string("<font color='orange'><i>[Screenshot failed: ") +
              reason.toStdString() + "]</i></font><br>");
      updateDataPanel();
      }

//---------------------------------------------------------
//   updateDataPanel
//    Rebuilds thumbnail labels in the narrow left panel next
//    to the prompt input. Creates one 24×24 thumbnail for
//    every image currently pending in _pendingImages.
//    Old labels are removed from the layout and deleted.
//---------------------------------------------------------

void Agent::updateDataPanel() {
      // Remove and delete all existing thumbnail labels
      for (QLabel* lbl : _imageIconLabels) {
            _dataPanelLayout->removeWidget(lbl);
            delete lbl;
            }
      _imageIconLabels.clear();

      // Remove the trailing stretch so we can re-add it after the new labels
      // Qt stores the stretch item at the last position; removeItem(int) works by index.
      // The simplest approach: just remove the last item if it is a spacer.
      int itemCount = _dataPanelLayout->count();
      if (itemCount > 0) {
            QLayoutItem* last = _dataPanelLayout->itemAt(itemCount - 1);
            if (last && last->spacerItem()) {
                  _dataPanelLayout->removeItem(last);
                  delete last;
                  }
            }

      // Create one thumbnail label per pending image
      for (int i = 0; i < _pendingImages.size(); ++i) {
            const QByteArray raw = QByteArray::fromBase64(_pendingImages[i].toUtf8());
            QImage img;
            img.loadFromData(raw, "JPEG");

            QLabel* lbl = new QLabel(dataPanel);
            lbl->setAlignment(Qt::AlignCenter);
            if (!img.isNull())
                  lbl->setPixmap(QPixmap::fromImage(img).scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            else
                  lbl->setText("🖼");
            lbl->setToolTip(QString("Image #%1 – will be sent with next prompt").arg(i + 1));
            lbl->setVisible(true);
            _dataPanelLayout->addWidget(lbl);
            _imageIconLabels.append(lbl);
            }

      _dataPanelLayout->addStretch(1);
      }
