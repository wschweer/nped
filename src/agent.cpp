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
#include <iostream>

#include "agent.h"
#include "logger.h"
#include "editor.h"
#include "undo.h"
#include "webview.h"
#include "llm.h"
#include "chatdisplay.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

static std::string manifest = "You are an experienced C++ developer. "
                              "Your task is to analyze and write code in the project and to fix build errors.\n\n"
                              "Use modern c++. Prefer object oriented design and use modern design patterns.\n"
                              "ERROR ANALYSIS RULES:\n"
                              "1. If a build fails, analyze the output of 'run_build_command'.\n"
                              "2. Look for lines like 'file.cpp:42:10: error: ...'.\n"
                              "TOOL FORMAT:\n"
                              "Answer exclusively in JSON format when you call a tool:\n"
                              "{\n"
                              "  \"tool\": \"replace_in_file\",\n"
                              "  \"args\": {\n"
                              "    \"path\": \"src/main.cpp\",\n"
                              "    \"search\": \"old code\",\n"
                              "    \"replace\": \"new code\"\n"
                              "  }\n"
                              "}\n\n"
                              "PROJECT STRUCTURE:\n"
                              "Standard Qt6 layout. The build directory is './build'. Use CMake with ninja.\n"
                              "Do not invent your own tasks and never act on your own authority! "
                              "Use the run_build_command tool to compile the project and check if errors occur. ";

//---------------------------------------------------------
//   Agent (Constructor)
//---------------------------------------------------------

Agent::Agent(Editor* e, QWidget* parent) : QWidget(parent), _editor(e), currentReply(nullptr) {
      networkManager = new QNetworkAccessManager(this);
      mcpTools       = getMCPTools();

      QVBoxLayout* mainLayout = new QVBoxLayout(this);

      // --- 1. Toolbar & Model Selection ---
      toolBar = new QToolBar(this);
      toolBar->setStyleSheet("QToolButton {"
                             "  border: 1px solid #888;"
                             "  border-radius: 3px;"
                             "  background-color: #e4e4e4;"
                             "  padding: 4px 8px;"
                             "  margin: 0px 4px;"
                             "}"
                             "QToolButton:hover { background-color: #d0d0d0; }"
                             "QToolButton:pressed { background-color: #a0a0a0; }");

      modelButton = new QToolButton(this);
      modelButton->setText("Model...");
      modelMenu = new QMenu(this);
      modelMenu->setStyleSheet("QMenu { background-color: lightblue; } QMenu::item:selected { background-color: yellow; }");
      modelButton->setMenu(modelMenu);
      modelButton->setPopupMode(QToolButton::InstantPopup);
      toolBar->addWidget(modelButton);
      toolBar->addSeparator();

      newSessionButton = new QToolButton(this);
      newSessionButton->setText("New Session");
      toolBar->addWidget(newSessionButton);
      connect(newSessionButton, &QToolButton::clicked, this, &Agent::startNewSession);
      toolBar->addSeparator();

      modeToggleAction = new QAction("Plan", this);
      modeToggleAction->setCheckable(true);
      modeToggleAction->setChecked(_isExecuteMode);

      connect(this, &Agent::modelChanged, [this] { modelButton->setText(currentModel()); });

      connect(modeToggleAction, &QAction::toggled, this, [this](bool checked) {
            _isExecuteMode = checked;
            modeToggleAction->setText(checked ? "Build" : "Plan");

            if (auto* button = qobject_cast<QToolButton*>(toolBar->widgetForAction(modeToggleAction))) {
                  if (checked) {
                        button->setStyleSheet("QToolButton { background-color: #f0d0d0; border: 1px solid #c88; color: #500; "
                                              "border-radius: 3px; padding: 4px 8px; margin: 0px 4px; }"
                                              "QToolButton:hover { background-color: #e0c0c0; }"
                                              "QToolButton:pressed { background-color: #d0b0b0; }");
                        }
                  else {
                        button->setStyleSheet("QToolButton { background-color: #d0f0d0; border: 1px solid #8c8; color: #050; "
                                              "border-radius: 3px; padding: 4px 8px; margin: 0px 4px; }"
                                              "QToolButton:hover { background-color: #c0e0c0; }"
                                              "QToolButton:pressed { background-color: #b0d0b0; }");
                        }
                  }

            if (chatDisplay) {
                  chatDisplay->append(
                      QString("<br><i>[System: Mode changed to <b>%1</b>]</i>").arg(checked ? "Build (read/write)" : "Plan (read only)"));
                  }
            });
      toolBar->addAction(modeToggleAction);

      QWidget* spacer = new QWidget();
      spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      toolBar->addWidget(spacer);

      // Initial color (Plan mode)
      if (auto* button = qobject_cast<QToolButton*>(toolBar->widgetForAction(modeToggleAction))) {
            if (_isExecuteMode) {
                  button->setStyleSheet("QToolButton { background-color: #f0d0d0; border: 1px solid #c88; color: #500; "
                                        "border-radius: 3px; padding: 4px 8px; margin: 0px 4px; }"
                                        "QToolButton:hover { background-color: #e0c0c0; }"
                                        "QToolButton:pressed { background-color: #d0b0b0; }");
                  modeToggleAction->setText("Build");
                  }
            else {
                  button->setStyleSheet("QToolButton { background-color: #d0f0d0; border: 1px solid #8c8; color: #050; border-radius: 3px; "
                                        "padding: 4px 8px; margin: 0px 4px; }"
                                        "QToolButton:hover { background-color: #c0e0c0; }"
                                        "QToolButton:pressed { background-color: #b0d0b0; }");
                  modeToggleAction->setText("Plan");
                  }
            }
      mainLayout->addWidget(toolBar);

      // --- 2. Chat Display ---
      chatDisplay = new ChatDisplay(_editor, parent);
      chatDisplay->setZoomFactor(1.2);
      chatDisplay->setDarkMode(_editor->darkMode());
      chatDisplay->setup();
      mainLayout->addWidget(chatDisplay->widget());
      connect(_editor, &Editor::darkModeChanged, chatDisplay, &ChatDisplay::setDarkMode);

      // --- 3. Input Field ---
      QHBoxLayout* inputLayout = new QHBoxLayout();
      statusLabel              = new QLabel(">", this);
      userInput                = new QPlainTextEdit(this);
      userInput->setCursorWidth(8);

      statusLabel->setFixedWidth(30);
      statusLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
      statusLabel->setAlignment(Qt::AlignCenter);

      userInput->setPlaceholderText("enter message to LLM...");
      QFontMetrics fm(userInput->font());
      userInput->setFixedHeight((fm.lineSpacing() * 4) + 10);
      userInput->installEventFilter(this);

      inputLayout->addWidget(statusLabel, Qt::AlignCenter);
      inputLayout->addWidget(userInput);
      mainLayout->addLayout(inputLayout);
      loadSettings();

      connect(_editor, &Editor::fontChanged, [this](QFont f) {
            chatDisplay->setFont(f);
            userInput->setFont(f);
            statusLabel->setFont(f);
            modelButton->setFont(f);
            newSessionButton->setFont(f);
            modeToggleAction->setFont(f);
            });

      connect(
          chatDisplay, &QWebEngineView::loadFinished, this,
          [this] {
                loadStatus();
                },
          Qt::QueuedConnection | Qt::SingleShotConnection);
      fetchModels();
      spinnerTimer = new QTimer(this);
      connect(spinnerTimer, &QTimer::timeout, this, &Agent::updateSpinner);
      }

//---------------------------------------------------------
//   setExecuteMode
//---------------------------------------------------------

void Agent::setExecuteMode(bool checked) {
      _isExecuteMode = checked;
      modeToggleAction->setChecked(checked);

      // Update the text and style directly
      modeToggleAction->setText(checked ? "Build" : "Plan");
      if (auto* button = qobject_cast<QToolButton*>(toolBar->widgetForAction(modeToggleAction))) {
            if (checked) {
                  button->setStyleSheet("QToolButton { background-color: #f0d0d0; border: 1px solid #c88; color: #500; "
                                        "border-radius: 3px; padding: 4px 8px; margin: 0px 4px; }"
                                        "QToolButton:hover { background-color: #e0c0c0; }"
                                        "QToolButton:pressed { background-color: #d0b0b0; }");
                  }
            else {
                  button->setStyleSheet("QToolButton { background-color: #d0f0d0; border: 1px solid #8c8; color: #050; "
                                        "border-radius: 3px; padding: 4px 8px; margin: 0px 4px; }"
                                        "QToolButton:hover { background-color: #c0e0c0; }"
                                        "QToolButton:pressed { background-color: #b0d0b0; }");
                  }
            }
      }

//---------------------------------------------------------
//   getConfigPath
// Helper function for the path
//---------------------------------------------------------

QString getConfigPath() {
      QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
      QDir().mkpath(path);
      return path + "/models.json";
      }

//---------------------------------------------------------
//   setCurrentModel
//---------------------------------------------------------

void Agent::setCurrentModel(const QString& s) {
      if (model.name == s)
            return;
      for (const auto& m : _models) {
            if (m.name == s) {
                  model = m;
                  llm   = llmFactory(this, &model, mcpTools);
                  connect(llm, &LLMClient::incomingChunk, chatDisplay, &ChatDisplay::handleIncomingChunk);
                  currentRetryCount  = 0;
                  retryPause         = 2000;
                  rateLimitResetTime = QDateTime();
                  chatHistory.clear();

                  chatDisplay->clear();
                  chatDisplay->append("<i>[System: New session started. Ready.]</i><br>");
                  emit modelChanged();
                  return;
                  }
            }
      Critical("model <{}> not found", s);
      }

//---------------------------------------------------------
//   getManifest
//---------------------------------------------------------

std::string Agent::getManifest() const {
      QString fullPath = _editor->projectRoot() + "/agents.md";
      QFile file(fullPath);
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            manifest += in.readAll().toStdString();
            return manifest + in.readAll().toStdString();
            }
      return manifest;
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

                        QAction* action = modelMenu->addAction(m.name);
                        connect(action, &QAction::triggered, [this, m] { setCurrentModel(m.name); });
                        _models.push_back(m);
                        }
                  // at this point we have a complete list of available LL models
                  // and we can select the last used model as saved in settings
                  setCurrentModel(_editor->settingsLLModel());
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

void Agent::sendMessage() {
      Debug("=============<{}>", userInput->toPlainText());

      std::string text = userInput->toPlainText().trimmed().toStdString();
      if (text.empty() || model.name == "")
            return;

      userInput->clear();
      chatDisplay->startNewStreamingMessage("User");
      chatDisplay->handleIncomingChunk("", QString::fromStdString(text));
      json msg;
      msg["role"] = "user";
      if (model.api == "gemini") {
            msg["parts"] = json::array({{{"text", text}}});
            chatHistory.push_back(msg);
            }
      else { // ollama
            msg["content"] = text;
            chatHistory.push_back(msg);
            }
      sendMessage2();
      }

//---------------------------------------------------------
//   trimHistory
//---------------------------------------------------------

void Agent::trimHistory() {
      if (chatHistory.size() <= kChatMaxMessages)
            return;

      // We need a new container
      json newHistory;

      // 1. Always keep system prompt!
      if (!chatHistory.empty())
            newHistory.push_back(chatHistory[0]);

      // 2. Calculate where to start copying from (the last N)
      unsigned startIdx = chatHistory.size() - (kChatMaxMessages - 1);

      // Make sure we don't cut in the middle of a tool flow
      // If the message at startIdx is a "tool_result", we need it
      // the "tool_use" before!
      while (startIdx < chatHistory.size()) {
            const auto& msg = chatHistory[startIdx];

            // Check if we just have an orphaned tool result
            bool isToolResult = (msg.value("role", "") == "tool") ||
                                (msg.value("role", "") == "user" && msg.contains("content") && msg["content"].is_array());

            if (isToolResult) {
                  // We can't cut here, we have to start one earlier
                  startIdx--;
                  }
            else {
                  break; // Everything okay, this is a "clean" message (usually user text)
                  }

            if (startIdx <= 1)
                  break; // Do not overwrite the system prompt
            }

      // 3. Copy the rest
      for (size_t i = startIdx; i < chatHistory.size(); ++i)
            newHistory.push_back(chatHistory[i]);

      chatHistory = newHistory;

      // Log for control
      Debug("History trimmed to {} messages.", chatHistory.size());
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
      return text.substr(0, maxChars) + std::format("\n\n... [Output truncated. {} characters omitted for brevity]", removed);
      }

//---------------------------------------------------------
//   sendMessage2
//---------------------------------------------------------

void Agent::sendMessage2() {
      setInputEnabled(false);
      retryPause = 2000;

      streamBuffer.clear();
      QNetworkRequest request;
      trimHistory();
      json jsonPayLoad   = llm->prompt(&request);
      QByteArray payload = QString::fromStdString(jsonPayLoad.dump()).toUtf8();

      //      Debug("send <{}>", jsonPayLoad.dump(3));
      request.setTransferTimeout(60000 * 5);

      chatDisplay->startNewStreamingMessage(model.name);

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
      QByteArray newData = currentReply->readAll();
      streamBuffer.append(newData);
      processData();
      llm->dataFinished(currentReply);
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
            obj["name"]    = m.name;
            obj["url"]     = m.baseUrl;
            obj["key"]     = m.apiKey;
            obj["modelId"] = m.modelIdentifier;
            obj["api"]     = m.api;
            array.append(obj);
            }

      QFile file(getConfigPath());
      if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(array).toJson());
            file.close();
            }
      }

//---------------------------------------------------------
//   loadSettings
//---------------------------------------------------------

void Agent::loadSettings() {
      QFile file(getConfigPath());
      if (!file.open(QIODevice::ReadOnly)) {
            Debug("config file <{}> not found", getConfigPath());
            return;
            }

      QByteArray s     = file.readAll();
      QJsonArray array = QJsonDocument::fromJson(s).array();
      _models.clear();
      modelMenu->clear();

      for (int i = 0; i < array.size(); ++i) {
            QJsonObject obj = array[i].toObject();
            Model m;
            m.name            = obj["name"].toString();
            m.baseUrl         = obj["url"].toString();
            m.apiKey          = obj["key"].toString();
            m.modelIdentifier = obj["modelId"].toString();
            m.api             = obj["api"].toString();
            m.isLocal         = false;

            _models.push_back(m);
            QAction* action = modelMenu->addAction(m.name);
            connect(action, &QAction::triggered, [this, m] {
                  setCurrentModel(m.name);
                  modelButton->setText(m.name);
                  });
            }
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

      Debug("session folder <{}>", sessionFolder);

      QStringList filters;
      filters << "Session-*.json";
      QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::NoSort);
      QFileInfo latestFileInfo;
      Debug("<{}> files", files.size());
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
      Debug("session file: <{}>", info.fileName);
      return info;
      }

//---------------------------------------------------------
//   sessionName
//---------------------------------------------------------

QString Agent::sessionName(bool getNext) const {
      SessionInfo info = getSessionInfo();
      QDate today      = QDate::currentDate();

      int nextNumber;
      if (info.lastNumber == 0 || getNext)
            nextNumber = info.lastNumber + 1;

      // Format: Session-dd-MM-yyyy-n.json
      // 'z' sorgt dafür, dass führende Nullen bei Tag/Monat erhalten bleiben (dd-MM)

      QString root = _editor->projectRoot();
      if (root.isEmpty()) // no session, no session info
            return QString();
      root += "/.nped";

      QString sessionFolder = QDir::cleanPath(root + "/.nped");
      return QString("%5/Session-%1-%2-%3-%4.json")
          .arg(today.day(), 2, 10, QChar('0'))
          .arg(today.month(), 2, 10, QChar('0'))
          .arg(today.year())
          .arg(nextNumber)
          .arg(root);
      }

//---------------------------------------------------------
//   startNewSession
//---------------------------------------------------------

void Agent::startNewSession() {
      saveStatus();
      chatHistory.clear();
      updateChatDisplay();

      currentSessionFileName = sessionName(true);
      chatDisplay->append(QString("<i>[System: New session started: <b>%1</b>]</i><br>").arg(QFileInfo(currentSessionFileName).fileName()));
      userInput->setFocus();
      }

//---------------------------------------------------------
//   commitGitChanges
//---------------------------------------------------------

bool Agent::commitGitChanges(const QString& commitMessage) {
      if (!_editor)
            return false;

      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QProcess process;
      process.setWorkingDirectory(projRoot);

      // 1. Check for uncommitted changes
      process.start("git", QStringList() << "status" << "--porcelain");
      process.waitForFinished();
      QByteArray output = process.readAllStandardOutput();

      if (output.trimmed().isEmpty()) {
            Debug("Git: No changes found. Skip commit.");
            return false;
            }

      chatDisplay->append("<i>[System: Changes detected, executing Git auto-commit...]</i>");

      // 2. Stage all changes (git add .)
      process.start("git", QStringList() << "add" << ".");
      process.waitForFinished();

      // 3. Commit changes
      process.start("git", QStringList() << "commit" << "-m" << commitMessage);
      process.waitForFinished();

      chatDisplay->append("<i>[System: Git auto-commit successfully completed.]</i>");
      return true;
      }

//---------------------------------------------------------
//   saveStatus
//---------------------------------------------------------

void Agent::saveStatus() {
      if (currentSessionFileName.isEmpty())
            return;

//      Debug("session <{}>", currentSessionFileName);
      QString path = QFileInfo(currentSessionFileName).absolutePath();
      QDir dir;
      dir.mkpath(path);
//      Debug("mkpath <{}>", path);

      std::ofstream f(currentSessionFileName.toStdString());
      if (f.is_open()) {
            f << "  ";
            f << chatHistory.dump(3);
            f.close();
            }
      else {
            Critical("Could not save session file: {}", currentSessionFileName);
            }
      }

//---------------------------------------------------------
//   loadStatus
//---------------------------------------------------------

void Agent::loadStatus() {
      if (!_editor)
            return;

      auto info = getSessionInfo();
      if (!info.fileName.isEmpty()) {
            try {
                  std::ifstream i(info.fileName.toStdString());
                  i >> chatHistory;
                  //                  chatDisplay->append(QString("<i>[System: Session loaded: <b>%1</b>]</i><br>").arg(QFileInfo(currentSessionFileName).fileName()));
                  currentSessionFileName = info.fileName;
                  updateChatDisplay();
                  chatDisplay->scrollToBottom();
                  return;
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
      else
            Debug("no session file found <{}>", info.fileName);
      // No last session found -> Start new session
      currentSessionFileName = sessionName(true);
      chatDisplay->append("<i>[System: No previous session found. New session started.]</i><br>");
      chatDisplay->scrollToBottom();
      chatHistory.clear();
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
      if (obj == userInput && event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                  if (keyEvent->modifiers() & Qt::ShiftModifier)
                        return false;
                  sendMessage();
                  return true;
                  }
            }
      return QWidget::eventFilter(obj, event);
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
      if (!content.contains("parts")) {
            if (content.contains("content")) { // ollama
                  std::string s = truncateOutput(content["content"].get<std::string>(), kChatResultMaxChars);
                  if (content.contains("role") && content["role"] == "function") {
                        if (content.contains("function")) {
                              json fc             = content["function"];
                              std::string argsStr = "";
                              bool first          = true;
                              for (auto& [key, value] : fc["arguments"].items()) {
                                    if (!first)
                                          argsStr += ", ";
                                    argsStr += std::format("{}={}", key, value.dump());
                                    first    = false;
                                    }
                              msg += std::format("\n\n<i>[System: Führe Tool aus: {}({})]</i>\n\n", static_cast<std::string>(fc["name"]),
                                                 argsStr);
                              }
                        msg += std::format("```\n{}\n```\n\n", s);
                        }
                  else
                        msg += s;
                  }
            return;
            }
      // gemini:
      for (const auto& part : content["parts"]) {
            if (part.contains("text")) {
                  if (part.contains("thought") && part["thought"] == true)
                        thought += part["text"];
                  else
                        msg += part["text"];
                  }
            if (part.contains("functionResponse")) {
                  json fr        = part["functionResponse"];
                  std::string s  = truncateOutput(static_cast<std::string>(fr["response"]["content"]), kChatResultMaxChars);
                  msg           += std::format("```\n{}\n```\n\n", s);
                  }
            if (part.contains("functionCall")) {
                  json fc             = part["functionCall"];
                  std::string argsStr = "";
                  bool first          = true;
                  for (auto& [key, value] : fc["args"].items()) {
                        if (!first)
                              argsStr += ", ";
                        argsStr += std::format("{}={}", key, value.dump());
                        first    = false;
                        }
                  msg += std::format("\n\n<i>[System: Führe Tool aus: {}({})]</i>\n\n", static_cast<std::string>(fc["name"]), argsStr);
                  }
            }
      }

//---------------------------------------------------------
//   updateChatDisplay
//---------------------------------------------------------

void Agent::updateChatDisplay() {
      chatDisplay->clear();
      // Wait for the web view to finish loading before appending messages
      QTimer::singleShot(250, this, [this]() {
            std::string lastRole;
            std::string mergedMsg;
            std::string mergedThought;

            for (const auto& content : chatHistory) {
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
                  if (msg.empty() && thought.empty()) {
                        // Debug("chatHistory entry: empty");
                        continue;
                        }

                  if (role == "function" || role == "tool") {
                        if (lastRole.empty())
                              lastRole = role;
                        mergedMsg     += msg;
                        mergedThought += thought;
                        }
                  else {
                        if (!lastRole.empty() && !(mergedMsg.empty() && mergedThought.empty())) {
                              QString s  = chatDisplay->renderMarkdownToHtml(QString::fromStdString(mergedMsg));
                              QString th = chatDisplay->renderMarkdownToHtml(QString::fromStdString(mergedThought));
                              chatDisplay->appendStaticHtml(QString::fromStdString(lastRole), s, th);
                              }
                        lastRole      = role;
                        mergedMsg     = msg;
                        mergedThought = thought;
                        }
                  }

            if (!lastRole.empty() && !(mergedMsg.empty() && mergedThought.empty())) {
                  QString s  = chatDisplay->renderMarkdownToHtml(QString::fromStdString(mergedMsg));
                  QString th = chatDisplay->renderMarkdownToHtml(QString::fromStdString(mergedThought));
                  chatDisplay->appendStaticHtml(QString::fromStdString(lastRole), s, th);
                  chatDisplay->scrollToBottom();
                  }
            });
      }

//---------------------------------------------------------
//   enableInput
//---------------------------------------------------------

void Agent::enableInput(bool flag) {
      setInputEnabled(flag);
      if (flag)
            userInput->setFocus();
      }
