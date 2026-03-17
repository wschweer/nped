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

      for (const auto& msg : agent->chatHistory.history()) {
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

void OllamaClient::dataFinished() {
#if 0
      json response;
      response["role"]    = "assistant";
      response["content"] = currentContent;
      agent->chatHistory.data.push_back(response);

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
                        agent->chatHistory.data.push_back(msg); // tool result

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
#endif
      }
