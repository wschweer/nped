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
#include <iostream>
#include <fstream>

#include "agent.h"
#include "logger.h"
#include "editor.h"
#include "undo.h"
#include "webview.h"
#include "llm.h"
#include "chatdisplay.h"
#include "historymanager.h"

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
      historyManager = new HistoryManager();
      mcpTools       = getMCPTools();

      QVBoxLayout* mainLayout = new QVBoxLayout(this);

      // --- 1. Toolbar & Model Selection ---
      toolBar = new QToolBar(this);

      modelMenu = new QComboBox(this);
      modelMenu->setMinimumWidth(250);
      toolBar->addWidget(modelMenu);
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

      toolBar->addSeparator();

      sessionComboBox = new QComboBox(this);
      sessionComboBox->setMinimumWidth(230);
      toolBar->addWidget(sessionComboBox);
      connect(sessionComboBox, &QComboBox::activated, this, &Agent::onSessionSelected);

      newSessionButton = new QToolButton(this);
      newSessionButton->setText("+");
      newSessionButton->setToolTip("New Session");
      toolBar->addWidget(newSessionButton);
      connect(newSessionButton, &QToolButton::clicked, this, &Agent::startNewSession);

      deleteSessionButton = new QToolButton(this);
      deleteSessionButton->setObjectName("deleteSessionButton");
      deleteSessionButton->setToolTip("Delete Session");
      toolBar->addWidget(deleteSessionButton);
      connect(deleteSessionButton, &QToolButton::clicked, this, &Agent::deleteCurrentSession);

      QWidget* spacer = new QWidget();
      spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      toolBar->addWidget(spacer);

      modeButton = new QToolButton(this);
      modeButton->setCheckable(true);
      if (!_editor->projectMode()) {
            _isExecuteMode = false;
            modeButton->setEnabled(false);
            Debug("disable execute mode");
            }
      else
            Debug("enable execute mode");
      modeButton->setChecked(_isExecuteMode);
      modeButton->setEnabled(_editor->projectMode());
      modeButton->setText(_isExecuteMode ? "Build" : "Plan");
      modeButton->setProperty("class", _isExecuteMode ? "errorButton" : "actionButton");
      toolBar->addWidget(modeButton);

      connect(modeButton, &QToolButton::toggled, [this](bool checked) {
            if (!_editor->projectMode())
                  return;
            _isExecuteMode = checked;
            modeButton->setText(checked ? "Build" : "Plan");
            modeButton->setProperty("class", checked ? "errorButton" : "actionButton");
            modeButton->style()->unpolish(modeButton);
            modeButton->style()->polish(modeButton);

            if (chatDisplay) {
                  chatDisplay->append(
                      QString("<br><i>[System: Mode changed to <b>%1</b>]</i>").arg(checked ? "Build (read/write)" : "Plan (read only)"));
                  }
            });

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
      statusLabel              = new QToolButton(this);
      statusLabel->setText(">");
      connect(statusLabel, &QToolButton::clicked, [this] {
            QString s = userInput->toPlainText();
            if (!s.isEmpty()) {
                  sendMessage(s);
                  userInput->clear();
                  }
            });

      userInput = new QPlainTextEdit(this);
      userInput->setCursorWidth(8);

      statusLabel->setMinimumWidth(30);
      statusLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
      //      statusLabel->setAlignment(Qt::AlignCenter);

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

QString getConfigPath() {
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
                  llm   = llmFactory(this, &model, mcpTools);
                  connect(llm, &LLMClient::incomingChunk, chatDisplay, &ChatDisplay::handleIncomingChunk);
                  currentRetryCount  = 0;
                  retryPause         = 2000;
                  rateLimitResetTime = QDateTime();

                  if (clearChat) {
                        historyManager->clear();
      savedEntries = 0;
                        chatDisplay->clear();
                        chatDisplay->append("<i>[System: New session started. Ready.]</i><br>");
                        }
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

      chatDisplay->startNewStreamingMessage("User");
      chatDisplay->handleIncomingChunk("", text);

      json msg;
      msg["role"] = "user";
      if (model.api == "gemini")
            msg["parts"] = json::array({{{"text", text}}});
      else // ollama
            msg["content"] = text;
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

      Debug("send <{}>", jsonPayLoad.dump(3));
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

      int nextNumber;
      if (info.lastNumber == 0 || getNext)
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
                  header["model"] = currentModel().toStdString();
                  header["activeEntries"] = historyManager->getActiveEntriesCount();
                  f << header.dump() << "\n";
            }
            const auto& data = historyManager->data();
            if (savedEntries < data.size()) {
                  if (savedEntries > 0) {
                        json meta;
                        meta["activeEntries"] = historyManager->getActiveEntriesCount();
                        f << meta.dump() << "\n";
                  }
                  for (size_t i = savedEntries; i < data.size(); ++i) {
                        f << data[i].content.dump() << "\n";
                  }
                  savedEntries = data.size();
            }
            f.close();
      } else {
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
                                    std::string model = root["model"];
                                    QString m         = QString::fromStdString(model);
                                    setCurrentModel(m, false);
                                    }
                              const json& history = root["history"];
                              historyManager->setHistory(history);
                              if (root.contains("activeEntries")) {
                                    historyManager->setActiveEntries(root["activeEntries"]);
                                    }
                              }
                        else if (root.is_array()) {
                              historyManager->setHistory(root);
                              }
                        savedEntries = historyManager->data().size();
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
                        json h = json::array();
                        size_t actEntries = 0;
                        while (std::getline(iss, line)) {
                              if (line.empty()) continue;
                              try {
                                    json obj = json::parse(line);
                                    if (obj.contains("model")) {
                                          std::string model = obj["model"];
                                          setCurrentModel(QString::fromStdString(model), false);
                                          }
                                    if (obj.contains("activeEntries")) {
                                          actEntries = obj["activeEntries"];
                                          }
                                    if (obj.contains("role") || obj.contains("parts") || obj.contains("content")) {
                                          h.push_back(obj);
                                          }
                              } catch (...) {}
                        }
                        historyManager->setHistory(h);
                        historyManager->setActiveEntries(actEntries);
                        savedEntries = historyManager->data().size();
                        currentSessionFileName = fileToLoad;
                        updateSessionList();
                        updateChatDisplay();
                        chatDisplay->scrollToBottom();
                        return;
                  }
            } else {
                  Debug("no session file found <{}>", fileToLoad);
            }
      }
      
      // No last session found -> Start new session
      currentSessionFileName = sessionName(true);
      savedEntries = 0;
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
      if (obj == userInput && event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                  if (keyEvent->modifiers() & Qt::ShiftModifier)
                        return false;
                  sendMessage(userInput->toPlainText());
                  userInput->clear();
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
      try {
            if (!content.contains("parts")) {
                  // ollama
                  if (content.contains("content")) {
                        json c = content["content"];
                        std::string s;
                        if (c.is_array())
                              for (auto& e : c)
                                    s += e.get<std::string>();
                        else
                              s = truncateOutput(c.get<std::string>(), kChatResultMaxChars);
                        if (content.contains("role") && (content["role"] == "function" || content["role"] == "tool")) {
                              if (content.contains("function")) {
                                    json fc  = content["function"];
                                    msg     += formatToolCall(fc["name"], fc["arguments"], s);
                                    }
                              }
                        else
                              msg += s;
                        }
                  if (content.contains("tool_calls")) {
                        for (const auto& tool : content["tool_calls"]) {
                              if (tool.contains("function")) {
                                    json fc  = tool["function"];
                                    msg     += formatToolCall(fc["name"], fc["arguments"], "");
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
                              json fr            = part["functionResponse"];
                              std::string output = std::format("\n\n<i>[System: Tool Response: {}()]</i>\n\n", std::string(fr["name"]));
                              std::string s      = truncateOutput(static_cast<std::string>(fr["response"]["content"]), kChatResultMaxChars);
                              msg += std::format("\n\n```\n{}\n```\n\n", s);
                              }
                        if (part.contains("functionCall")) {
                              json fc  = part["functionCall"];
                              msg     += formatToolCall(fc["name"], fc["args"]);
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

      // Wait for the web view to finish loading before appending messages
      //      QTimer::singleShot(250, this, [this]() {
      std::string lastRole;
      std::string mergedMsg;
      std::string mergedThought;

      for (const auto& item : historyManager->data()) {
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

            /*            if (role == "function" || role == "tool") {
                  if (lastRole.empty())
                        lastRole = role;
                  mergedMsg     += msg;
                  mergedThought += thought;
                                                }
            else {
*/
            if (!lastRole.empty() && !(mergedMsg.empty() && mergedThought.empty())) {
                  QString s  = chatDisplay->renderMarkdownToHtml(mergedMsg);
                  QString th = chatDisplay->renderMarkdownToHtml(mergedThought);
                  chatDisplay->appendStaticHtml(QString::fromStdString(lastRole), s, th);
                  }
            lastRole      = role;
            mergedMsg     = msg;
            mergedThought = thought;
            //                  }
            }

      if (!lastRole.empty() && !(mergedMsg.empty() && mergedThought.empty())) {
            QString s  = chatDisplay->renderMarkdownToHtml(mergedMsg);
            QString th = chatDisplay->renderMarkdownToHtml(mergedThought);
            chatDisplay->appendStaticHtml(QString::fromStdString(lastRole), s, th);
            chatDisplay->scrollToBottom();
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
