//============================================================================="
//  nped Program Editor
//
//  Copyright (C) 2025-2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//==============================================================================
#include <algorithm>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QByteArray>
#include <QTimer>

#include "gemini.h"
#include "agent.h"
#include "chatdisplay.h"
#include "session.h"

//---------------------------------------------------------
//   GeminiClient
//---------------------------------------------------------

GeminiClient::GeminiClient(Agent* a, Model* m, const std::vector<json>& mcps) : LLMClient(a, m) {
      setTools(mcps);
      }

//---------------------------------------------------------
//   setTools
//---------------------------------------------------------

void GeminiClient::setTools(const std::vector<json>& mcps) {
      try {
            json fd = json::array();
            for (auto& tool : mcps) {
                  json schema = tool["inputSchema"];
                  sanitizeSchemaRecursive(schema, true);
                  fd.push_back({
                           {       "name",        tool["name"]},
                           {"description", tool["description"]},
                           { "parameters",              schema}
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
      }

void GeminiClient::sanitizeSchemaRecursive(json& schema, bool isRoot) {
      if (!schema.is_object())
            return;

      // Gemini wants the root of the tool's parameters to be an object.
      if (isRoot && schema["type"] != "object") {
            // We'll leave it, but the API might still reject it.
            }

      if (schema.contains("type") && schema["type"] == "object") {
            if (schema.contains("properties"))
                  for (auto it = schema["properties"].begin(); it != schema["properties"].end(); ++it)
                        sanitizeSchemaRecursive(it.value(), false);
            }
      else if (schema.contains("type") && schema["type"] == "array") {
            if (schema.contains("items"))
                  sanitizeSchemaRecursive(schema["items"], false);
            }

      // We only keep what's strictly necessary for a function declaration.
      std::vector<std::string> allowedKeys = {"type",     "description", "properties",
                                              "required", "items",       "enum"};
      for (auto it = schema.begin(); it != schema.end();) {
            if (it.value().is_null()) {
                  it = schema.erase(it);
                  }
            else if (!it.key().empty() &&
                     std::find(allowedKeys.begin(), allowedKeys.end(), it.key()) == allowedKeys.end()) {
                  it = schema.erase(it);
                  }
            else {
                  ++it;
                  }
            }
      }

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

      // Transform history: embed images stored as generic "images" array (or legacy "image")
      // into Gemini's inline_data part format
      json contents = json::array();

      auto addMessage = [&contents](const json& msg) {
            if (!contents.empty() && contents.back()["role"] == msg["role"]) {
                  // Merge parts
                  auto& lastMsg  = contents.back();
                  json newParts  = msg.contains("parts") ? msg["parts"] : json::array();
                  json lastParts = lastMsg.contains("parts") ? lastMsg["parts"] : json::array();
                  for (const auto& part : newParts)
                        lastParts.push_back(part);
                  lastMsg["parts"] = lastParts;
                  }
            else {
                  contents.push_back(msg);
                  }
            };

      for (auto msg : agent->session()->getActiveEntries()) {
            if (msg.contains("role") && msg["role"] == "assistant")
                  msg["role"] = "model";

            if (!msg.contains("parts") && msg.contains("content")) {
                  if (msg["content"].is_string())
                        msg["parts"] = json::array({{{"text", msg["content"]}}});
                  else
                        msg["parts"] = json::array({{{"text", msg["content"].dump()}}});
                  msg.erase("content");
                  }

            // New format: "images" array
            if (msg.contains("images") && msg["images"].is_array()) {
                  for (const auto& imgItem : msg["images"]) {
                        std::string b64 = imgItem.get<std::string>();
                        msg["parts"].push_back({
                                 {"inline_data", {{"mime_type", "image/jpeg"}, {"data", b64}}}
                              });
                        }
                  msg.erase("images");
                  }
            // Legacy format: single "image" string
            else if (msg.contains("image")) {
                  std::string b64 = msg["image"].get<std::string>();
                  msg["parts"].push_back({
                           {"inline_data", {{"mime_type", "image/jpeg"}, {"data", b64}}}
                        });
                  msg.erase("image");
                  }
            addMessage(msg);
            }

      requestJson["contents"]         = contents;
      requestJson["generationConfig"] = {
               {"thinking_config",
                {{"include_thoughts", true}, {"thinking_level", "MEDIUM"}}}, // LOW, MINIMAL, MEDIUM, HIGH
               {    "temperature",                                     1.0}, // Reasoning-Modelle profitieren oft von etwas höherer Temperatur
               {           "topP",                                    0.95}
            };

      currentContent.clear();
      _lastUsageMetadata.clear();
      return requestJson;
      }

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
                  if (part.contains("functionCall")) {
                        _currentToolCalls.push_back(part);
                        agent->chatDisplay->startNewStreamingMessage("tool");
                        }
                  if (part.contains("text")) {
                        std::string s = part["text"];
                        if (!s.empty()) {
                              if (part.contains("thought") && part["thought"] == true)
                                    agent->chatDisplay->handleIncomingChunk(s, "");
                              else
                                    agent->chatDisplay->handleIncomingChunk("", s);
                              }
                        }
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

            agent->chatDisplay->startNewStreamingMessage("model");

            msg["role"]  = "user";
            msg["parts"] = json::array();

            std::string displayMsg;
            for (const auto& call : _currentToolCalls) {
                  if (!call.contains("functionCall")) {
                        Critical("ToolCall does not contain <functionCall>");
                        continue;
                        }
                  json fc = call["functionCall"];
                  if (!fc.is_object() || !fc.contains("name") || !fc["name"].is_string()) {
                        Critical("ToolCall does not contain valid <name>");
                        continue;
                        }
                  json args = (fc.contains("args") && fc["args"].is_object()) ? fc["args"] : json::object();

                  std::string result = agent->executeTool(fc["name"], args);

                  msg["parts"].push_back({
                           {"functionResponse", {{"name", fc["name"]}, {"response", {{"content", result}}}}}
                        });
                  if (!agent->filterToolMessages)
                        displayMsg += agent->formatToolCall(fc["name"], args);
                  }

            // show on display
            std::string thinking;
            std::string text;
            agent->logContent(msg, text, thinking);
            agent->chatDisplay->startNewStreamingMessage("tool");
            agent->chatDisplay->handleIncomingChunk(thinking, text);

            // put on history
            agent->session()->addRequest(msg, 0);
            _currentToolCalls.clear();
            agent->sendMessage2();
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
            agent->session()->addResult(responseContent, totalTokens);
            agent->stopAgent();
            }
      else {
            // Tool calls detected: Add the assistant's call to history first
            agent->session()->addRequest(responseContent, totalTokens);

            // processTools() will execute the tools and internally call sendMessage2()
            // to send the results back to the LLM.
            processTools();
            }
      }
