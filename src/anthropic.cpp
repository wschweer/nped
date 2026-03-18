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

#include "anthropic.h"
#include "agent.h"
#include "chatdisplay.h"
#include "historymanager.h"

//---------------------------------------------------------
//   AnthropicClient
//---------------------------------------------------------

AnthropicClient::AnthropicClient(Agent* a, Model* m, const std::vector<json>& mcps) : LLMClient(a, m) {
      try {
            tools = json::array();
            for (auto& tool : mcps) {
                  tools.push_back({
                           {        "name",        tool["name"]},
                           { "description", tool["description"]},
                           {"input_schema", tool["inputSchema"]}  // Anthropic expects input_schema
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
//   prompt
//    prepare prompt for Anthropic
//---------------------------------------------------------

json AnthropicClient::prompt(QNetworkRequest* request) {
      request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
      request->setRawHeader("x-api-key", model->apiKey.toUtf8());
      request->setRawHeader("anthropic-version", "2023-06-01");
      // request->setRawHeader("anthropic-beta", "messages-2023-12-15"); // if needed for tool streaming or beta features

      QUrl url(model->baseUrl.isEmpty() ? "https://api.anthropic.com/v1/messages" : model->baseUrl);
      request->setUrl(url);

      json anthropicRequest;
      anthropicRequest["model"]      = model->modelIdentifier.toStdString();
      anthropicRequest["max_tokens"] = 4096;
      anthropicRequest["stream"]     = true;
      if (!tools.empty())
            anthropicRequest["tools"] = tools;

      json anthropicMessages = json::array();

      anthropicRequest["system"] = agent->getManifest();

      for (const auto& msg : agent->historyManager->data()) {
            json item        = msg.content;
            std::string role = item.value("role", "");

            if (role == "system") {
                  // system messages are not allowed in messages array for Anthropic, they are in system prompt
                  continue;
                  }
            else if (role == "tool") {
                  json toolMsg;
                  toolMsg["role"]           = "user";
                  json contentArray         = json::array();
                  json toolResult           = json::object();
                  toolResult["type"]        = "tool_result";
                  toolResult["tool_use_id"] = item.value("tool_call_id", "");
                  toolResult["content"]     = item.value("content", "");
                  contentArray.push_back(toolResult);
                  toolMsg["content"] = contentArray;
                  anthropicMessages.push_back(toolMsg);
                  }
            else if (role == "assistant") {
                  if (item.contains("tool_calls") && item["tool_calls"].is_array()) {
                        json contentArray = json::array();
                        if (item.contains("content") && item["content"].is_string() && !item["content"].get<std::string>().empty()) {
                              json textBlock;
                              textBlock["type"] = "text";
                              textBlock["text"] = item["content"];
                              contentArray.push_back(textBlock);
                              }
                        for (const auto& tc : item["tool_calls"]) {
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
                  else if (item.contains("content")) {
                        // Fix for error: Ensure content is a string
                        json converted = item;
                        if (converted["content"].is_array()) {
                              std::string s = "";
                              for (const auto& part : converted["content"])
                                    if (part.is_string())
                                          s += part.get<std::string>();
                              converted["content"] = s;
                              }
                        anthropicMessages.push_back(converted);
                        }
                  else {
                        anthropicMessages.push_back(item);
                        }
                  }
            else {
                  // Ensure user role content is string if array of strings was accidentally created
                  json cleaned = item;
                  if (cleaned.contains("content") && cleaned["content"].is_array()) {
                        std::string s = "";
                        for (const auto& part : cleaned["content"])
                              if (part.is_string())
                                    s += part.get<std::string>();
                        cleaned["content"] = s;
                        }
                  anthropicMessages.push_back(cleaned);
                  }
            }

      anthropicRequest["messages"] = anthropicMessages;
      currentContent.clear();
      _currentToolCalls.clear();
      return anthropicRequest;
      }

//---------------------------------------------------------
//   processJsonItem
//    Hilfsfunktion für die Verarbeitung eines einzelnen JSON-Items
//---------------------------------------------------------

void AnthropicClient::processJsonItem(const json& item) {
      if (!item.contains("type"))
            return;

      std::string type = item["type"];

      if (type == "content_block_delta") {
            const auto& delta = item["delta"];
            if (delta.contains("type") && delta["type"] == "text_delta") {
                  std::string text = delta["text"];
                  agent->chatDisplay->handleIncomingChunk("", text);
                  currentContent += text;
                  }
            else if (delta.contains("type") && delta["type"] == "input_json_delta") {
                  // Append to current tool call arguments string
                  if (!_currentToolCalls.empty()) {
                        auto& currentCall = _currentToolCalls.back();
                        if (!currentCall.contains("arguments_str"))
                              currentCall["arguments_str"] = "";
                        currentCall["arguments_str"] =
                            currentCall["arguments_str"].get<std::string>() + delta["partial_json"].get<std::string>();
                        }
                  }
            }
      else if (type == "content_block_start") {
            const auto& block = item["content_block"];
            if (block.contains("type") && block["type"] == "tool_use") {
                  json toolCall;
                  toolCall["id"]       = block["id"];
                  toolCall["function"] = {
                           {     "name",  block["name"]},
                           {"arguments", json::object()}
                        };
                  toolCall["arguments_str"] = "";
                  _currentToolCalls.push_back(toolCall);
                  }
            }
      }

//---------------------------------------------------------
//   processTools
//---------------------------------------------------------

void AnthropicClient::processTools() {
      try {
            for (auto& call : _currentToolCalls) {
                  std::string functionName = call["function"]["name"];

                  // Arguments are already parsed into call["function"]["arguments"] by dataFinished()
                  json args = call["function"]["arguments"];

                  std::string result = agent->executeTool(functionName, args);

                  json msg;
                  msg["role"]    = "tool";
                  msg["content"] = result;
                  msg["name"]    = functionName;
                  if (call.contains("id"))
                        msg["tool_call_id"] = call["id"];
                  msg["function"] = call["function"];

                  // show on display
                  std::string thinking;
                  std::string text;
                  agent->logContent(msg, text, thinking);
                  agent->chatDisplay->handleIncomingChunk(thinking, text);

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

void AnthropicClient::dataFinished() {
      json responseContent;
      responseContent["role"]    = "assistant";
      responseContent["content"] = currentContent;

      // We need to store tool_calls in standard format so that prompt() can convert them later
      if (!_currentToolCalls.empty()) {
            json standardToolCalls = json::array();
            for (auto& call : _currentToolCalls) {
                  if (call.contains("arguments_str")) {
                        std::string argsStr = call["arguments_str"];
                        if (!argsStr.empty())
                              call["function"]["arguments"] = json::parse(argsStr);
                        call.erase("arguments_str");
                        }
                  standardToolCalls.push_back(call);
                  }
            responseContent["tool_calls"] = standardToolCalls;
            }

      currentContent.clear();

      size_t totalTokens = 0; // Anthropic usage could be extracted from "message_stop" item

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
