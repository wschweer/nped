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

#include "openai.h"
#include "agent.h"
#include "chatdisplay.h"
#include "historymanager.h"

//---------------------------------------------------------
//   OpenAiClient
//---------------------------------------------------------

OpenAiClient::OpenAiClient(Agent* a, Model* m, const std::vector<json>& mcps) : LLMClient(a, m) {
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
//    prepare prompt for OpenAI
//---------------------------------------------------------

json OpenAiClient::prompt(QNetworkRequest* request) {
      request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
      request->setRawHeader("Authorization", ("Bearer " + model->apiKey).toUtf8());

      QUrl url(model->baseUrl.isEmpty() ? "https://api.openai.com/v1/chat/completions" : model->baseUrl);
      request->setUrl(url);

      json requestJson;
      requestJson["model"]  = model->modelIdentifier.toStdString();
      requestJson["stream"] = true;
      if (!tools.empty())
            requestJson["tools"] = tools;

      json history = json::array();

      // system message
      json jmanifest;
      jmanifest["content"] = agent->getManifest();
      jmanifest["role"]    = "system";
      history.push_back(jmanifest);

      for (const auto& [msg, tokens] : agent->historyManager->data()) {
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
//    Helper to parse a single JSON item
//---------------------------------------------------------

void OpenAiClient::processJsonItem(const json& item) {
      if (!item.contains("choices"))
            return;
      const auto& choices = item["choices"];
      if (choices.empty())
            return;
      const auto& choice = choices[0];
      if (!choice.contains("delta"))
            return;
      const auto& delta = choice["delta"];

      if (delta.contains("content") && !delta["content"].is_null()) {
            std::string s = delta["content"];
            if (!s.empty()) {
                  agent->chatDisplay->handleIncomingChunk("", QString::fromStdString(s));
                  currentContent += s;
                  }
            }

      if (delta.contains("tool_calls")) {
            for (const auto& tc : delta["tool_calls"]) {
                  int index = tc["index"].get<int>();
                  // Ensure array is large enough
                  while (_currentToolCalls.size() <= static_cast<size_t>(index))
                        _currentToolCalls.push_back(json::object());

                  auto& currentCall = _currentToolCalls[index];

                  if (tc.contains("id"))
                        currentCall["id"] = tc["id"];
                  if (tc.contains("type"))
                        currentCall["type"] = tc["type"];

                  if (tc.contains("function")) {
                        if (!currentCall.contains("function"))
                              currentCall["function"] = json::object();

                        const auto& func = tc["function"];
                        if (func.contains("name"))
                              currentCall["function"]["name"] = func["name"];
                        if (func.contains("arguments")) {
                              if (!currentCall["function"].contains("arguments_str"))
                                    currentCall["function"]["arguments_str"] = "";
                              currentCall["function"]["arguments_str"] =
                                  currentCall["function"]["arguments_str"].get<std::string>() + func["arguments"].get<std::string>();
                              }
                        }
                  }
            }
      }

//---------------------------------------------------------
//   processTools
//    process the functionCall requests
//---------------------------------------------------------

void OpenAiClient::processTools() {
      try {
            for (const auto& call : _currentToolCalls) {
                  if (!call.contains("function")) {
                        Critical("ToolCall does not contain <function>");
                        continue;
                        }
                  json fc   = call["function"];
                  json args = json::object();
                  if (fc.contains("arguments") && fc["arguments"].is_string()) {
                        std::string argsStr = fc["arguments"].get<std::string>();
                        if (!argsStr.empty())
                              args = json::parse(argsStr);
                        }
                  else if (fc.contains("arguments") && fc["arguments"].is_object()) {
                        args = fc["arguments"];
                        }

                  fc["arguments"]          = args;
                  std::string functionName = fc["name"];

                  std::string result = agent->executeTool(functionName, args);

                  json msg;
                  msg["role"]    = "tool";
                  msg["content"] = result;
                  msg["name"]    = functionName;
                  if (call.contains("id"))
                        msg["tool_call_id"] = call["id"];
                  msg["function"] = fc; // For logContent

                  // show on display
                  std::string thinking;
                  std::string text;
                  agent->logContent(msg, text, thinking);
                  agent->chatDisplay->handleIncomingChunk(QString::fromStdString(thinking), QString::fromStdString(text));

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

void OpenAiClient::dataFinished() {
      json responseContent;
      responseContent["role"] = "assistant";
      if (!currentContent.empty())
            responseContent["content"] = currentContent;

      if (!_currentToolCalls.empty()) {
            for (auto& call : _currentToolCalls) {
                  if (call.contains("function") && call["function"].contains("arguments_str")) {
                        call["function"]["arguments"] = call["function"]["arguments_str"];
                        call["function"].erase("arguments_str");
                        }
                  }
            responseContent["tool_calls"] = _currentToolCalls;
            }

      currentContent.clear();

      size_t totalTokens = 0;

      if (_currentToolCalls.empty()) {
            if (agent->historyManager->addResult(responseContent, totalTokens))
                  agent->sendMessage2();
            else
                  agent->enableInput(true);
            }
      else {
            agent->historyManager->addRequest(responseContent, totalTokens);
            processTools();
            }
      }
