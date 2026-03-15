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

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QByteArray>
#include <QTimer>

#include "gemini.h"
#include "agent.h"
#include "chatdisplay.h"

//---------------------------------------------------------
//   GeminiClient
//---------------------------------------------------------

GeminiClient::GeminiClient(Agent* a, Model* m, const std::vector<json>& mcps) : LLMClient(a, m) {
      try {
            json fd = json::array();
            for (auto& tool : mcps) {
                  fd.push_back({
                           {       "name",        tool["name"]},
                           {"description", tool["description"]},
                           { "parameters", tool["inputSchema"]}
                        });
                  }
            tools = json::array({{{"function_declarations", fd}}});
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
      };

//---------------------------------------------------------
//   prompt
//    prepare prompt for Gemini
//---------------------------------------------------------

json GeminiClient::prompt(QNetworkRequest* request) {
      Debug("===");
      request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
      request->setRawHeader("x-goog-api-key", model->apiKey.toUtf8());

      QUrl url(model->baseUrl);
      request->setUrl(url);

      json requestJson;
      requestJson["model"] = model->modelIdentifier.toStdString();
      if (!tools.empty())
            requestJson["tools"] = tools;

      json history = json::array();
      Debug("chat-history: <{}>", agent->chatHistory.dump(3));
      for (auto msg : agent->chatHistory) {
            json jmsg;
            if (msg["role"] == "function") {
                  // Tool-Ergebnis Format
                  jmsg["role"]  = "function";
                  jmsg["parts"] = msg["parts"];
                  }
            else if (msg["role"] == "assistant" || msg["role"] == "model") {
                  jmsg["role"] = "model";
                  json parts   = msg["parts"];

                  // Falls diese Nachricht Tool-Calls enthielt, müssen diese
                  // im korrekten Format zurückgesendet werden
                  if (msg.contains("toolCalls"))
                        for (const auto& call : msg["toolCalls"])
                              parts.push_back(call);
                  jmsg["parts"] = parts;
                  }

            else {
                  jmsg["role"]  = (msg["role"] == "assistant") ? "model" : msg["role"];
                  jmsg["parts"] = msg["parts"];
                  }
            history.push_back(jmsg);
            }
      json jmanifest;
      jmanifest["parts"]                = json::array({{{"text", agent->getManifest()}}});
      jmanifest["role"]                 = "system";
      requestJson["system_instruction"] = jmanifest;
      requestJson["contents"]           = history;
      requestJson["generationConfig"]   = {
               {"thinking_config", {{"include_thoughts", true}, {"thinking_level", "HIGH"}}},
         //               {"temperature",  0.2},
               {    "temperature",                                                      0.7}, // Reasoning-Modelle profitieren oft von etwas höherer Temperatur
               {           "topP",                                                     0.95}
            };

      streamBuffer.clear();
      currentContent.clear();
      return requestJson;
      };

//---------------------------------------------------------
//   dataReceived
//---------------------------------------------------------

void GeminiClient::dataReceived(QNetworkReply* reply) {
      QByteArray newData = reply->readAll();
      Debug("Received: <{}>", newData.data());
      streamBuffer.append(newData);

      // Der Stream liefert oft Fragmente wie: [ {obj1}, {obj2} ...
      // Wir entfernen führende [ oder , um die Objekte einzeln zu parsen
      std::string content = streamBuffer.toStdString();

      // Einfacher heuristischer Splitter für JSON-Objekte im Array
      size_t startPos = 0;
      while ((startPos = content.find('{', startPos)) != std::string::npos) {
            int braceCount = 0;
            size_t endPos  = std::string::npos;

            for (size_t i = startPos; i < content.length(); ++i) {
                  if (content[i] == '{')
                        braceCount++;
                  else if (content[i] == '}')
                        braceCount--;

                  if (braceCount == 0) {
                        endPos = i;
                        break;
                        }
                  }

            if (endPos != std::string::npos) {
                  std::string jsonObject = content.substr(startPos, endPos - startPos + 1);
                  try {
                        auto j = json::parse(jsonObject);
                        processJsonItem(j); // Hilfsfunktion zur Verarbeitung
                        }
                  catch (const json::parse_error&) {
                        // Fragment ist noch nicht vollständig
                        break;
                        }
                  startPos = endPos + 1;
                  // Puffer bis hierhin leeren
                  streamBuffer.remove(0, static_cast<int>(startPos));
                  content  = streamBuffer.toStdString();
                  startPos = 0;
                  }
            else {
                  break; // Kein vollständiges Objekt gefunden
                  }
            }
      }

//---------------------------------------------------------
//   processJsonItem
//    Hilfsfunktion für die Verarbeitung eines einzelnen JSON-Items
//---------------------------------------------------------

void GeminiClient::processJsonItem(const json& item) {
      if (!item.contains("candidates"))
            return;

      for (const auto& candidate : item["candidates"]) {
            if (!candidate.contains("content")) {
                  Critical("candidata has no content");
                  continue;
                  }
            const auto& content = candidate["content"];
            if (!content.contains("parts")) {
                  Critical("content has no parts");
                  continue;
                  }
            for (const auto& part : content["parts"]) {
                  if (part.contains("text")) {
                        std::string s = part["text"];
                        if (part.contains("thought") && part["thought"] == true)
                              agent->chatDisplay->handleIncomingChunk(QString::fromStdString(s), "");
                        else
                              agent->chatDisplay->handleIncomingChunk("", QString::fromStdString(s));
                        }
                  if (part.contains("functionCall"))
                        _currentToolCalls.push_back(part);
                  currentContent["parts"].push_back(part);
                  }
            currentContent["role"] = content["role"];
            }
      }

//---------------------------------------------------------
//   dataFinished
//---------------------------------------------------------

void GeminiClient::dataFinished(QNetworkReply* reply) {
      if (!reply)
            return;
      // --- ERROR HANDLING & BACKOFF LOGIC ---
      if (reply->error() != QNetworkReply::NoError) {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // Case 1: Rate Limit (429) or Server Overload (503/500/502/504)
            if (statusCode == 429 || (statusCode >= 500 && statusCode < 600)) {
                  if (currentRetryCount < maxRetries) {
                        int waitMs = 0;
                        // A: Rate Limit (429) -> respect headers
                        if (statusCode == 429) {
                              // CORRECTION: Read "Retry-After" as raw header, as there is no enum for it
                              QByteArray retryAfterRaw = reply->rawHeader("Retry-After");

                              if (!retryAfterRaw.isEmpty()) {
                                    // Anthropic/OpenAI usually send seconds as integer here
                                    waitMs = retryAfterRaw.toInt() * 1000;
                                    }

                              // Fallback: Standard Exponential if no headers are there
                              if (waitMs <= 0)
                                    waitMs = 1000 * std::pow(2, currentRetryCount);

                              // Safety margin (jitter)
                              waitMs += (rand() % 500);

                              agent->chatDisplay->append(
                                  QString("<br><font color='orange'><b>[Rate Limit]:</b> Pause for %1 seconds...</font><br>")
                                      .arg(waitMs / 1000.0));
                              }
                        // B: Server Error (5xx) -> Exponential Backoff
                        else {
                              waitMs = 2000 * std::pow(2, currentRetryCount);
                              agent->chatDisplay->append(
                                  QString("<br><font color='orange'><b>[Server Error %1]:</b> Retry %2/%3 in %4s...</font><br>")
                                      .arg(statusCode)
                                      .arg(currentRetryCount + 1)
                                      .arg(maxRetries)
                                      .arg(waitMs / 1000.0));
                              }

                        reply->deleteLater();
                        reply = nullptr;

                        currentRetryCount++;
                        isRetrying = true; // Set flag

                        // Start timer for next attempt
                        //TODO                        QTimer::singleShot(waitMs, this, &Agent::sendChatRequest);
                        return;
                        }
                  else {
                        Debug("too many reply's");
                        agent->chatDisplay->append(
                            QString("<br><font color='red'><b>[Abort]:</b> Too many attempts (%1).</font><br>").arg(maxRetries));
                        }
                  }

            QString errorMessage;
            // Generate specific messages
            errorMessage = reply->errorString();
            switch (reply->error()) {
                  case QNetworkReply::TimeoutError:
                        errorMessage += "\nTimeout: The LL server did not respond within 60 seconds. Maybe the "
                                        "model is still loading or the server is hanging.";
                        break;
                  case QNetworkReply::ContentNotFoundError: errorMessage += "\nModell not found (bad baseUrl configured?)"; break;
                  default: break;
                  }
            Debug("Network/API error {}: {}", int(reply->error()), errorMessage);

            // Show the error to the user in the UI
            agent->chatDisplay->append(QString("<br><font color='red'><b>[Connection abort]:</b> %1</font><br>").arg(errorMessage));
            return;
            }
      QByteArray newData = reply->readAll();
      Debug("Received: <{}>", newData.data());
      agent->chatHistory.push_back(currentContent);

      if (!_currentToolCalls.empty()) {
            std::string thinking;
            for (const auto& call : _currentToolCalls) {
                  try {
                        // Argumente von String/JSON-Fragmenten in ein JSON-Objekt umwandeln
                        json fc            = call["functionCall"];
                        json args          = fc["args"];
                        std::string result = agent->executeTool(fc["name"], args);

                        // Ergebnis in die Historie einfügen (Rolle "function" für Gemini/OpenAI)
                        json msg;
                        msg["role"] = "function";
                        msg["parts"].push_back({
                                 {"functionResponse", {{"name", fc["name"]}, {"response", {{"content", result}}}}}
                              });
                        agent->chatHistory.push_back(msg); // tool result

                        std::string functionName = fc["name"];
                        std::string argsStr      = "";
                        bool first               = true;
                        for (auto& [key, value] : args.items()) {
                              if (!first)
                                    argsStr += ", ";
                              argsStr += std::format("{}={}", key, value.dump());
                              first    = false;
                              }

                        result = agent->truncateOutput(result, Agent::kChatResultMaxChars);
                        std::string s =
                            std::format("\n\n<i>[System: Execute Tool: {}({})]</i>\n\n```\n{}\n```\n\n", functionName, argsStr, result);
                        agent->chatDisplay->handleIncomingChunk(QString::fromStdString(thinking), QString::fromStdString(s));
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
                  _currentToolCalls.clear();
                  reply->deleteLater();
                  reply = nullptr;
                  agent->sendMessage2(); // TODO: keep busy
                  }
            }
      else {
            reply->deleteLater();
            reply = nullptr;
            agent->enableInput(true);
            }
      }
