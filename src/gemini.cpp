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
#include "historymanager.h"

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
      request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
      request->setRawHeader("x-goog-api-key", model->apiKey.toUtf8());

      QUrl url(model->baseUrl);
      request->setUrl(url);

      _currentToolCalls.clear();

      json requestJson;
      requestJson["model"] = model->modelIdentifier.toStdString();
      if (!tools.empty())
            requestJson["tools"] = tools;

      json jmanifest;
      jmanifest["parts"]                = json::array({{{"text", agent->getManifest()}}});
      jmanifest["role"]                 = "system";
      requestJson["system_instruction"] = jmanifest;
      json h = agent->historyManager->getActiveEntries();
      requestJson["contents"]           = h;
      requestJson["generationConfig"]   = {
               {"thinking_config", {{"include_thoughts", true}, {"thinking_level", "MEDIUM"}}}, // LOW, MINIMAL, MEDIUM, HIGH
                                                                                          //               {"temperature",  0.2},
                                                                                          //               { "candidateCount", 1 },
               {    "temperature",                                                        1.0}, // Reasoning-Modelle profitieren oft von etwas höherer Temperatur
               {           "topP",                                                       0.95}
            };

      currentContent.clear();
      _lastUsageMetadata.clear();
      return requestJson;
      };

//---------------------------------------------------------
//   processJsonItem
//    Hilfsfunktion für die Verarbeitung eines einzelnen JSON-Items
//---------------------------------------------------------

void GeminiClient::processJsonItem(const json& item) {
      if (item.contains("usageMetadata"))
            _lastUsageMetadata = item["usageMetadata"];

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
                        if (!s.empty()) {
                              if (part.contains("thought") && part["thought"] == true)
                                    agent->chatDisplay->handleIncomingChunk(s, "");
                              else
                                    agent->chatDisplay->handleIncomingChunk("", s);
                              }
                        }
                  if (part.contains("functionCall"))
                        _currentToolCalls.push_back(part);
                  currentContent["parts"].push_back(part);
                  }
            currentContent["role"] = content["role"];
            // "finishReason": "STOP"
            // "index": 0
            }
      }

//---------------------------------------------------------
//   processTools
//    process the functionCall requests from gemini
//---------------------------------------------------------

void GeminiClient::processTools() {
      json msg; // tool answer message
      try {
            msg["role"]  = "function";
            msg["parts"] = json::array();
            for (const auto& call : _currentToolCalls) {
                  if (!call.contains("functionCall")) {
                        Critical("ToolCall does not contain <functionCall>");
                        continue;
                        }
                  json fc            = call["functionCall"];
                  json args          = fc["args"];
                  std::string result = agent->executeTool(fc["name"], args);

                  msg["parts"].push_back({
                           {"functionResponse", {{"name", fc["name"]}, {"response", {{"content", result}}}}}
                        });
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
      // show on display
      std::string thinking;
      std::string text;
      agent->logContent(msg, text, thinking);
      agent->chatDisplay->handleIncomingChunk(thinking, text);

      // put on history
      // Assume tool call token count is negligible or fixed; for now use 0
      agent->historyManager->addRequest(msg, 0);
      _currentToolCalls.clear();
      agent->sendMessage2();
      }

//---------------------------------------------------------
//   dataFinished
//---------------------------------------------------------

void GeminiClient::dataFinished() {
      // Capture the content and clear the local buffer immediately
      json responseContent = currentContent;
      currentContent.clear();

      size_t totalTokens = 0;
      if (_lastUsageMetadata.contains("totalTokenCount"))
            totalTokens = _lastUsageMetadata["totalTokenCount"].get<size_t>();

      if (_currentToolCalls.empty()) {
            // No tools: this is a final turn or a summary request
            if (agent->historyManager->addResult(responseContent, totalTokens)) {
                  // trim() returned true: A summary was requested and added to history.
                  // We must send this summary request to the LLM.
                  agent->sendMessage2();
                  }
            else {
                  // Standard end of conversation turn
                  agent->enableInput(true);
                  }
            }
      else {
            // Tool calls detected: Add the assistant's call to history first
            agent->historyManager->addRequest(responseContent, totalTokens);

            // processTools() will execute the tools and internally call sendMessage2()
            // to send the results back to the LLM.
            processTools();
            }
      }
