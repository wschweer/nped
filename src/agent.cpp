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
#include <fstream>

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

Agent::Agent(Editor* e, QWidget* parent) : QWidget(parent), _editor(e) {
      networkManager = new QNetworkAccessManager(this);
      mcpTools       = getMCPTools();

      QVBoxLayout* mainLayout = new QVBoxLayout(this);

      // --- 1. Toolbar & Model Selection ---
      toolBar = new QToolBar(this);

      modelButton = new QToolButton(this);
      modelButton->setText("Model...");
      modelMenu = new QMenu(this);
      modelButton->setMenu(modelMenu);
      modelButton->setPopupMode(QToolButton::InstantPopup);
      toolBar->addWidget(modelButton);
      toolBar->addSeparator();

      newSessionButton = new QToolButton(this);
      newSessionButton->setText("New Session");
      newSessionButton->setProperty("class", "actionButton");
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
                  button->setProperty("class", checked ? "errorButton" : "actionButton");
                  button->style()->unpolish(button);
                  button->style()->polish(button);
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

      // Initial state
      if (auto* button = qobject_cast<QToolButton*>(toolBar->widgetForAction(modeToggleAction))) {
            button->setProperty("class", _isExecuteMode ? "errorButton" : "actionButton");
            modeToggleAction->setText(_isExecuteMode ? "Build" : "Plan");
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

      connect(chatDisplay, &QWebEngineView::loadFinished, this, [this] { loadStatus(); }, Qt::QueuedConnection | Qt::SingleShotConnection);
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
            button->setProperty("class", checked ? "errorButton" : "actionButton");
            button->style()->unpolish(button);
            button->style()->polish(button);
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
            std::string content = in.readAll().toStdString();
            return manifest + content;
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

void Agent::sendMessage(QString qtext) {
      Debug("=============<{}>", qtext);

      std::string text = qtext.trimmed().toStdString();
      if (text.empty() || model.name == "")
            return;

      userInput->clear();
      chatDisplay->startNewStreamingMessage("User");
      chatDisplay->handleIncomingChunk("", QString::fromStdString(text));

      json msg;
      msg["role"] = "user";
      if (model.api == "gemini")
            msg["parts"] = json::array({{{"text", text}}});
      else // ollama
            msg["content"] = text;
      // Approximate token count: 4 chars per token
      chatHistory.addRequest(msg, text.length() / 4);
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

      Debug("send <{}>", jsonPayLoad.dump(3));
      request.setTransferTimeout(60000 * 5);

      chatDisplay->startNewStreamingMessage(model.name);

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

                              chatDisplay->append(
                                  QString("<br><font color='orange'><b>[Rate Limit]:</b> Pause for %1 seconds...</font><br>")
                                      .arg(waitMs / 1000.0));
                              }
                        // B: Server Error (5xx) -> Exponential Backoff
                        else {
                              waitMs = 2000 * std::pow(2, currentRetryCount);
                              chatDisplay->append(
                                  QString("<br><font color='orange'><b>[Server Error %1]:</b> Retry %2/%3 in %4s...</font><br>")
                                      .arg(statusCode)
                                      .arg(currentRetryCount + 1)
                                      .arg(maxRetries)
                                      .arg(waitMs / 1000.0));
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
                        chatDisplay->append(
                            QString("<br><font color='red'><b>[Abort]:</b> Too many attempts (%1).</font><br>").arg(maxRetries));
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
            chatDisplay->append(QString("<br><font color='red'><b>[Connection abort]:</b> %1</font><br>").arg(errorMessage));
            currentReply->deleteLater();
            currentReply = nullptr;
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
                              Debug("received <{}>", j.dump(3));
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
            f << chatHistory.history().dump(3);
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
                  json h;
                  i >> h;
                  chatHistory.setHistory(h);
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
                  sendMessage(userInput->toPlainText());
                  return true;
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
      if (!content.contains("parts")) {
            // ollama
            if (content.contains("content")) {
                  std::string s = truncateOutput(content["content"].get<std::string>(), kChatResultMaxChars);
                  if (content.contains("role") && content["role"] == "function") {
                        if (content.contains("function")) {
                              json fc  = content["function"];
                              msg     += formatToolCall(fc["name"], fc["arguments"], s);
                              }
                        }
                  else
                        msg += s;
                  }
            return;
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
                        json fr             = part["functionResponse"];
                        std::string output  = std::format("\n\n<i>[System: Tool Response: {}()]</i>\n\n", std::string(fr["name"]));
                        std::string s       = truncateOutput(static_cast<std::string>(fr["response"]["content"]), kChatResultMaxChars);
                        msg                += std::format("\n\n```\n{}\n```\n\n", s);
                        }
                  if (part.contains("functionCall")) {
                        json fc  = part["functionCall"];
                        msg     += formatToolCall(fc["name"], fc["args"]);
                        }
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

            for (const auto& item : chatHistory.data) {
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

//---------------------------------------------------------------------------------------
//   trim
//---------------------------------------------------------------------------------------

bool HistoryManager::trim() {
      // 1. Wenn der letzte Turn eine Zusammenfassung war:
      // Wir löschen ALLES vor dieser Zusammenfassung, da sie nun der neue Kontext-Anker ist.
      if (summaryRequested) {
            if (data.size() >= 1) {
                  HistoryItem lastEntry = data.back();
                  data.clear();
                  data.push_back(lastEntry);
                  totalTokens = lastEntry.tokens;
                  }
            summaryRequested = false;
            return false;
            }

      // 2. Klassisches Rolling Window (von vorne kürzen)
      while (data.size() > maxEntries)
            if (data.size() >= 2) {
                  totalTokens -= data.front().tokens;
                  data.erase(data.begin());
                  totalTokens -= data.front().tokens;
                  data.erase(data.begin());
                  }
            else
                  break;
      if (hitLimit()) {
            // request summary
            std::string text = "Please provide a concise technical summary of our conversation so far. "
                               "Focus specifically on the results obtained from the tool calls and the final "
                               "conclusions reached. Discard the raw, voluminous data output from the tools, "
                               "but retain the key facts, parameters used, and the current state of the task. "
                               "This summary will serve as the new starting point for our context, "
                               "so ensure no critical logical step is lost.";
            json msg;
            msg["role"]  = "user";
            msg["parts"] = json::array({{{"text", text}}});
            addRequest(msg, text.length() / 4);
            summaryRequested = true;
            }
      return summaryRequested;
      }

//---------------------------------------------------------
//   addResult
//---------------------------------------------------------

bool HistoryManager::addResult(const json& content, size_t tokens) {
      bool needSummary = trim();
      data.push_back({content, tokens});
      totalTokens += tokens;
      return needSummary;
      }
