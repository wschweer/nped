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

#include "agent.h"
#include "logger.h"
#include "editor.h"
#include "undo.h"
#include "configdialog.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

//---------------------------------------------------------
//   Agent (Constructor)
//---------------------------------------------------------

Agent::Agent(Editor* e, QWidget* parent) : QWidget(parent), _editor(e), currentReply(nullptr) {
      networkManager  = new QNetworkAccessManager(this);
      chatHistory     = json::array();
      toolsDefinition = getToolsDefinition();

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
      modeToggleAction->setChecked(isExecuteMode);

      connect(this, &Agent::modelChanged, [this] { modelButton->setText(currentModel()); });

      connect(modeToggleAction, &QAction::toggled, this, [this](bool checked) {
            isExecuteMode = checked;
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
                  chatDisplay->append(QString("<br><i>[System: Mode changed to <b>%1</b>]</i>")
                                          .arg(checked ? "Build (read/write)" : "Plan (read only)"));
                  }
            });
      toolBar->addAction(modeToggleAction);

      QWidget* spacer = new QWidget();
      spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      toolBar->addWidget(spacer);

      configButton = new QToolButton(this);
      configButton->setText("⚙");
      configButton->setToolTip("Config");
      configButton->setStyleSheet("QToolButton { background: transparent; border: none; }"
                                  "QToolButton:hover { background: rgba(128, 128, 128, 50); border: 1px solid gray; border-radius: 3px; }");
      toolBar->addWidget(configButton);
      connect(configButton, &QToolButton::clicked, this, &Agent::openConfigDialog);

      // Initial color (Plan mode)
      if (auto* button = qobject_cast<QToolButton*>(toolBar->widgetForAction(modeToggleAction))) {
            button->setStyleSheet("QToolButton { background-color: #d0f0d0; border: 1px solid #8c8; color: #050; border-radius: 3px; "
                                  "padding: 4px 8px; margin: 0px 4px; }"
                                  "QToolButton:hover { background-color: #c0e0c0; }"
                                  "QToolButton:pressed { background-color: #b0d0b0; }");
            }
      mainLayout->addWidget(toolBar);

      // --- 2. Chat Display ---
      chatDisplay = new QTextEdit(this);
      chatDisplay->setReadOnly(true);
      mainLayout->addWidget(chatDisplay);

      // --- 3. Input Field ---
      QHBoxLayout* inputLayout = new QHBoxLayout();
      statusLabel              = new QLabel(">", this);
      userInput                = new QPlainTextEdit(this);

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

      connect(_editor, &Editor::fontChanged, [this](QFont f) {
            chatDisplay->setFont(f);
            userInput->setFont(f);
            statusLabel->setFont(f);
            modelButton->setFont(f);
            newSessionButton->setFont(f);
            modeToggleAction->setFont(f);
            });

      loadSettings();
      fetchModels();
      spinnerTimer = new QTimer(this);
      connect(spinnerTimer, &QTimer::timeout, this, &Agent::updateSpinner);
      configDialog = new ConfigDialog(this, _editor);
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
      Debug("{}", s);
      for (const auto& m : _models) {
            if (m.name == s) {
                  if (model.name != s) {
                        model = m;
                        emit modelChanged();
                        }
                  return;
                  }
            }
      Critical("model <{}> not found", s);
      }

//---------------------------------------------------------
//   connectToServer
//---------------------------------------------------------

void Agent::connectToServer(const QString& url) {
    model.baseUrl = url;

    currentRetryCount  = 0;
    retryPause         = 2000;
    rateLimitResetTime = QDateTime();

    chatHistory = json::array();
    json systemMsg;
    systemMsg["role"] = "system";

    std::string manifest = "You are an experienced C++ developer. "
                           "Your task is to analyze and write code in the project and to fix build errors.\n\n"
                           "ERROR ANALYSIS RULES:\n"
                           "1. If a build fails, analyze the output of 'run_build'.\n"
                           "2. Look for lines like 'file.cpp:42:10: error: ...'.\n"
                           "3. Use 'read_file' to read the context around the faulty line.\n"
                           "4. Use 'modify_file' to correct the error in a targeted manner.\n\n"
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
                           "Preferably use the replace_in_file tool for changes to files. Make sure that the "
                           "'search' parameter is absolutely identical to the text in the file, including all whitespace! "
                           "Use the run_build_command tool to compile the project and check if errors occur. "
                           "Use ninja as the project type and not Makefile.";

    QString fullPath = _editor->projectRoot() + "/agents.md";
    QFile file(fullPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        manifest += "\n" + in.readAll().toStdString();
    }
    systemMsg["content"] = manifest;
    chatHistory.push_back(systemMsg);

    streamBuffer.clear();
    currentAssistantMessage.clear();
    currentToolCalls = json::array();

    if (chatDisplay) {
        chatDisplay->clear();
        chatDisplay->append("<i>[System: New session started. Ready.]</i><br>");
    }
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
                        m.interface       = "ollama";

                        QAction* action = modelMenu->addAction(m.name);
                        connect(action, &QAction::triggered, [this, m] { setCurrentModel(m.name); });
                        _models.push_back(m);
                        }
                  // at this point we have a complete list of available LL models
                  // and we can select the last used model as saved in settings
                  setCurrentModel(_editor->settingsLLModel());
                  }
            catch (...) {
                  Critical("fail");
                  }
            });
      }

//---------------------------------------------------------
//   sendMessage
//---------------------------------------------------------

void Agent::sendMessage() {
      // Rate Limit Check vor dem Senden ---
      if (rateLimitResetTime.isValid() && QDateTime::currentDateTime() < rateLimitResetTime) {
            qint64 secondsLeft = QDateTime::currentDateTime().secsTo(rateLimitResetTime);
            chatDisplay->append(
                QString("<br><font color='orange'><b>[Rate Limit]:</b> Please wait %1 more seconds...</font><br>").arg(secondsLeft));
            return;
            }
      QString text = userInput->toPlainText().trimmed();
      if (text.isEmpty() || model.name == "")
            return;

      userInput->clear();
      setInputEnabled(false);
      retryPause = 2000;

      json userMsg;
      userMsg["role"]    = "user";
      userMsg["content"] = text.toStdString();
      chatHistory.push_back(userMsg);

      updateChatDisplay();
      chatDisplay->append(QString("<b>%1:</b> ").arg(model.name.isEmpty() ? "AI" : model.name));

      sendChatRequest();
      }

void Agent::trimHistory() {
      const int MAX_MSG = 20; // Keep only the last 20 messages
      if (chatHistory.size() <= MAX_MSG)
            return;

      // We need a new container
      json newHistory = json::array();

      // 1. Always keep system prompt!
      if (!chatHistory.empty())
            newHistory.push_back(chatHistory[0]);

      // 2. Calculate where to start copying from (the last N)
      unsigned startIdx = chatHistory.size() - (MAX_MSG - 1);

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
//   sendChatRequest
//---------------------------------------------------------

void Agent::sendChatRequest() {
      QByteArray payload;
      QUrl url;
      QNetworkRequest request;

      trimHistory();
      json requestJson;

      if (model.interface == "gemini") {
            url = QUrl("https://generativelanguage.googleapis.com/v1beta/openai/chat/completions");
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            request.setRawHeader("Authorization", ("Bearer " + model.apiKey).toUtf8());

            requestJson["model"]  = model.modelIdentifier.toStdString();
            requestJson["stream"] = true;
            requestJson["tools"]  = toolsDefinition;

            json adaptedHistory = json::array();
            for (auto msg : chatHistory) {
                  if (msg.contains("tool_calls")) {
                        for (auto& tc : msg["tool_calls"]) {
                              if (tc.contains("function") && tc["function"].contains("arguments") &&
                                  tc["function"]["arguments"].is_object()) {
                                    tc["function"]["arguments"] = tc["function"]["arguments"].dump();
                                    }
                              }
                        }
                  adaptedHistory.push_back(msg);
                  }
            requestJson["messages"] = adaptedHistory;
            payload                 = QString::fromStdString(requestJson.dump()).toUtf8();
            }
      else if (model.interface == "anthropic") {
            url = QUrl("https://api.anthropic.com/v1/messages");
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            request.setRawHeader("x-api-key", model.apiKey.toUtf8());
            request.setRawHeader("anthropic-version", "2023-06-01");

            json anthropicRequest;
            anthropicRequest["model"]      = model.modelIdentifier.toStdString();
            anthropicRequest["max_tokens"] = 4096;
            anthropicRequest["stream"]     = true;

            // --- CORRECTION: Transform tool definitions for Anthropic ---
            json anthropicTools = json::array();
            for (const auto& t : toolsDefinition) {
                  if (t.contains("function")) {
                        json tool = t["function"];
                        // Anthropic uses 'input_schema' instead of 'parameters'
                        if (tool.contains("parameters")) {
                              tool["input_schema"] = tool["parameters"];
                              tool.erase("parameters");
                              }
                        anthropicTools.push_back(tool);
                        }
                  }
            anthropicRequest["tools"] = anthropicTools;

            json anthropicMessages = json::array();
            std::string system_prompt;

            for (const auto& msg : chatHistory) {
                  std::string role = msg.value("role", "");
                  if (role == "system") {
                        system_prompt = msg.value("content", "");
                        }
                  else if (role == "tool") {
                        json toolMsg;
                        toolMsg["role"]           = "user";
                        json contentArray         = json::array();
                        json toolResult           = json::object();
                        toolResult["type"]        = "tool_result";
                        toolResult["tool_use_id"] = msg.value("tool_call_id", "");
                        toolResult["content"]     = msg.value("content", "");
                        contentArray.push_back(toolResult);
                        toolMsg["content"] = contentArray;
                        anthropicMessages.push_back(toolMsg);
                        }
                  else if (role == "assistant") {
                        // Anthropic expects tool_calls as a content array with type="tool_use",
                        // not as a separate "tool_calls" key (OpenAI/Ollama format).
                        // Conversion is necessary so that subsequent requests after tool calls do not
                        // fail with HTTP 400.
                        if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
                              json contentArray = json::array();
                              if (msg.contains("content") && msg["content"].is_string() && !msg["content"].get<std::string>().empty()) {
                                    json textBlock;
                                    textBlock["type"] = "text";
                                    textBlock["text"] = msg["content"];
                                    contentArray.push_back(textBlock);
                                    }
                              for (const auto& tc : msg["tool_calls"]) {
                                    json toolUse;
                                    toolUse["type"]  = "tool_use";
                                    toolUse["id"]    = tc.value("id", "");
                                    toolUse["name"]  = tc.contains("function") ? tc["function"].value("name", "") : "";
                                    toolUse["input"] = tc.contains("function") && tc["function"].contains("arguments")
                                                           ? tc["function"]["arguments"]
                                                           : json::object();
                                    contentArray.push_back(toolUse);
                                    }
                              json converted;
                              converted["role"]    = "assistant";
                              converted["content"] = contentArray;
                              anthropicMessages.push_back(converted);
                              }
                        else {
                              // Normal assistant response without tool calls: adopt directly
                              anthropicMessages.push_back(msg);
                              }
                        }
                  else {
                        anthropicMessages.push_back(msg);
                        }
                  }

            if (!system_prompt.empty())
                  anthropicRequest["system"] = system_prompt;
            anthropicRequest["messages"] = anthropicMessages;
            payload                      = QString::fromStdString(anthropicRequest.dump()).toUtf8();
            }
      else { // Ollama as standard
            url = QUrl(model.baseUrl + "/api/chat");
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

            requestJson["model"]    = model.modelIdentifier.toStdString();
            requestJson["stream"]   = true;
            requestJson["tools"]    = toolsDefinition;
            requestJson["messages"] = chatHistory;
            payload                 = QString::fromStdString(requestJson.dump()).toUtf8();
            }

      //      Debug("send {}: <{}>", url.url(), json::parse(payload.toStdString()).dump(2));
      Debug("send {}", url.url());
      request.setUrl(url);
      request.setTransferTimeout(60000 * 5);

      streamBuffer.clear();
      currentAssistantMessage.clear();
      currentToolCalls = json::array();
      openAIToolCallAssembler.clear();
      anthropicToolCallAssembler.clear();

      currentReply = networkManager->post(request, payload);
      connect(currentReply, &QNetworkReply::readyRead, this, &Agent::handleChatReadyRead);
      connect(currentReply, &QNetworkReply::finished, this, &Agent::handleChatFinished);
      }

//---------------------------------------------------------
//   handleChatReadyRead
//---------------------------------------------------------

void Agent::handleChatReadyRead() {
      if (!currentReply)
            return;

      // Header analysis for Anthropic Rate Limits ---
      if (model.interface == "anthropic") {
            // Check when the limit is reset
            QByteArray resetHeader = currentReply->rawHeader("anthropic-ratelimit-reset");
            if (!resetHeader.isEmpty()) {
                  // Anthropic sends ISO 8601 date
                  QDateTime resetTime = QDateTime::fromString(QString::fromUtf8(resetHeader), Qt::ISODate);
                  if (resetTime.isValid() && resetTime > QDateTime::currentDateTime()) {
                        // We remember globally when we are allowed again
                        rateLimitResetTime = resetTime;
                        }
                  }

            // Optional: Warning when tokens run low (throttle)
            QByteArray remainingHeader = currentReply->rawHeader("anthropic-ratelimit-tokens-remaining");
            if (!remainingHeader.isEmpty()) {
                  int remaining = remainingHeader.toInt();
                  if (remaining < 2000) // Buffer of 2000 tokens
                        Debug("Rate Limit Warning: Only {} tokens left.", remaining);
                  }
            }
      int statusCode = currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode >= 400) {
            // In case of an HTTP error, we just read everything at once,
            // as it is probably not a stream, but a single JSON error object.
            QByteArray errorData = currentReply->readAll();
            QString errorString  = QString::fromUtf8(errorData);

            if (statusCode == 503) {
                  QString msg =
                      QString("Server overloaded (503). Pause for %1 seconds and try again...").arg(retryPause / 1000.0);
                  chatDisplay->append(QString("<br><font color='orange'><b>[Info]:</b> %1</font><br>").arg(msg));
                  Warning("{}", msg.toStdString());
                  isRetrying = true;
                  }
            else {
                  chatDisplay->append(
                      QString("<br><font color='red'><b>[HTTP Error %1]:</b> %2</font><br>").arg(statusCode).arg(errorString));
                  Critical("HTTP Error {}: {}", statusCode, errorString.toStdString());
                  }

            // Abort connection, as no more useful data will come anyway
            currentReply->abort();
            return;
            }
      streamBuffer.append(currentReply->readAll());
      int newlineIndex;

      while ((newlineIndex = streamBuffer.indexOf('\n')) != -1) {
            QByteArray line = streamBuffer.left(newlineIndex).trimmed();
            streamBuffer.remove(0, newlineIndex + 1);
            if (line.isEmpty())
                  continue;

            // Filter SSE Prefix (Server-Sent Events) from Gemini/OpenAI
            if (line.startsWith("data: ")) {
                  line = line.mid(6);
                  if (line == "[DONE]")
                        continue; // End of stream
                  }
            // SSE Prefix for Anthropic-Events
            if (line.startsWith("event: ")) {
                  currentAnthropicEventType = line.mid(7);
                  continue; // Next line (data:) contains the content
                  }
            try {
                  auto j = json::parse(line.toStdString());
                  // --- 0. Error handling (Gemini/OpenAI & Ollama) ---
                  if (j.contains("error")) {
                        std::string errorMessage = "Unknown API Error";

                        // Gemini/OpenAI often packs details into an object
                        if (j["error"].is_object() && j["error"].contains("message"))
                              errorMessage = j["error"]["message"].get<std::string>();
                        // Fallback, if the error comes as a simple string
                        else if (j["error"].is_string())
                              errorMessage = j["error"].get<std::string>();

                        // Show error red in chat
                        QTextCursor cursor = chatDisplay->textCursor();
                        cursor.movePosition(QTextCursor::End);
                        chatDisplay->setTextCursor(cursor);
                        chatDisplay->insertHtml(
                            QString("<br><font color='red'><b>[API Error]:</b> %1</font><br>").arg(QString::fromStdString(errorMessage)));
                        chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
                        Critical("API Error received: {}", errorMessage);
                        // continue; // Continue with the next line
                        }
                  // --- 1. Ollama Format ---
                  else if (j.contains("message")) {
                        auto msg = j["message"];
                        if (msg.contains("content") && msg["content"].is_string()) {
                              std::string chunk        = msg["content"].get<std::string>();
                              currentAssistantMessage += chunk;

                              QTextCursor cursor = chatDisplay->textCursor();
                              cursor.movePosition(QTextCursor::End);
                              chatDisplay->setTextCursor(cursor);
                              chatDisplay->insertPlainText(QString::fromStdString(chunk));
                              chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
                              chatDisplay->horizontalScrollBar()->setValue(chatDisplay->horizontalScrollBar()->minimum());
                              }
                        if (msg.contains("tool_calls"))
                              for (const auto& tc : msg["tool_calls"])
                                    currentToolCalls.push_back(tc);
                        }
                  // --- 2. Gemini / OpenAI Format ---
                  else if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
                        auto choice = j["choices"][0];
                        if (choice.contains("delta")) {
                              auto delta = choice["delta"];

                              if (delta.contains("content") && delta["content"].is_string()) {
                                    std::string chunk        = delta["content"].get<std::string>();
                                    currentAssistantMessage += chunk;

                                    QTextCursor cursor = chatDisplay->textCursor();
                                    cursor.movePosition(QTextCursor::End);
                                    chatDisplay->setTextCursor(cursor);
                                    chatDisplay->insertPlainText(QString::fromStdString(chunk));
                                    chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
                                    chatDisplay->horizontalScrollBar()->setValue(chatDisplay->horizontalScrollBar()->minimum());
                                    }

                              // Assemble tool calls in stream (chunking)
                              if (delta.contains("tool_calls")) {
                                    for (const auto& tc : delta["tool_calls"]) {
                                          int index = 0;

                                          // --- 1. Robust index detection ---
                                          if (tc.contains("index")) {
                                                index = tc["index"].get<int>();
                                                }
                                          else {
                                                // Fallback: If index is missing, check content of chunk
                                                if (openAIToolCallAssembler.empty()) {
                                                      index = 0;
                                                      }
                                                else {
                                                      // If an ID or name is sent, a NEW tool starts.
                                                      // If only arguments come, it is the continuation of the OLD tool.
                                                      if (tc.contains("id") || (tc.contains("function") && tc["function"].contains("name")))
                                                            index = openAIToolCallAssembler.rbegin()->first + 1;
                                                      else
                                                            index = openAIToolCallAssembler.rbegin()->first;
                                                      }
                                                }

                                          // --- 2. Initialize assembler ---
                                          if (openAIToolCallAssembler.find(index) == openAIToolCallAssembler.end()) {
                                                openAIToolCallAssembler[index] = {
                                                         {    "type",                        "function"},
                                                         {"function", {{"name", ""}, {"arguments", ""}}}
                                                      };
                                                }

                                          // --- 3. Assign data ---
                                          if (tc.contains("id"))
                                                openAIToolCallAssembler[index]["id"] = tc["id"];

                                          // NEW: thought_signature for Gemini must be preserved!
                                          if (tc.contains("extra_content"))
                                                openAIToolCallAssembler[index]["extra_content"] = tc["extra_content"];

                                          if (tc.contains("function")) {
                                                auto func = tc["function"];
                                                if (func.contains("name"))
                                                      openAIToolCallAssembler[index]["function"]["name"] =
                                                          openAIToolCallAssembler[index]["function"]["name"].get<std::string>() +
                                                          func["name"].get<std::string>();
                                                if (func.contains("arguments"))
                                                      openAIToolCallAssembler[index]["function"]["arguments"] =
                                                          openAIToolCallAssembler[index]["function"]["arguments"].get<std::string>() +
                                                          func["arguments"].get<std::string>();
                                                }
                                          }
                                    }
                              }
                        }
                  // --- 3. Anthropic Format ---

                  else if (currentAnthropicEventType == "content_block_delta") {
                        if (j.contains("delta")) {
                              std::string type = j["delta"].value("type", "");

                              // Case A: Text stream (assistant's response)
                              if (type == "text_delta" && j["delta"].contains("text")) {
                                    std::string chunk        = j["delta"]["text"].get<std::string>();
                                    currentAssistantMessage += chunk;
                                    QTextCursor cursor       = chatDisplay->textCursor();
                                    cursor.movePosition(QTextCursor::End);
                                    chatDisplay->setTextCursor(cursor);
                                    chatDisplay->insertPlainText(QString::fromStdString(chunk));
                                    chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
                                    chatDisplay->horizontalScrollBar()->setValue(chatDisplay->horizontalScrollBar()->minimum());
                                    }
                              // Case B: Tool stream (build JSON arguments)
                              else if (type == "input_json_delta" && j["delta"].contains("partial_json")) {
                                    int index                = j["index"].get<int>();
                                    std::string partialInput = j["delta"]["partial_json"].get<std::string>();

                                    // Make sure the entry exists (content_block_start
                                    // might not have arrived in the stream yet)
                                    if (!anthropicToolCallAssembler.count(index)) {
                                          Debug("input_json_delta: index {} not yet in assembler, initialize empty", index);
                                          anthropicToolCallAssembler[index] = json::object();
                                          }
                                    if (!anthropicToolCallAssembler[index].contains("input"))
                                          anthropicToolCallAssembler[index]["input"] = "";

                                    // Append JSON fragment
                                    anthropicToolCallAssembler[index]["input"] =
                                        anthropicToolCallAssembler[index]["input"].get<std::string>() + partialInput;
                                    }
                              }
                        }
                  else if (currentAnthropicEventType == "content_block_start") {
                        // ... (This block was correct and can remain as is) ...
                        if (j.contains("content_block") && j["content_block"]["type"] == "tool_use") {
                              int index                         = j["index"].get<int>();
                              anthropicToolCallAssembler[index] = j["content_block"];
                              // Initialize input string empty for safety
                              anthropicToolCallAssembler[index]["input"] = "";
                              }
                        }
                  }
            catch (const json::parse_error& e) {
                  // Normal for streaming that broken fragments appear from time to time.
                  // Helpful to see in debug mode, otherwise just ignore.
                  Debug("Parse Warning: {} | Raw Line: {}", e.what(), line.toStdString());
                  }
            catch (const std::exception& e) {
                  Critical("Unexpected error in chat stream: {}", e.what());
                  }
            }
      }

//---------------------------------------------------------
//   handleChatFinished
//---------------------------------------------------------

void Agent::handleChatFinished() {
      if (!currentReply && currentToolCalls.empty() && openAIToolCallAssembler.empty() && anthropicToolCallAssembler.empty()) {
            Debug("no currentReply");
            return;
            }

      // --- ERROR HANDLING & BACKOFF LOGIC ---
      if (currentReply && currentReply->error() != QNetworkReply::NoError) {
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

                              // Fallback: If we have already set Anthropic Reset Time in readyRead
                              else if (rateLimitResetTime.isValid()) {
                                    waitMs = QDateTime::currentDateTime().msecsTo(rateLimitResetTime);
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
                        QTimer::singleShot(waitMs, this, &Agent::sendChatRequest);
                        return;
                        }
                  else {
                        chatDisplay->append(
                            QString("<br><font color='red'><b>[Abort]:</b> Too many attempts (%1).</font><br>").arg(maxRetries));
                        }
                  }

            QString errorMessage;

            // Generate specific message for timeout case
            if (currentReply->error() == QNetworkReply::TimeoutError) {
                  errorMessage = "Timeout: The LL server did not respond within 60 seconds. Maybe the "
                                 "model is still loading or the server is hanging.";
                  }
            else {
                  errorMessage = currentReply->errorString();
                  }

            Debug("Network/API error: {}", errorMessage.toStdString());

            // Show the error to the user in the UI
            chatDisplay->append(QString("<br><font color='red'><b>[Connection abort]:</b> %1</font><br>").arg(errorMessage));

            currentReply->deleteLater();
            currentReply      = nullptr;
            currentRetryCount = 0;

            // Important: Release UI again!
            setInputEnabled(true);
            userInput->setFocus();
            return;
            }
      // 1. Finalize Gemini tool calls from stream
      for (auto& [index, tc] : openAIToolCallAssembler) {
            try {
                  // Parse arguments from string back into object
                  std::string argsStr         = tc["function"]["arguments"].get<std::string>();
                  tc["function"]["arguments"] = argsStr.empty() ? json::object() : json::parse(argsStr);

                  // Generate fallback ID if API did not provide one
                  if (!tc.contains("id"))
                        tc["id"] = "call_" + std::to_string(rand());
                  currentToolCalls.push_back(tc);
                  }
            catch (const json::parse_error& e) {
                  Debug("LLM generated defective JSON in tool arguments: {}", e.what());

                  // Report error to the model so it can correct itself!
                  tc["function"]["arguments"] = {
                           {"error_info", "JSON Parse Error: " + std::string(e.what())}
                        };
                  if (!tc.contains("id"))
                        tc["id"] = "call_" + std::to_string(rand());
                  currentToolCalls.push_back(tc);
                  }
            }
      openAIToolCallAssembler.clear();

      // 2. Finalize Anthropic tool calls
      for (auto const& [index, tc] : anthropicToolCallAssembler) {
            try {
                  json finalToolCall;
                  finalToolCall["type"] = "function";
                  finalToolCall["id"]   = tc.value("id", ""); // ID directly from tc

                  json function;
                  // CORRECTION: Anthropic places 'name' directly in 'tc', not in 'function'
                  function["name"] = tc.value("name", "unknown_tool");

                  // The 'input' field at Anthropic contains the arguments
                  if (tc.contains("input")) {
                        if (tc["input"].is_string()) {
                              std::string inputStr = tc["input"].get<std::string>();
                              // FIX: Catch empty strings, otherwise json::parse throws an exception

                              if (QString::fromStdString(inputStr).trimmed().isEmpty())
                                    function["arguments"] = json::object();
                              else {
                                    try {
                                          function["arguments"] = json::parse(inputStr);
                                          }
                                    catch (const json::parse_error&) {
                                          function["arguments"] = json::object();
                                          }
                                    }
                              }
                        else {
                              // tc["input"] is already a JSON object -> adopt directly
                              function["arguments"] = tc["input"];
                              }

                        finalToolCall["function"] = function;
                        currentToolCalls.push_back(finalToolCall);
                        }
                  }
            catch (const json::parse_error& e) {
                  Debug("Anthropic JSON Parse Error: {}", e.what());
                  // ... error handling as before
                  }
            }
      anthropicToolCallAssembler.clear();

      if (currentReply) {
            currentReply->deleteLater();
            currentReply = nullptr;
            }
      currentRetryCount = 0;
      isRetrying        = false;

      // Add assistant response to history
      json assistantMsg;
      assistantMsg["role"] = "assistant";

      if (!currentToolCalls.empty()) {
            if (model.interface == "anthropic") {
                  // Anthropic expects a different structure for tool responses in history
                  json contentArray = json::array();
                  if (!currentAssistantMessage.empty()) {
                        json textBlock;
                        textBlock["type"] = "text";
                        textBlock["text"] = currentAssistantMessage;
                        contentArray.push_back(textBlock);
                        }
                  for (const auto& tc : currentToolCalls) {
                        json toolUse;
                        toolUse["type"]  = "tool_use";
                        toolUse["id"]    = tc["id"];
                        toolUse["name"]  = tc["function"]["name"];
                        toolUse["input"] = tc["function"]["arguments"];
                        contentArray.push_back(toolUse);
                        }
                  assistantMsg["content"] = contentArray;
                  }
            else {
                  assistantMsg["content"]    = currentAssistantMessage.empty() ? "" : currentAssistantMessage;
                  assistantMsg["tool_calls"] = currentToolCalls;
                  }
            }
      else {
            assistantMsg["content"] = currentAssistantMessage;
            }

      chatHistory.push_back(assistantMsg);

      updateChatDisplay(); // RENDER IMMEDIATELY

      // --- Agent action: Execute tools ---
      if (!currentToolCalls.empty()) {

            // Container for Anthropic: All results must be collected in ONE message
            json anthropicContentArray = json::array();

            for (const auto& toolCall : currentToolCalls) {
                  std::string funcName = toolCall["function"]["name"].get<std::string>();
                  json args            = toolCall["function"]["arguments"];

                  QString result = executeTool(funcName, args);

                  if (model.interface == "anthropic") {
                        // ANTHROPIC: Collect result, but do not send yet
                        json toolResult;
                        toolResult["type"]        = "tool_result";
                        toolResult["tool_use_id"] = toolCall.value("id", "");
                        toolResult["content"]     = result.toStdString();

                        // Error indication for the model, if the tool failed (optional)
                        if (result.startsWith("Error:") || result.startsWith("Error:"))
                              toolResult["is_error"] = true;

                        anthropicContentArray.push_back(toolResult);
                        }
                  else {
                        // OPENAI / OLLAMA: Every tool is its own message
                        json toolResponseMsg;
                        toolResponseMsg["role"] = "tool";
                        toolResponseMsg["name"] = funcName;
                        if (toolCall.contains("id"))
                              toolResponseMsg["tool_call_id"] = toolCall["id"];
                        toolResponseMsg["content"] = result.toStdString();
                        chatHistory.push_back(toolResponseMsg);
                        }
                  }

            // AFTER the loop: Push the collected Anthropic results as ONE message
            if (model.interface == "anthropic") {
                  json toolResponseMsg;
                  toolResponseMsg["role"]    = "user";
                  toolResponseMsg["content"] = anthropicContentArray;
                  chatHistory.push_back(toolResponseMsg);
                  }

            updateChatDisplay(); // Update UI

            // Send result back to LLM
            retryPause = 2000;
            sendChatRequest();
            return;
            }
      setInputEnabled(true);
      userInput->setFocus();
      retryPause = 2000;
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
            obj["name"]      = m.name;
            obj["url"]       = m.baseUrl;
            obj["key"]       = m.apiKey;
            obj["modelId"]   = m.modelIdentifier;
            obj["interface"] = m.interface;
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
            m.interface       = obj["interface"].toString();
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
//   startNewSession
//---------------------------------------------------------

void Agent::startNewSession() {
      // 1. Save old state
      saveStatus();

      // 2. Check and commit to Git
//      QString commitMsg = "Auto-commit: End of session " + QFileInfo(currentSessionFileName).fileName();
//      commitGitChanges(commitMsg);

      // 3. Reset UI and history
      chatHistory = json::array();
      chatDisplay->clear();
      streamBuffer.clear();
      currentAssistantMessage.clear();
      currentToolCalls = json::array();

      // 4. Generate new file name
      currentSessionFileName = generateSessionFileName();

      // 5. Re-initialize system prompt (copy from connectToServer)
      json systemMsg;
      systemMsg["role"]    = "system";
      std::string manifest = "You are an experienced C++ developer... (here your manifest)";

      QString fullPath = _editor->projectRoot() + "/agents.md";
      QFile file(fullPath);
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            manifest += "\n" + in.readAll().toStdString();
            }
      systemMsg["content"] = manifest;
      chatHistory.push_back(systemMsg);

      // 6. Confirmation in UI
      chatDisplay->append(
          QString("<i>[System: New session started: <b>%1</b>]</i><br>").arg(QFileInfo(currentSessionFileName).fileName()));

      // Set focus back to input field
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
//   generateSessionFileName
//---------------------------------------------------------

QString Agent::generateSessionFileName() {
      if (!_editor)
            return "";

      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QDate date       = QDate::currentDate();
      QString dateStr  = date.toString("dd-MM-yyyy");

      QDir dir(projRoot);
      QStringList filters;
      // GEÄNDERT: Suche nach .json
      filters << QString("Session-%1-*.json").arg(dateStr);
      QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

      int maxNum = 0;
      for (const QFileInfo& fi : files) {
            QString name      = fi.baseName();
            QStringList parts = name.split('-');
            if (parts.size() >= 5) {
                  int num = parts.last().toInt();
                  if (num > maxNum)
                        maxNum = num;
                  }
            }

      QString newNum = QString::number(maxNum + 1).rightJustified(2, '0');
      return QString("%1/.nped/Session-%2-%3.json").arg(projRoot, dateStr, newNum);
      }

//---------------------------------------------------------
//   saveStatus
//---------------------------------------------------------

void Agent::saveStatus() {
      // chatHistory is considered empty if it contains only the system manifest
      if (currentSessionFileName.isEmpty() || chatHistory.size() <= 1)
            return;

      Debug("session <{}>", currentSessionFileName);
      QString path = QFileInfo(currentSessionFileName).absolutePath();
      QDir dir;
      dir.mkpath(path);
      Debug("mkpath <{}>", path);

      QFile file(currentSessionFileName);
      if (file.open(QIODevice::WriteOnly)) {
            // simply save the complete array as a formatted JSON file
            std::string dump = chatHistory.dump(4);
            file.write(dump.c_str(), dump.length());
            file.close();
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

      QString projRoot = QDir::cleanPath(_editor->projectRoot());
      QDir dir(projRoot + "/.nped");
      QStringList filters;
      filters << "Session-*.json"; // CHANGED: We only search for .json now

      QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);

      if (!files.isEmpty()) {
            currentSessionFileName = files.first().absoluteFilePath();

            QFile file(currentSessionFileName);
            if (file.open(QIODevice::ReadOnly)) {
                  try {
                        // 1. Load memory
                        chatHistory = json::parse(file.readAll().toStdString());

                        // 2. Generate visual representation
                        updateChatDisplay();

                        chatDisplay->append("<br><i>[System: Last session (" + files.first().fileName() + ") continued.]</i><br>");

                        // Scroll down
                        QTextCursor cursor = chatDisplay->textCursor();
                        cursor.movePosition(QTextCursor::End);
                        chatDisplay->setTextCursor(cursor);
                        }
                  catch (const std::exception& e) {
                        Critical("Error loading JSON history: {}", e.what());
                        chatHistory = json::array();
                        }
                  }
            }
      else {
            // No files found -> Start new session
            currentSessionFileName = generateSessionFileName();
            chatDisplay->append("<i>[System: No previous session found. New session started.]</i><br>");

            chatHistory = json::array();
            json systemMsg;
            systemMsg["role"] = "system";

            std::string manifest = "You are an experienced C++ developer. Your task is to analyze and write code in the project and to fix build errors.";
            QString fullPath     = projRoot + "/agents.md";
            QFile manifestFile(fullPath);
            if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                  QTextStream in(&manifestFile);
                  manifest += "\n" + in.readAll().toStdString();
                  }
            systemMsg["content"] = manifest;
            chatHistory.push_back(systemMsg);
            }
      }
