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
#include "historymanager.h"

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

      for (const auto& msg : agent->historyManager->getActiveEntries()) {
            json jmsg;
            if (msg.contains("role"))
                  jmsg["role"] = msg["role"];
            else
                  Debug("no role: <{}>", msg.dump(3));
            if (msg.contains("content"))
                  jmsg["content"] = msg["content"];
            if (msg.contains("tool_calls"))
                  jmsg["tool_calls"] = msg["tool_calls"];
            if (msg.contains("name"))
                  jmsg["name"] = msg["name"];
            if (msg.contains("tool_call_id"))
                  jmsg["tool_call_id"] = msg["tool_call_id"];
            history.push_back(jmsg);
            }
      requestJson["messages"] = history;
      currentContent.clear();
      _currentToolCalls.clear();
      return requestJson;
      };

//---------------------------------------------------------
//   processJsonItem
//    Hilfsfunktion für die Verarbeitung eines einzelnen JSON-Items
//---------------------------------------------------------

void OllamaClient::processJsonItem(const json& item) {
      if (!item.contains("message")) {
            return;
            }
      const auto& message = item["message"];
      if (message.contains("content")) {
            std::string s = message["content"];
            if (!s.empty()) {
                  agent->chatDisplay->handleIncomingChunk("", s);
                  currentContent += s;
                  }
            }
      if (message.contains("tool_calls")) {
            for (const auto& toolCall : message["tool_calls"])
                  _currentToolCalls.push_back(toolCall);
            }
      }

//---------------------------------------------------------
//   processTools
//    process the functionCall requests from ollama
//---------------------------------------------------------

void OllamaClient::processTools() {
      try {
            for (const auto& call : _currentToolCalls) {
                  if (!call.contains("function")) {
                        Critical("ToolCall does not contain <function>");
                        continue;
                        }
                  json fc            = call["function"];
                  json args          = fc["arguments"];
                  if (args.is_string())
                        args = json::parse(args.get<std::string>());
                  fc["arguments"]    = args;
                  std::string functionName = fc["name"];

                  std::string result = agent->executeTool(functionName, args);

                  json msg;
                  msg["role"] = "tool";
                  msg["content"] = result;
                  msg["name"] = functionName;
                  if (call.contains("id"))
                        msg["tool_call_id"] = call["id"];
                  msg["function"] = fc; // For logContent

                  // show on display
                  std::string thinking;
                  std::string text;
                  agent->logContent(msg, text, thinking);
                  agent->chatDisplay->handleIncomingChunk(thinking, text);

                  // Don't leak 'function' field to Ollama prompt if it doesn't need it
                  // Wait, prompt() only copies what it needs (role, content, tool_calls, name).
                  // So we can leave it in msg.
                  agent->historyManager->addRequest(msg, 0);
                  }
            }
      catch (const json::parse_error& e) {
            Critical("Parse Error: {}", e.what());
            }
      catch (const json::type_error& e) {
            Critical("TypeError: {}", e.what());
            }
      catch (...) {
            Critical("Unexpected error");
            }

      _currentToolCalls.clear();
      agent->sendMessage2();
      }

//---------------------------------------------------------
//   dataFinished
//---------------------------------------------------------

void OllamaClient::dataFinished() {
      json responseContent;
      responseContent["role"] = "assistant";
      responseContent["content"] = currentContent;
      if (!_currentToolCalls.empty())
            responseContent["tool_calls"] = _currentToolCalls;

      currentContent.clear();

      size_t totalTokens = 0;

      if (_currentToolCalls.empty()) {
            if (agent->historyManager->addResult(responseContent, totalTokens)) {
                  agent->sendMessage2();
                  }
            else {
                  agent->enableInput(true);
                  }
            }
      else {
            agent->historyManager->addRequest(responseContent, totalTokens);
            processTools();
            }
      }
