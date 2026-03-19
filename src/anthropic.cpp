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
      // Enable Extended Thinking for models that support it (claude-3-7-sonnet and later).
      // The beta header is required; thinking budget must be < max_tokens.
      const bool extendedThinking = model->modelIdentifier.contains("claude-3-7") ||
                                    model->modelIdentifier.contains("claude-3.7");
      if (extendedThinking)
            request->setRawHeader("anthropic-beta", "interleaved-thinking-2025-05-14");

      QUrl url(model->baseUrl.isEmpty() ? "https://api.anthropic.com/v1/messages" : model->baseUrl);
      request->setUrl(url);

      json anthropicRequest;
      anthropicRequest["model"]      = model->modelIdentifier.toStdString();
      // Claude 3.5 / 3.7 supports up to 8192 output tokens; older models support 4096.
      anthropicRequest["max_tokens"] = 8192;
      anthropicRequest["stream"]     = true;
      if (!tools.empty())
            anthropicRequest["tools"] = tools;

      if (extendedThinking) {
            // Budget must be strictly less than max_tokens (8192).
            anthropicRequest["thinking"] = {
                     {"type",          "enabled"},
                     {"budget_tokens",      5000}
                  };
            }

      // Reset token counters for this new request
      _inputTokens  = 0;
      _outputTokens = 0;

      json anthropicMessages = json::array();

      anthropicRequest["system"] = agent->getManifest();

      for (const auto& item : agent->historyManager->getActiveEntries()) {
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
                        // Preserve thinking block in round-trip if present (Extended Thinking).
                        if (item.contains("thinking") && !item["thinking"].get<std::string>().empty()) {
                              json thinkingBlock;
                              thinkingBlock["type"]     = "thinking";
                              thinkingBlock["thinking"] = item["thinking"];
                              contentArray.push_back(thinkingBlock);
                              }
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
                  else {
                        // Plain text or thinking-only assistant turn.
                        // Build a content array so we can include the thinking block if present.
                        json contentArray = json::array();
                        if (item.contains("thinking") && !item["thinking"].get<std::string>().empty()) {
                              json thinkingBlock;
                              thinkingBlock["type"]     = "thinking";
                              thinkingBlock["thinking"] = item["thinking"];
                              contentArray.push_back(thinkingBlock);
                              }
                        if (item.contains("content")) {
                              std::string text;
                              if (item["content"].is_string()) {
                                    text = item["content"].get<std::string>();
                                    }
                              else if (item["content"].is_array()) {
                                    // Flatten array of strings into a single string.
                                    for (const auto& part : item["content"])
                                          if (part.is_string())
                                                text += part.get<std::string>();
                                    }
                              if (!text.empty()) {
                                    json textBlock;
                                    textBlock["type"] = "text";
                                    textBlock["text"] = text;
                                    contentArray.push_back(textBlock);
                                    }
                              }
                        if (!contentArray.empty()) {
                              json converted;
                              converted["role"]    = "assistant";
                              converted["content"] = contentArray;
                              anthropicMessages.push_back(converted);
                              }
                        else {
                              anthropicMessages.push_back(item);
                              }
                        }
                  }
            else {
                  // User role: Anthropic accepts either a plain string or a content-block array.
                  // If the stored content is an array of strings (legacy), flatten it.
                  // If it's already a proper content-block array, pass it through unchanged.
                  json cleaned = item;
                  if (cleaned.contains("content") && cleaned["content"].is_array()) {
                        const auto& arr = cleaned["content"];
                        // Check if all elements are plain strings – then flatten.
                        bool allStrings = std::all_of(arr.begin(), arr.end(),
                                                      [](const json& p) { return p.is_string(); });
                        if (allStrings) {
                              std::string s;
                              for (const auto& part : arr)
                                    s += part.get<std::string>();
                              cleaned["content"] = s;
                              }
                        // Otherwise keep the structured array as-is (e.g. text+image blocks).
                        }
                  anthropicMessages.push_back(cleaned);
                  }
            }

      anthropicRequest["messages"] = anthropicMessages;
      currentContent.clear();
      currentThinking.clear();
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

      const std::string type = item["type"];

      // ── Token accounting ────────────────────────────────────────────────────
      // message_start carries the input token count for the whole request.
      if (type == "message_start") {
            if (item.contains("message") && item["message"].contains("usage"))
                  _inputTokens = item["message"]["usage"].value("input_tokens", size_t{0});
            return;
            }

      // message_delta carries the cumulative output token count.
      if (type == "message_delta") {
            if (item.contains("usage"))
                  _outputTokens = item["usage"].value("output_tokens", size_t{0});
            return;
            }

      // message_stop – nothing to do, dataFinished() is called by the network layer.
      if (type == "message_stop")
            return;

      // ── Content block handling ───────────────────────────────────────────────
      if (type == "content_block_start") {
            if (!item.contains("content_block"))
                  return;
            const auto& block     = item["content_block"];
            const std::string btype = block.value("type", "");

            if (btype == "tool_use") {
                  json toolCall;
                  toolCall["id"]   = block.value("id", "");
                  toolCall["type"] = "tool_use";
                  toolCall["function"] = {
                           {     "name",  block.value("name", "")},
                           {"arguments", json::object()}
                        };
                  toolCall["arguments_str"] = "";
                  _currentToolCalls.push_back(toolCall);
                  }
            // "text" and "thinking" block_start events don't require special setup.
            return;
            }

      if (type == "content_block_stop")
            return; // no state change needed

      if (type == "content_block_delta") {
            if (!item.contains("delta"))
                  return;
            const auto& delta     = item["delta"];
            const std::string dtype = delta.value("type", "");

            if (dtype == "text_delta") {
                  std::string text = delta.value("text", "");
                  agent->chatDisplay->handleIncomingChunk("", text);
                  currentContent += text;
                  }
            else if (dtype == "thinking_delta") {
                  // Extended Thinking: stream thought text to the display.
                  std::string thought = delta.value("thinking", "");
                  agent->chatDisplay->handleIncomingChunk(thought, "");
                  currentThinking += thought;
                  }
            else if (dtype == "input_json_delta") {
                  // Accumulate streamed JSON fragments for the current tool call.
                  if (!_currentToolCalls.empty() && delta.contains("partial_json")) {
                        auto& currentCall         = _currentToolCalls.back();
                        currentCall["arguments_str"] =
                            currentCall["arguments_str"].get<std::string>() + delta["partial_json"].get<std::string>();
                        }
                  }
            }
      }

//---------------------------------------------------------
//   processTools
//    receives the already-resolved tool calls (arguments already parsed as JSON
//    objects) so that this method is independent of the _currentToolCalls member
//    state, which has already been cleared by dataFinished().
//---------------------------------------------------------

void AnthropicClient::processTools(json resolvedToolCalls) {
      try {
            for (auto& call : resolvedToolCalls) {
                  std::string functionName = call["function"]["name"];

                  // Arguments have been parsed from arguments_str by dataFinished()
                  // and are stored in call["function"]["arguments"] as a JSON object.
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

      agent->sendMessage2();
      }

//---------------------------------------------------------
//   dataFinished
//---------------------------------------------------------

void AnthropicClient::dataFinished() {
      json responseContent;
      responseContent["role"]    = "assistant";
      responseContent["content"] = currentContent;
      // Persist thinking content so it can be displayed when reloading a session.
      if (!currentThinking.empty())
            responseContent["thinking"] = currentThinking;

      // Parse arguments_str → JSON object and build the standard tool_calls array
      // that prompt() can later convert back to the Anthropic wire format.
      json resolvedToolCalls = json::array();
      for (auto& call : _currentToolCalls) {
            if (call.contains("arguments_str")) {
                  std::string argsStr = call["arguments_str"].get<std::string>();
                  try {
                        call["function"]["arguments"] = argsStr.empty() ? json::object() : json::parse(argsStr);
                        }
                  catch (const json::parse_error& e) {
                        Critical("Failed to parse tool arguments: {}", e.what());
                        call["function"]["arguments"] = json::object();
                        }
                  call.erase("arguments_str");
                  }
            // Ensure "type" field is present for round-trip conversion in prompt()
            if (!call.contains("type"))
                  call["type"] = "tool_use";
            resolvedToolCalls.push_back(call);
            }

      currentContent.clear();
      currentThinking.clear();
      _currentToolCalls.clear();

      // Use real token counts reported by the Anthropic API.
      const size_t totalTokens = _inputTokens + _outputTokens;

      if (resolvedToolCalls.empty()) {
            // Plain text response — let the history manager decide whether a summary is needed.
            if (agent->historyManager->addResult(responseContent, totalTokens))
                  agent->sendMessage2();
            else
                  agent->enableInput(true);
            }
      else {
            // Store the assistant turn (with tool_calls) and immediately execute the tools.
            // processTools() receives the fully resolved list by value so it is independent
            // of _currentToolCalls, which has already been cleared above.
            responseContent["tool_calls"] = resolvedToolCalls;
            agent->historyManager->addRequest(responseContent, totalTokens);
            processTools(std::move(resolvedToolCalls));
            }
      }
