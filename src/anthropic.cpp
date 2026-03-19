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
      // Enable Extended Thinking when the model flag is set.
      // The beta header is required; thinking budget must be strictly < max_tokens.
      const bool extendedThinking = model->supportsThinking;
      if (extendedThinking)
            request->setRawHeader("anthropic-beta", "interleaved-thinking-2025-05-14");

      QUrl url(model->baseUrl.isEmpty() ? "https://api.anthropic.com/v1/messages" : model->baseUrl);
      request->setUrl(url);

      json anthropicRequest;
      anthropicRequest["model"] = model->modelIdentifier.toStdString();
      // max_tokens: use model setting if configured, otherwise 8192 (supports claude-3.5+, claude-3.7+).
      // Older claude-3 models cap at 4096 – set model->maxTokens accordingly in the config.
      const int maxTokens            = (model->maxTokens > 0) ? model->maxTokens : 8192;
      anthropicRequest["max_tokens"] = maxTokens;
      anthropicRequest["stream"]     = true;
      if (!tools.empty())
            anthropicRequest["tools"] = tools;

      // Optional sampling parameters – only set when explicitly configured.
      if (model->temperature >= 0.0)
            anthropicRequest["temperature"] = model->temperature;
      if (model->topP >= 0.0)
            anthropicRequest["top_p"] = model->topP;

      if (extendedThinking) {
            // Budget must be strictly less than max_tokens.
            const int thinkingBudget     = std::max(1024, maxTokens - 1000);
            anthropicRequest["thinking"] = {
                     {         "type",      "enabled"},
                     {"budget_tokens", thinkingBudget}
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
                  json contentArray = json::array();

                  // ── Thinking block (Extended Thinking round-trip) ────────────────
                  // The API requires the full thinking object including its "signature"
                  // field to be sent back verbatim.  We store it as a JSON object so
                  // nothing is lost between turns.
                  if (item.contains("thinking")) {
                        const auto& th = item["thinking"];
                        if (th.is_object()) {
                              // Stored as full block {type, thinking, signature} – pass through.
                              contentArray.push_back(th);
                              }
                        else if (th.is_string() && !th.get<std::string>().empty()) {
                              // Legacy: stored as plain string (no signature available).
                              json thinkingBlock;
                              thinkingBlock["type"]     = "thinking";
                              thinkingBlock["thinking"] = th.get<std::string>();
                              contentArray.push_back(thinkingBlock);
                              }
                        }

                  // ── Text block ───────────────────────────────────────────────────
                  if (item.contains("content")) {
                        std::string text;
                        if (item["content"].is_string()) {
                              text = item["content"].get<std::string>();
                              }
                        else if (item["content"].is_array()) {
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

                  // ── Tool-use blocks ──────────────────────────────────────────────
                  if (item.contains("tool_calls") && item["tool_calls"].is_array()) {
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
            else {
                  // User role: Anthropic accepts either a plain string or a content-block array.
                  // If the stored content is an array of strings (legacy), flatten it.
                  // If it's already a proper content-block array, pass it through unchanged.
                  json cleaned = item;
                  if (cleaned.contains("content") && cleaned["content"].is_array()) {
                        const auto& arr = cleaned["content"];
                        // Check if all elements are plain strings – then flatten.
                        bool allStrings = std::all_of(arr.begin(), arr.end(), [](const json& p) { return p.is_string(); });
                        if (allStrings) {
                              std::string s;
                              for (const auto& part : arr)
                                    s += part.get<std::string>();
                              cleaned["content"] = s;
                              }
                        // Otherwise keep the structured array as-is (e.g. text+image blocks).
                        }

                  // Embed screenshot if present as an Anthropic image content block
                  if (cleaned.contains("image")) {
                        std::string b64 = cleaned["image"].get<std::string>();
                        cleaned.erase("image");

                        // Build a content block array: image block first, then text
                        json contentArray = json::array();
                        contentArray.push_back({
                                 {"type", "image"},
                                 {
                                  "source", {
                                           {      "type",  "base64"},
                                           {"media_type", "image/jpeg"},
                                           {      "data",          b64}
                                        }
                                 }
                              });

                        // Extract existing text content (may be string or array)
                        std::string textContent;
                        if (cleaned.contains("content")) {
                              if (cleaned["content"].is_string())
                                    textContent = cleaned["content"].get<std::string>();
                              else if (cleaned["content"].is_array()) {
                                    for (const auto& p : cleaned["content"])
                                          if (p.is_string())
                                                textContent += p.get<std::string>();
                                    }
                              }
                        contentArray.push_back({{"type", "text"}, {"text", textContent}});
                        cleaned["content"] = contentArray;
                        }

                  anthropicMessages.push_back(cleaned);
                  }
            }

      anthropicRequest["messages"] = anthropicMessages;
      currentContent.clear();
      currentThinkingBlock = json::object();
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
            const auto& block       = item["content_block"];
            const std::string btype = block.value("type", "");

            if (btype == "tool_use") {
                  json toolCall;
                  toolCall["id"]       = block.value("id", "");
                  toolCall["type"]     = "tool_use";
                  toolCall["function"] = {
                           {"name", block.value("name", "")},
                           {"arguments", json::object()}
                        };
                  toolCall["arguments_str"] = "";
                  _currentToolCalls.push_back(toolCall);
                  }
            else if (btype == "thinking") {
                  // Start a fresh thinking block; signature arrives via signature_delta.
                  currentThinkingBlock              = json::object();
                  currentThinkingBlock["type"]      = "thinking";
                  currentThinkingBlock["thinking"]  = "";
                  currentThinkingBlock["signature"] = "";
                  }
            return;
            }

      if (type == "content_block_stop")
            return; // no state change needed

      if (type == "content_block_delta") {
            if (!item.contains("delta"))
                  return;
            const auto& delta       = item["delta"];
            const std::string dtype = delta.value("type", "");

            if (dtype == "text_delta") {
                  std::string text = delta.value("text", "");
                  agent->chatDisplay->handleIncomingChunk("", text);
                  currentContent += text;
                  }
            else if (dtype == "thinking_delta") {
                  // Extended Thinking: stream thought text; accumulate into block object.
                  std::string thought = delta.value("thinking", "");
                  agent->chatDisplay->handleIncomingChunk(thought, "");
                  currentThinkingBlock["thinking"] = currentThinkingBlock["thinking"].get<std::string>() + thought;
                  }
            else if (dtype == "signature_delta") {
                  // The API streams the cryptographic signature of the thinking block.
                  // It must be sent back verbatim in subsequent turns.
                  currentThinkingBlock["signature"] = currentThinkingBlock["signature"].get<std::string>() + delta.value("signature", "");
                  }
            else if (dtype == "input_json_delta") {
                  // Accumulate streamed JSON fragments for the current tool call.
                  if (!_currentToolCalls.empty() && delta.contains("partial_json")) {
                        auto& currentCall = _currentToolCalls.back();
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
      // Persist the full thinking block (including signature) for correct round-trip.
      // The block is stored as a JSON object so prompt() can pass it back verbatim.
      if (!currentThinkingBlock.empty() && !currentThinkingBlock.value("thinking", "").empty())
            responseContent["thinking"] = currentThinkingBlock;

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
      currentThinkingBlock = json::object();
      _currentToolCalls.clear();

      // Use real token counts reported by the Anthropic API.
      const size_t totalTokens = _inputTokens + _outputTokens;

      if (resolvedToolCalls.empty()) {
            // Plain text response — let the history manager decide whether a summary is needed.
            if (agent->historyManager->addResult(responseContent, totalTokens)) {
                  // request summary
                  std::string text = "Please provide a concise technical summary of our conversation so far. "
                               "Focus specifically on the results obtained from the tool calls and the final "
                               "conclusions reached. Discard the raw, voluminous data output from the tools, "
                               "but retain the key facts, parameters used, and the current state of the task. "
                               "This summary will serve as the new starting point for our context, "
                               "so ensure no critical logical step is lost.";

                  json msg;
                  msg["role"] = "user";
                  msg["content"] = text;
                  agent->historyManager->addRequest(msg, text.length() / 4);
                  agent->sendMessage2();
                  }
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
