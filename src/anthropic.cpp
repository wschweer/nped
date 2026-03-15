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
#include "anthropic.h"
#include "agent.h"

//---------------------------------------------------------
//   AnthropicClient
//---------------------------------------------------------

AnthropicClient::AnthropicClient(Model* m, const std::vector<json>& mcps) : LLMClient(m) {
      for (auto& tool : mcps) {
            tools.push_back({
                     {       "name",        tool["name"]},
                     {"description", tool["description"]},
                     {"inputSchema", tool["inputSchema"]}
                  });
            }
      }

//---------------------------------------------------------
//   sendPrompt
//---------------------------------------------------------

json AnthropicClient::prompt(QNetworkRequest* request, const json& chatHistory) {
      request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
      request->setRawHeader("x-api-key", model->apiKey.toUtf8());
      request->setRawHeader("anthropic-version", "2023-06-01");

      json anthropicRequest;
      anthropicRequest["model"]      = model->modelIdentifier.toStdString();
      anthropicRequest["max_tokens"] = 4096;
      anthropicRequest["stream"]     = true;
      anthropicRequest["tools"]      = tools;

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
                  // Anthropic expects tool_calls as a content array with type="tool_use",
                  // not as a separate "tool_calls" key (OpenAI/Ollama format).
                  // Conversion is necessary so that subsequent requests after tool calls do not
                  // fail with HTTP 400.
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
                        // Normal assistant response without tool calls: adopt directly
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
      return anthropicRequest;
      };
