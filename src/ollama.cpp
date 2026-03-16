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

#include "ollama.h"
#include "agent.h"
#include "chatdisplay.h"

//---------------------------------------------------------
//   OllamaClient
//---------------------------------------------------------

OllamaClient::OllamaClient(Agent* a, Model* m, const std::vector<json>& mcps) : LLMClient(a, m) {
      try {
            tools = json::array();
            for (auto& tool : mcps) {
                  json t;
                  t["type"]     = "function";
                  t["function"] = {
                           {       "name",        tool["name"]},
                           {"description", tool["description"]},
                           { "parameters", tool["inputSchema"]}
                        };
                  tools.push_back(t);
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
      };

//---------------------------------------------------------
//   prompt
//    prepare prompt for Ollama
//---------------------------------------------------------

json OllamaClient::prompt(QNetworkRequest* request) {
      Debug("===");
      request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

      QUrl url("http://localhost:11434/api/chat");
      request->setUrl(url);

      json requestJson;
      requestJson["model"]  = model->modelIdentifier.toStdString();
      requestJson["stream"] = true;
      if (!tools.empty())
            requestJson["tools"] = tools;

      json history = json::array();

      //=========================================
      // add manifest to prompt
      //=========================================

      json jmanifest;
      jmanifest["content"] = agent->getManifest();
      jmanifest["role"]    = "system";
      history.push_back(jmanifest);

      for (const auto& msg : agent->chatHistory) {
            json jmsg;
            if (msg.contains("role"))
                  jmsg["role"] = msg["role"];
            else
                  Debug("no role: <{}>", msg.dump(3));
            if (msg.contains("content"))
                  jmsg["content"] = msg["content"];
            history.push_back(jmsg);
            }
      requestJson["messages"] = history;
      currentContent.clear();
      return requestJson;
      };

//---------------------------------------------------------
//   processJsonItem
//    Hilfsfunktion für die Verarbeitung eines einzelnen JSON-Items
//---------------------------------------------------------

void OllamaClient::processJsonItem(const json& item) {
      Debug("Received Item: <{}>", item.dump(3));
      if (!item.contains("message")) {
            Critical("item has no message");
            return;
            }
      const auto& message = item["message"];
      if (message.contains("content")) {
            std::string s = message["content"];
            if (!s.empty()) {
                  emit incomingChunk("", QString::fromStdString(s));
                  currentContent += s;
                  }
            }
      if (message.contains("tool_calls"))
            for (const auto& toolCall : message["tool_calls"])
                  _currentToolCalls.push_back(toolCall);
      }

//---------------------------------------------------------
//   dataFinished
//---------------------------------------------------------

void OllamaClient::dataFinished(QNetworkReply* reply) {
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
      json response;
      response["role"]    = "assistant";
      response["content"] = currentContent;
      agent->chatHistory.push_back(response);

      if (!_currentToolCalls.empty()) {
            std::string thinking;
            Debug("=====Tool Calls <{}>", _currentToolCalls.size(), _currentToolCalls.dump(3));
            for (const auto& call : _currentToolCalls) {
                  try {
                        // Argumente von String/JSON-Fragmenten in ein JSON-Objekt umwandeln
                        json fc                  = call["function"];
                        json args                = fc["arguments"];
                        std::string functionName = fc["name"];
                        json toolCall;
                        std::string result = agent->executeTool(functionName, args);
                        // append result of tool call to chatHistory
                        json msg;
                        msg["role"]     = std::string("function");
                        msg["content"]  = result;
                        msg["function"] = fc;
                        agent->chatHistory.push_back(msg); // tool result

                        std::string argsStr = "";
                        bool first          = true;
                        for (auto& [key, value] : args.items()) {
                              if (!first)
                                    argsStr += ", ";
                              argsStr += std::format("{}={}", key, value.dump());
                              first    = false;
                              }

                        result = agent->truncateOutput(result, Agent::kChatResultMaxChars);
                        std::string s = agent->formatToolCall(functionName, args, result);
                        emit incomingChunk(QString::fromStdString(thinking), QString::fromStdString(s));
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
