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
#include <algorithm>
#include <cmath>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QByteArray>
#include <QTimer>

#include "gemini2.h"
#include "agent.h"
#include "chatdisplay.h"
#include "session.h"

//---------------------------------------------------------
//   Gemini2Client
//---------------------------------------------------------

Gemini2Client::Gemini2Client(Agent* a, Model* m, const std::vector<json>& mcps) : LLMClient(a, m) {
      setTools(mcps);
      }

//---------------------------------------------------------
//   setTools
//---------------------------------------------------------

void Gemini2Client::setTools(const std::vector<json>& mcps) {
      try {
            tools = json::array();
            for (auto& tool : mcps) {
                  json schema = tool["inputSchema"];
                  sanitizeSchemaRecursive(schema, true);
                  tools.push_back({
                           {       "type",          "function"},
                           {       "name",        tool["name"]},
                           {"description", tool["description"]},
                           { "parameters",              schema}
                        });
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
//   sanitizeSchemaRecursive
//---------------------------------------------------------

void Gemini2Client::sanitizeSchemaRecursive(json& schema, bool isRoot) {
      if (!schema.is_object())
            return;

      if (isRoot && schema["type"] != "object") {
            // We'll leave it
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
//---------------------------------------------------------

json Gemini2Client::prompt(QNetworkRequest* request) {
      request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
      request->setRawHeader("x-goog-api-key", model->apiKey.toUtf8());

      QUrl url(model->baseUrl);
      request->setUrl(url);

      _currentToolCalls.clear();
      _currentStepType.clear();

      json requestJson;
      requestJson["model"] = model->modelIdentifier.toStdString();
      if (!tools.empty())
            requestJson["tools"] = tools;

      requestJson["stream"] = model->stream;

      if (agent->session()->getActiveEntriesCount() <= 1)
            _lastInteractionId.clear();

      if (!_lastInteractionId.empty()) {
            requestJson["previous_interaction_id"] = _lastInteractionId;
            }
      else {
            // MUST be a Content object, not a string
            requestJson["system_instruction"] = agent->getManifest();
#if 0
                                    {
                                       { "role",                           "system"},
                                       {"parts", {{{"text", agent->getManifest()}}}}
                                    };
#endif
            }
      json inputParts    = json::array();
      auto activeEntries = agent->session()->getActiveEntries();
      if (!activeEntries.empty()) {
            const auto& msg   = activeEntries.back();
            bool isToolResult = false;
            if (msg.contains("parts") && msg["parts"].is_array()) {
                  for (const auto& part : msg["parts"]) {
                        if (part.contains("function_result")) {
                              isToolResult = true;
                              inputParts.push_back(part["function_result"]);
                              }
                        else if (part.contains("functionResponse")) {
                              isToolResult  = true;
                              json legacyFR = part["functionResponse"];
                              json resObj;
                              resObj["type"]    = "function_result";
                              resObj["call_id"] = "fc_0";
                              resObj["name"]    = legacyFR.value("name", "");
                              resObj["result"]  = legacyFR["response"].value("content", "");
                              inputParts.push_back(resObj);
                              }
                        }
                  }

            if (!isToolResult) {
                  std::string userText;
                  if (msg.contains("parts") && msg["parts"].is_array()) {
                        for (const auto& part : msg["parts"])
                              if (part.contains("text") && part["text"].is_string())
                                    userText += part["text"].get<std::string>();
                        }
                  else if (msg.contains("content")) {
                        if (msg["content"].is_string())
                              userText = msg["content"].get<std::string>();
                        else
                              userText = msg["content"].dump(-1, ' ', false, json::error_handler_t::replace);
                        }

                  if (!userText.empty()) {
                        inputParts.push_back({
                                 {"type",   "text"},
                                 {"text", userText}
                              });
                        }

                  if (msg.contains("images") && msg["images"].is_array()) {
                        for (const auto& imgItem : msg["images"]) {
                              std::string b64 = imgItem.is_string() ? imgItem.get<std::string>() : "";
                              inputParts.push_back({
                                       {     "type",      "image"},
                                       {"mime_type", "image/jpeg"},
                                       {     "data",          b64}
                                    });
                              }
                        }
                  }
            }

      if (!inputParts.empty())
            requestJson["input"] = inputParts;
      return requestJson;
      }

//---------------------------------------------------------
//   processJsonItem
//---------------------------------------------------------

void Gemini2Client::processJsonItem(const json& item) {
      if (item.contains("id"))
            _lastInteractionId = item["id"].get<std::string>();

      if (item.contains("usageMetadata"))
            _lastUsageMetadata = item["usageMetadata"];
      else if (item.contains("usage"))
            _lastUsageMetadata = item["usage"];

      std::string eventType = item.value("type", "");
      if (eventType == "step.start" && item.contains("step")) {
            _currentStepType = item["step"].value("type", "");
            }
      else if (eventType == "step.delta" && item.contains("delta")) {
            json delta = item["delta"];
            if (delta.value("type", "") == "text" && delta.contains("text") && delta["text"].is_string()) {
                  std::string s = delta["text"].get<std::string>();
                  if (!s.empty()) {
                        if (_currentStepType == "thought") {
                              agent->chatDisplay->handleIncomingChunk(s, "");
                              _accumulatedThought += s;
                              }
                        else {
                              agent->chatDisplay->handleIncomingChunk("", s);
                              _accumulatedText += s;
                              }
                        }
                  }
            }
      else if (eventType == "step.stop" && item.contains("step")) {
            json step = item["step"];
            if (step.value("type", "") == "function_call") {
                  _currentToolCalls.push_back(step);
                  agent->chatDisplay->startNewStreamingMessage("tool");
                  }
            }
      else if (eventType == "interaction.completed") {
            }

      if (item.contains("steps")) {
            for (const auto& step : item["steps"]) {
                  std::string stepType = step.value("type", "");
                  if (stepType == "model_output") {
                        if (step.contains("content")) {
                              for (const auto& chunk : step["content"]) {
                                    if (chunk.value("type", "") == "text") {
                                          std::string text = chunk.value("text", "");
                                          if (!text.empty()) {
                                                agent->chatDisplay->handleIncomingChunk("", text);
                                                _accumulatedText += text;
                                                }
                                          }
                                    }
                              }
                        }
                  else if (stepType == "thought") {
                        if (step.contains("content")) {
                              for (const auto& chunk : step["content"]) {
                                    if (chunk.value("type", "") == "text") {
                                          std::string thought = chunk.value("text", "");
                                          if (!thought.empty()) {
                                                agent->chatDisplay->handleIncomingChunk(thought, "");
                                                _accumulatedThought += thought;
                                                }
                                          }
                                    }
                              }
                        }
                  else if (stepType == "function_call") {
                        _currentToolCalls.push_back(step);
                        agent->chatDisplay->startNewStreamingMessage("tool");
                        }
                  }
            }
      }

//---------------------------------------------------------
//   processTools
//---------------------------------------------------------

void Gemini2Client::processTools() {
      json msg;
      try {
            agent->chatDisplay->startNewStreamingMessage("model");

            msg["role"]  = "user";
            msg["parts"] = json::array();

            std::string displayMsg;
            for (const auto& call : _currentToolCalls) {
                  std::string callId = call.value("id", "");
                  std::string name   = call.value("name", "");
                  json args          = (call.contains("arguments") && call["arguments"].is_object())
                                           ? call["arguments"]
                                           : json::object();

                  if (name.empty()) {
                        Critical("Tool call has empty name");
                        continue;
                        }

                  std::string result;
                  try {
                        result = agent->executeTool(name, args);
                        }
                  catch (const std::exception& e) {
                        Critical("Exception in executeTool: {}", e.what());
                        result = "Error: " + std::string(e.what());
                        }

                  msg["parts"].push_back({
                           {"function_result",
                            {{"type", "function_result"},
                             {"call_id", callId},
                             {"name", name},
                             {"result", result}}}
                        });

                  if (name == "extract_video_frames") {
                        try {
                              json imgList = json::parse(result);
                              if (imgList.is_array()) {
                                    json imgs = json::array();
                                    for (const auto& imgObj : imgList)
                                          if (imgObj.contains("data") && imgObj["data"].is_string())
                                                imgs.push_back(imgObj["data"].get<std::string>());
                                    if (!imgs.empty())
                                          msg["images"] = imgs;
                                    }
                              }
                        catch (...) {
                              }
                        }

                  if (!agent->filterToolMessages)
                        displayMsg += agent->formatToolCall(name, args);
                  }

            std::string thinking;
            std::string text;
            agent->logContent(msg, text, thinking);
            agent->chatDisplay->startNewStreamingMessage("tool");
            agent->chatDisplay->handleIncomingChunk(thinking, text);

            agent->session()->addRequest(msg, 0);
            _currentToolCalls.clear();

            try {
                  agent->sendMessage2();
                  }
            catch (const std::exception& e) {
                  Critical("Exception in sendMessage2: {}", e.what());
                  }
            }
      catch (const json::parse_error& e) {
            Critical("Parse Error in processTools: {}", e.what());
            _currentToolCalls.clear();
            agent->sendMessage2();
            }
      catch (const json::type_error& e) {
            Critical("TypeError in processTools: {}", e.what());
            _currentToolCalls.clear();
            agent->sendMessage2();
            }
      catch (const std::exception& e) {
            Critical("Exception in processTools: {}", e.what());
            _currentToolCalls.clear();
            agent->sendMessage2();
            }
      catch (...) {
            Critical("Unexpected error in processTools");
            _currentToolCalls.clear();
            agent->sendMessage2();
            }
      }

//---------------------------------------------------------
//   dataFinished
//---------------------------------------------------------

void Gemini2Client::dataFinished() {
      json responseContent;
      responseContent["role"] = "assistant";
      json parts              = json::array();
      if (!_accumulatedThought.empty()) {
            parts.push_back({
                     {   "text", _accumulatedThought},
                     {"thought",                true}
                  });
            }
      if (!_accumulatedText.empty()) {
            parts.push_back({
                     {"text", _accumulatedText}
                  });
            }
      responseContent["parts"] = parts;

      _accumulatedText.clear();
      _accumulatedThought.clear();

      size_t totalTokens = 0;
      if (_lastUsageMetadata.contains("totalTokenCount"))
            totalTokens = _lastUsageMetadata["totalTokenCount"].get<size_t>();
      else if (_lastUsageMetadata.contains("total_tokens"))
            totalTokens = _lastUsageMetadata["total_tokens"].get<size_t>();

      if (_currentToolCalls.empty()) {
            agent->session()->addResult(responseContent, totalTokens);
            agent->stopAgent();
            }
      else {
            agent->session()->addRequest(responseContent, totalTokens);
            processTools();
            }
      }
