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
//   Agent  (Konstruktor)
//---------------------------------------------------------

Agent::Agent(Editor* e, QWidget* parent) : QWidget(parent), _editor(e), currentReply(nullptr) {
      networkManager  = new QNetworkAccessManager(this);
      chatHistory     = json::array();
      toolsDefinition = getToolsDefinition();

      QVBoxLayout* mainLayout = new QVBoxLayout(this);

      // --- 1. Tool-Leiste & Modell-Auswahl ---
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
                  chatDisplay->append(QString("<br><i>[System: Modus gewechselt zu <b>%1</b>]</i>")
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

      // Initiale Farbe setzen (Plan-Modus)
      if (auto* button = qobject_cast<QToolButton*>(toolBar->widgetForAction(modeToggleAction))) {
            button->setStyleSheet("QToolButton { background-color: #d0f0d0; border: 1px solid #8c8; color: #050; border-radius: 3px; "
                                  "padding: 4px 8px; margin: 0px 4px; }"
                                  "QToolButton:hover { background-color: #c0e0c0; }"
                                  "QToolButton:pressed { background-color: #b0d0b0; }");
            }
      mainLayout->addWidget(toolBar);

      // --- 2. Chat-Anzeige ---
      chatDisplay = new QTextEdit(this);
      chatDisplay->setReadOnly(true);
      mainLayout->addWidget(chatDisplay);

      // --- 3. Eingabefeld ---
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
//    Hilfsfunktion für den Pfad
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

      std::string manifest = "Du bist ein erfahrener C++ Entwickler. "
                             "Deine Aufgabe ist es, Code im Projekt zu analysieren, zu schreiben und Build-Fehler zu beheben.\n\n"
                             "FEHLERANALYSE-REGELN:\n"
                             "1. Wenn ein Build fehlschlägt, analysiere den Output von 'run_build'.\n"
                             "2. Suche nach Zeilen wie 'file.cpp:42:10: error: ...'.\n"
                             "3. Nutze 'read_file', um den Kontext um die fehlerhafte Zeile zu lesen.\n"
                             "4. Nutze 'modify_file', um den Fehler gezielt zu korrigieren.\n\n"

                             "TOOL-FORMAT:\n"
                             "Antworte ausschließlich im JSON-Format, wenn du ein Tool aufrufst:\n"
                             "{\n"
                             "  \"tool\": \"modify_file\",\n"
                             "  \"args\": {\n"
                             "    \"path\": \"src/main.cpp\",\n"
                             "    \"find\": \"alter code\",\n"
                             "    \"replace\": \"neuer code\"\n"
                             "  }\n"
                             "}\n\n"

                             "PROJEKT-STRUKTUR:\n"
                             "Standard-Qt6-Layout. Build-Verzeichnis ist './build'. Nutze CMake.\n"

                             "Denke dir keine eigenen Aufgaben aus und handle niemals eigenmächtig! "
                             "Nutze für Änderungen an Dateien bevorzugt das Tool replace_in_file. Achte darauf, dass der "
                             "'search'-Parameter absolut identisch mit dem Text in der Datei ist, inklusive aller Leerzeichen! "
                             "Nutze das Tool run_build_command um das Projekts zu kompilieren und überprüfe, ob Fehler auftreten. "
                             "Verwende ninja als projekt typ und nicht Makefile ";

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
            chatDisplay->append("<i>[System: Neue Session gestartet. Bereit.]</i><br>");
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
                QString("<br><font color='orange'><b>[Rate Limit]:</b> Bitte warte noch %1 Sekunden...</font><br>").arg(secondsLeft));
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
      const int MAX_MSG = 20; // Behalte nur die letzten 20 Nachrichten
      if (chatHistory.size() <= MAX_MSG)
            return;

      // Wir brauchen einen neuen Container
      json newHistory = json::array();

      // 1. System Prompt immer behalten!
      if (!chatHistory.empty())
            newHistory.push_back(chatHistory[0]);

      // 2. Berechne, ab wo wir kopieren (die letzten N)
      unsigned startIdx = chatHistory.size() - (MAX_MSG - 1);

      // Sicherstellen, dass wir nicht mitten in einem Tool-Flow schneiden
      // Wenn die Nachricht am startIdx ein "tool_result" ist, brauchen wir zwingend
      // den "tool_use" davor!
      while (startIdx < chatHistory.size()) {
            const auto& msg = chatHistory[startIdx];

            // Prüfen, ob wir gerade ein verwaistes Tool-Result haben
            bool isToolResult = (msg.value("role", "") == "tool") ||
                                (msg.value("role", "") == "user" && msg.contains("content") && msg["content"].is_array());

            if (isToolResult) {
                  // Wir dürfen nicht hier schneiden, wir müssen eins früher anfangen
                  startIdx--;
                  }
            else {
                  break; // Alles okay, das ist eine "saubere" Nachricht (meist User-Text)
                  }

            if (startIdx <= 1)
                  break; // Nicht den System-Prompt überschreiben
            }

      // 3. Den Rest kopieren
      for (size_t i = startIdx; i < chatHistory.size(); ++i)
            newHistory.push_back(chatHistory[i]);

      chatHistory = newHistory;

      // Loggen zur Kontrolle
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

            // --- KORREKTUR: Tool-Definitionen für Anthropic transformieren ---
            json anthropicTools = json::array();
            for (const auto& t : toolsDefinition) {
                  if (t.contains("function")) {
                        json tool = t["function"];
                        // Anthropic nutzt 'input_schema' statt 'parameters'
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
                        // Anthropic erwartet tool_calls als content-Array mit type="tool_use",
                        // nicht als separaten "tool_calls"-Key (OpenAI/Ollama-Format).
                        // Konvertierung ist nötig, damit Folge-Requests nach Tool-Calls nicht
                        // mit HTTP 400 scheitern.
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
                              // Normale Assistenten-Antwort ohne Tool-Calls: direkt übernehmen
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
      else { // Ollama als Standard
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

      // Header-Analyse für Anthropic Rate Limits ---
      if (model.interface == "anthropic") {
            // Prüfen, wann das Limit zurückgesetzt wird
            QByteArray resetHeader = currentReply->rawHeader("anthropic-ratelimit-reset");
            if (!resetHeader.isEmpty()) {
                  // Anthropic sendet ISO 8601 Datum
                  QDateTime resetTime = QDateTime::fromString(QString::fromUtf8(resetHeader), Qt::ISODate);
                  if (resetTime.isValid() && resetTime > QDateTime::currentDateTime()) {
                        // Wir merken uns global, wann wir wieder dürfen
                        rateLimitResetTime = resetTime;
                        }
                  }

            // Optional: Warnung wenn Tokens knapp werden (drosseln)
            QByteArray remainingHeader = currentReply->rawHeader("anthropic-ratelimit-tokens-remaining");
            if (!remainingHeader.isEmpty()) {
                  int remaining = remainingHeader.toInt();
                  if (remaining < 2000) // Puffer von 2000 Tokens
                        Debug("Rate Limit Warnung: Nur noch {} Tokens übrig.", remaining);
                  }
            }
      int statusCode = currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode >= 400) {
            // Bei einem HTTP-Fehler lesen wir einfach alles am Stück aus,
            // da es wahrscheinlich kein Stream, sondern ein einzelnes JSON-Fehlerobjekt ist.
            QByteArray errorData = currentReply->readAll();
            QString errorString  = QString::fromUtf8(errorData);

            if (statusCode == 503) {
                  QString msg =
                      QString("Server überlastet (503). Mache Pause von %1 Sekunden und versuche es erneut...").arg(retryPause / 1000.0);
                  chatDisplay->append(QString("<br><font color='orange'><b>[Info]:</b> %1</font><br>").arg(msg));
                  Warning("{}", msg.toStdString());
                  isRetrying = true;
                  }
            else {
                  chatDisplay->append(
                      QString("<br><font color='red'><b>[HTTP Fehler %1]:</b> %2</font><br>").arg(statusCode).arg(errorString));
                  Critical("HTTP Error {}: {}", statusCode, errorString.toStdString());
                  }

            // Verbindung abbrechen, da eh keine nützlichen Daten mehr kommen
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

            // SSE Prefix (Server-Sent Events) von Gemini/OpenAI filtern
            if (line.startsWith("data: ")) {
                  line = line.mid(6);
                  if (line == "[DONE]")
                        continue; // Ende des Streams
                  }
            // SSE Prefix für Anthropic-Events
            if (line.startsWith("event: ")) {
                  currentAnthropicEventType = line.mid(7);
                  continue; // Nächste Zeile (data:) enthält den Inhalt
                  }
            try {
                  auto j = json::parse(line.toStdString());
                  // --- 0. Fehlerbehandlung (Gemini/OpenAI & Ollama) ---
                  if (j.contains("error")) {
                        std::string errorMessage = "Unbekannter API Fehler";

                        // Gemini/OpenAI verpackt Details oft in ein Objekt
                        if (j["error"].is_object() && j["error"].contains("message"))
                              errorMessage = j["error"]["message"].get<std::string>();
                        // Fallback, falls der Fehler als simpler String kommt
                        else if (j["error"].is_string())
                              errorMessage = j["error"].get<std::string>();

                        // Fehler rot im Chat anzeigen
                        QTextCursor cursor = chatDisplay->textCursor();
                        cursor.movePosition(QTextCursor::End);
                        chatDisplay->setTextCursor(cursor);
                        chatDisplay->insertHtml(
                            QString("<br><font color='red'><b>[API Fehler]:</b> %1</font><br>").arg(QString::fromStdString(errorMessage)));
                        chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
                        Critical("API Fehler empfangen: {}", errorMessage);
                        // continue; // Mit der nächsten Zeile weitermachen
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

                              // Tool-Calls im Stream zusammensetzen (Chunking)
                              if (delta.contains("tool_calls")) {
                                    for (const auto& tc : delta["tool_calls"]) {
                                          int index = 0;

                                          // --- 1. Robuste Index-Ermittlung ---
                                          if (tc.contains("index")) {
                                                index = tc["index"].get<int>();
                                                }
                                          else {
                                                // Fallback: Fehlt der Index, prüfen wir den Inhalt des Chunks
                                                if (openAIToolCallAssembler.empty()) {
                                                      index = 0;
                                                      }
                                                else {
                                                      // Wenn eine ID oder ein Name mitgeschickt wird, startet ein NEUES Tool.
                                                      // Wenn nur Argumente kommen, ist es die Fortsetzung des ALTEN Tools.
                                                      if (tc.contains("id") || (tc.contains("function") && tc["function"].contains("name")))
                                                            index = openAIToolCallAssembler.rbegin()->first + 1;
                                                      else
                                                            index = openAIToolCallAssembler.rbegin()->first;
                                                      }
                                                }

                                          // --- 2. Assembler initialisieren ---
                                          if (openAIToolCallAssembler.find(index) == openAIToolCallAssembler.end()) {
                                                openAIToolCallAssembler[index] = {
                                                         {    "type",                        "function"},
                                                         {"function", {{"name", ""}, {"arguments", ""}}}
                                                      };
                                                }

                                          // --- 3. Daten zuweisen ---
                                          if (tc.contains("id"))
                                                openAIToolCallAssembler[index]["id"] = tc["id"];

                                          // NEU: thought_signature für Gemini zwingend erhalten!
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

                              // Fall A: Text-Stream (Antwort des Assistenten)
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
                              // Fall B: Tool-Stream (JSON Argumente aufbauen)
                              else if (type == "input_json_delta" && j["delta"].contains("partial_json")) {
                                    int index                = j["index"].get<int>();
                                    std::string partialInput = j["delta"]["partial_json"].get<std::string>();

                                    // Sicherstellen, dass der Eintrag existiert (content_block_start
                                    // könnte im Stream noch nicht angekommen sein)
                                    if (!anthropicToolCallAssembler.count(index)) {
                                          Debug("input_json_delta: index {} noch nicht im Assembler, initialisiere leer", index);
                                          anthropicToolCallAssembler[index] = json::object();
                                          }
                                    if (!anthropicToolCallAssembler[index].contains("input"))
                                          anthropicToolCallAssembler[index]["input"] = "";

                                    // JSON-Fragment anhängen
                                    anthropicToolCallAssembler[index]["input"] =
                                        anthropicToolCallAssembler[index]["input"].get<std::string>() + partialInput;
                                    }
                              }
                        }
                  else if (currentAnthropicEventType == "content_block_start") {
                        // ... (Dieser Block war korrekt und kann so bleiben) ...
                        if (j.contains("content_block") && j["content_block"]["type"] == "tool_use") {
                              int index                         = j["index"].get<int>();
                              anthropicToolCallAssembler[index] = j["content_block"];
                              // Initialisiere input string sicherheitshalber leer
                              anthropicToolCallAssembler[index]["input"] = "";
                              }
                        }
                  }
            catch (const json::parse_error& e) {
                  // Bei Streaming normal, dass ab und zu kaputte Fragmente auftauchen.
                  // Im Debug-Modus hilfreich zu sehen, sonst einfach ignorieren.
                  Debug("Parse Warning: {} | Raw Line: {}", e.what(), line.toStdString());
                  }
            catch (const std::exception& e) {
                  Critical("Unerwarteter Fehler im Chat-Stream: {}", e.what());
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

      // --- FEHLERBEHANDLUNG & BACKOFF LOGIK ---
      if (currentReply && currentReply->error() != QNetworkReply::NoError) {
            int statusCode = currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // Fall 1: Rate Limit (429) oder Server Overload (503/500/502/504)
            if (statusCode == 429 || (statusCode >= 500 && statusCode < 600)) {

                  if (currentRetryCount < maxRetries) {
                        int waitMs = 0;
                        // A: Rate Limit (429) -> Headers respektieren
                        if (statusCode == 429) {
                              // KORREKTUR: "Retry-After" als Raw Header lesen, da es kein Enum dafür gibt
                              QByteArray retryAfterRaw = currentReply->rawHeader("Retry-After");

                              if (!retryAfterRaw.isEmpty()) {
                                    // Anthropic/OpenAI senden hier meist Sekunden als Integer
                                    waitMs = retryAfterRaw.toInt() * 1000;
                                    }

                              // Fallback: Wenn wir Anthropic Reset Time schon in readyRead gesetzt haben
                              else if (rateLimitResetTime.isValid()) {
                                    waitMs = QDateTime::currentDateTime().msecsTo(rateLimitResetTime);
                                    }

                              // Fallback: Standard Exponential wenn keine Header da sind
                              if (waitMs <= 0)
                                    waitMs = 1000 * std::pow(2, currentRetryCount);

                              // Sicherheitszuschlag (Jitter)
                              waitMs += (rand() % 500);

                              chatDisplay->append(
                                  QString("<br><font color='orange'><b>[Rate Limit]:</b> Pause für %1 Sekunden...</font><br>")
                                      .arg(waitMs / 1000.0));
                              }
                        // B: Server Fehler (5xx) -> Exponential Backoff
                        else {
                              waitMs = 2000 * std::pow(2, currentRetryCount);
                              chatDisplay->append(
                                  QString("<br><font color='orange'><b>[Server Fehler %1]:</b> Retry %2/%3 in %4s...</font><br>")
                                      .arg(statusCode)
                                      .arg(currentRetryCount + 1)
                                      .arg(maxRetries)
                                      .arg(waitMs / 1000.0));
                              }

                        currentReply->deleteLater();
                        currentReply = nullptr;

                        currentRetryCount++;
                        isRetrying = true; // Flag setzen

                        // Timer starten für nächsten Versuch
                        QTimer::singleShot(waitMs, this, &Agent::sendChatRequest);
                        return;
                        }
                  else {
                        chatDisplay->append(
                            QString("<br><font color='red'><b>[Abbruch]:</b> Zu viele Versuche (%1).</font><br>").arg(maxRetries));
                        }
                  }

            QString errorMessage;

            // Spezifische Meldung für den Timeout-Fall generieren
            if (currentReply->error() == QNetworkReply::TimeoutError) {
                  errorMessage = "Timeout: Der LL-Server hat nicht innerhalb von 60 Sekunden geantwortet. Eventuell lädt das "
                                 "Modell noch oder der Server hängt.";
                  }
            else {
                  errorMessage = currentReply->errorString();
                  }

            Debug("Netzwerk/API-Fehler: {}", errorMessage.toStdString());

            // Dem Nutzer den Fehler im UI anzeigen
            chatDisplay->append(QString("<br><font color='red'><b>[Verbindungsabbruch]:</b> %1</font><br>").arg(errorMessage));

            currentReply->deleteLater();
            currentReply      = nullptr;
            currentRetryCount = 0;

            // Wichtig: UI wieder freigeben!
            setInputEnabled(true);
            userInput->setFocus();
            return;
            }
      // 1. Gemini Tool-Calls aus dem Stream finalisieren
      for (auto& [index, tc] : openAIToolCallAssembler) {
            try {
                  // Argumente vom String zurück ins Objekt parsen
                  std::string argsStr         = tc["function"]["arguments"].get<std::string>();
                  tc["function"]["arguments"] = argsStr.empty() ? json::object() : json::parse(argsStr);

                  // Generiere Fallback-ID, falls API keine mitgeliefert hat
                  if (!tc.contains("id"))
                        tc["id"] = "call_" + std::to_string(rand());
                  currentToolCalls.push_back(tc);
                  }
            catch (const json::parse_error& e) {
                  Debug("LLM hat defektes JSON in den Tool-Argumenten generiert: {}", e.what());

                  // Fehler an das Modell melden, damit es sich selbst korrigieren kann!
                  tc["function"]["arguments"] = {
                           {"error_info", "JSON Parse Error: " + std::string(e.what())}
                        };
                  if (!tc.contains("id"))
                        tc["id"] = "call_" + std::to_string(rand());
                  currentToolCalls.push_back(tc);
                  }
            }
      openAIToolCallAssembler.clear();

      // 2. Anthropic Tool-Calls finalisieren
      for (auto const& [index, tc] : anthropicToolCallAssembler) {
            try {
                  json finalToolCall;
                  finalToolCall["type"] = "function";
                  finalToolCall["id"]   = tc.value("id", ""); // ID direkt aus tc

                  json function;
                  // KORREKTUR: Anthropic legt 'name' direkt in 'tc' ab, nicht in 'function'
                  function["name"] = tc.value("name", "unknown_tool");

                  // Das 'input' Feld bei Anthropic enthält die Argumente
                  if (tc.contains("input")) {
                        if (tc["input"].is_string()) {
                              std::string inputStr = tc["input"].get<std::string>();
                              // FIX: Leere Strings abfangen, sonst wirft json::parse eine Exception

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
                              // tc["input"] ist bereits ein JSON-Objekt -> direkt übernehmen
                              function["arguments"] = tc["input"];
                              }

                        finalToolCall["function"] = function;
                        currentToolCalls.push_back(finalToolCall);
                        }
                  }
            catch (const json::parse_error& e) {
                  Debug("Anthropic JSON Parse Error: {}", e.what());
                  // ... Fehler-Handling wie gehabt
                  }
            }
      anthropicToolCallAssembler.clear();

      if (currentReply) {
            currentReply->deleteLater();
            currentReply = nullptr;
            }
      currentRetryCount = 0;
      isRetrying        = false;

      // Assistenten-Antwort zur Historie hinzufügen
      json assistantMsg;
      assistantMsg["role"] = "assistant";

      if (!currentToolCalls.empty()) {
            if (model.interface == "anthropic") {
                  // Anthropic erwartet eine andere Struktur für die Tool-Antworten in der History
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

      updateChatDisplay(); // SOFORT RENDERN

      // --- Agenten-Aktion: Tools ausführen ---
      if (!currentToolCalls.empty()) {

            // Container für Anthropic: Alle Ergebnisse müssen in EINE Nachricht gesammelt werden
            json anthropicContentArray = json::array();

            for (const auto& toolCall : currentToolCalls) {
                  std::string funcName = toolCall["function"]["name"].get<std::string>();
                  json args            = toolCall["function"]["arguments"];

                  QString result = executeTool(funcName, args);

                  if (model.interface == "anthropic") {
                        // ANTHROPIC: Ergebnis sammeln, aber noch nicht senden
                        json toolResult;
                        toolResult["type"]        = "tool_result";
                        toolResult["tool_use_id"] = toolCall.value("id", "");
                        toolResult["content"]     = result.toStdString();

                        // Fehlerindikation für das Modell, falls das Tool fehlschlug (optional)
                        if (result.startsWith("Error:") || result.startsWith("Fehler:"))
                              toolResult["is_error"] = true;

                        anthropicContentArray.push_back(toolResult);
                        }
                  else {
                        // OPENAI / OLLAMA: Jedes Tool ist eine eigene Nachricht
                        json toolResponseMsg;
                        toolResponseMsg["role"] = "tool";
                        toolResponseMsg["name"] = funcName;
                        if (toolCall.contains("id"))
                              toolResponseMsg["tool_call_id"] = toolCall["id"];
                        toolResponseMsg["content"] = result.toStdString();
                        chatHistory.push_back(toolResponseMsg);
                        }
                  }

            // NACH der Schleife: Die gesammelten Anthropic-Ergebnisse als EINE Nachricht pushen
            if (model.interface == "anthropic") {
                  json toolResponseMsg;
                  toolResponseMsg["role"]    = "user";
                  toolResponseMsg["content"] = anthropicContentArray;
                  chatHistory.push_back(toolResponseMsg);
                  }

            updateChatDisplay(); // Update UI

            // Ergebnis an LLM zurücksenden
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
      // Als JSON speichern
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
      // 1. Alten Stand speichern
      saveStatus();

      // 2. Prüfen und in Git committen
      QString commitMsg = "Auto-commit: Ende von Session " + QFileInfo(currentSessionFileName).fileName();
      commitGitChanges(commitMsg);

      // 3. UI und Historie zurücksetzen
      chatHistory = json::array();
      chatDisplay->clear();
      streamBuffer.clear();
      currentAssistantMessage.clear();
      currentToolCalls = json::array();

      // 4. Neuen Dateinamen generieren
      currentSessionFileName = generateSessionFileName();

      // 5. System-Prompt wieder initialisieren (Kopie aus connectToServer)
      json systemMsg;
      systemMsg["role"]    = "system";
      std::string manifest = "Du bist ein erfahrener C++ Entwickler... (hier dein Manifest)";

      QString fullPath = _editor->projectRoot() + "/agents.md";
      QFile file(fullPath);
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            manifest += "\n" + in.readAll().toStdString();
            }
      systemMsg["content"] = manifest;
      chatHistory.push_back(systemMsg);

      // 6. Bestätigung im UI
      chatDisplay->append(
          QString("<i>[System: Neue Session gestartet: <b>%1</b>]</i><br>").arg(QFileInfo(currentSessionFileName).fileName()));

      // Fokus wieder ins Eingabefeld setzen
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

      // 1. Prüfen, ob es uncommittete Änderungen gibt
      process.start("git", QStringList() << "status" << "--porcelain");
      process.waitForFinished();
      QByteArray output = process.readAllStandardOutput();

      if (output.trimmed().isEmpty()) {
            Debug("Git: Keine Änderungen gefunden. Überspringe Commit.");
            return false;
            }

      chatDisplay->append("<i>[System: Änderungen erkannt, führe Git Auto-Commit aus...]</i>");

      // 2. Alle Änderungen stagen (git add .)
      process.start("git", QStringList() << "add" << ".");
      process.waitForFinished();

      // 3. Änderungen committen
      process.start("git", QStringList() << "commit" << "-m" << commitMessage);
      process.waitForFinished();

      chatDisplay->append("<i>[System: Git Auto-Commit erfolgreich durchgeführt.]</i>");
      return true;
      }

//---------------------------------------------------------
//   saveStatus
//---------------------------------------------------------
void Agent::saveStatus() {
      // chatHistory is considered empty if it contains only the system manifest
      if (currentSessionFileName.isEmpty() || chatHistory.size() <= 1)
            return;

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
      QDir dir(projRoot);
      QStringList filters;
      filters << "Session-*.json"; // GEÄNDERT: Wir suchen nur noch .json

      QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);

      if (!files.isEmpty()) {
            currentSessionFileName = files.first().absoluteFilePath();

            QFile file(currentSessionFileName);
            if (file.open(QIODevice::ReadOnly)) {
                  try {
                        // 1. Gedächtnis laden
                        chatHistory = json::parse(file.readAll().toStdString());

                        // 2. Visuelle Darstellung generieren
                        updateChatDisplay();

                        chatDisplay->append("<br><i>[System: Letzte Session (" + files.first().fileName() + ") fortgesetzt.]</i><br>");

                        // Nach unten scrollen
                        QTextCursor cursor = chatDisplay->textCursor();
                        cursor.movePosition(QTextCursor::End);
                        chatDisplay->setTextCursor(cursor);
                        }
                  catch (const std::exception& e) {
                        Critical("Fehler beim Laden der JSON-Historie: {}", e.what());
                        chatHistory = json::array();
                        }
                  }
            }
      else {
            // Keine Dateien gefunden -> Neue Session starten
            currentSessionFileName = generateSessionFileName();
            chatDisplay->append("<i>[System: Keine vorherige Session gefunden. Neue Session gestartet.]</i><br>");

            chatHistory = json::array();
            json systemMsg;
            systemMsg["role"] = "system";

            std::string manifest = "Du bist ein erfahrener C++ Entwickler. Deine Aufgabe ist es, Code im Projekt zu analysieren, zu "
                                   "schreiben und Build-Fehler zu beheben.";
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
