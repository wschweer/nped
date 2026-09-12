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
#include "session.h"

static const int maxThinkingBudget = 1024;

//---------------------------------------------------------
//   AnthropicClient
//---------------------------------------------------------

AnthropicClient::AnthropicClient(Agent* a, Model* m, const std::vector<json>& mcps) : LLMClient(a, m) {
      setTools(mcps);
      }

//---------------------------------------------------------
//   setTools
//---------------------------------------------------------

void AnthropicClient::setTools(const std::vector<json>& mcps) {
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
      // Enable Extended Thinking when the provider-native "thinking" block is
      // present in the model configuration, e.g.
      //   {"thinking": {"type": "enabled", "budget_tokens": 4096}}
      // The beta header is required; thinking budget must be strictly < max_tokens.
      const json cfg            = model->configJson();
      const bool extendedThinking = cfg.contains("thinking");
      if (extendedThinking) {
            // Claude 4+ models support thinking as GA; beta header only needed for 3.x
            const std::string mid = model->modelIdentifier.toStdString();
            if (mid.find("claude-3") != std::string::npos)
                  request->setRawHeader("anthropic-beta", "interleaved-thinking-2025-05-14");
            }

      QUrl url(model->baseUrl.isEmpty() ? "https://api.anthropic.com/v1/messages" : model->baseUrl);
      request->setUrl(url);

      json anthropicRequest;
      anthropicRequest["model"] = model->modelIdentifier.toStdString();
      // max_tokens: use model setting if configured, otherwise 8192 (supports claude-3.5+, claude-3.7+).
      // Older claude-3 models cap at 4096 – set model->maxTokens accordingly in the config.
      const int maxTokens = (model->maxTokens > 0) ? model->maxTokens : (extendedThinking ? 16384 : 8192);

      anthropicRequest["max_tokens"] = maxTokens;
      anthropicRequest["stream"]     = model->stream;
      if (!tools.empty())
            anthropicRequest["tools"] = tools;

      if (extendedThinking) {
            // Extended Thinking: temperature/top_p/top_k are forbidden by the API.
            // Pass the configured block through verbatim when given; otherwise
            // derive a budget that is >= 1024 and strictly < max_tokens.
            if (cfg["thinking"].is_object())
                  anthropicRequest["thinking"] = cfg["thinking"];
            else {
                  const int thinkingBudget = std::max(1024, maxTokens - 1000);
                  anthropicRequest["thinking"] = {
                           {         "type",      "enabled"},
                           {"budget_tokens", thinkingBudget}
                        };
                  }
            }
      else {
            // Optional sampling parameters – only without thinking.
            if (cfg.contains("temperature") && cfg["temperature"].is_number() && cfg["temperature"] >= 0.0)
                  anthropicRequest["temperature"] = cfg["temperature"];
            if (cfg.contains("top_p") && cfg["top_p"].is_number() && cfg["top_p"] >= 0.0)
                  anthropicRequest["top_p"] = cfg["top_p"];
            }

      // Reset token counters for this new request
      _inputTokens  = 0;
      _outputTokens = 0;

      json anthropicMessages = json::array();

      anthropicRequest["system"] = agent->getManifest();

      // ── Strategy: Strip thinking blocks from all but the very last assistant turn.
      // Anthropic only requires the thinking block (with its signature) from the
      // immediately preceding assistant message.  Keeping older thinking blocks
      // wastes thousands of tokens per turn without any benefit.
      const json activeHistory        = agent->session()->getActiveEntries();
      size_t lastThinkingAssistantIdx = SIZE_MAX;
      for (size_t i = 0; i < activeHistory.size(); ++i) {
            const auto& h = activeHistory[i];
            if (h.value("role", "") == "assistant" && h.contains("thinking"))
                  lastThinkingAssistantIdx = i;
            }

      // We will build anthropicMessages by ensuring consecutive messages of the same role are merged.
      auto addMessage = [&anthropicMessages](const json& newMsg) {
            if (!anthropicMessages.empty() && anthropicMessages.back()["role"] == newMsg["role"]) {
                  // Merge content
                  auto& lastMsg = anthropicMessages.back();

                  // Ensure both contents are arrays to merge them safely
                  json lastContent = lastMsg.contains("content") ? lastMsg["content"] : json::array();
                  if (!lastContent.is_array()) {
                        lastContent = json::array({
                                 {{"type", "text"}, {"text", lastContent}}
                              });
                        }

                  json newContent = newMsg.contains("content") ? newMsg["content"] : json::array();
                  if (!newContent.is_array()) {
                        newContent = json::array({
                                 {{"type", "text"}, {"text", newContent}}
                              });
                        }

                  for (const auto& item : newContent)
                        lastContent.push_back(item);
                  lastMsg["content"] = lastContent;
                  }
            else {
                  anthropicMessages.push_back(newMsg);
                  }
            };

      size_t historyIdx = 0;
      for (const auto& item : activeHistory) {
            const size_t currentIdx = historyIdx++;
            std::string role        = item.value("role", "");

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
                  if (item.contains("images") && item["images"].is_array() && !item["images"].empty()) {
                        json toolContent = json::array();
                        toolContent.push_back({
                                 {"type", "text"},
                                 {"text", item.value("content", "")}
                              });
                        for (const auto& imgItem : item["images"]) {
                              std::string b64 = imgItem.is_string() ? imgItem.get<std::string>() : "";
                              toolContent.push_back({
                                       {  "type",      "image"                                           },
                                       {"source",
                                        {{"type", "base64"}, {"media_type", "image/jpeg"}, {"data", b64}}}
                                    });
                              }
                        toolResult["content"] = toolContent;
                        }
                  else {
                        toolResult["content"] = item.value("content", "");
                        }
                  contentArray.push_back(toolResult);
                  toolMsg["content"] = contentArray;
                  addMessage(toolMsg);
                  }
            else if (role == "assistant") {
                  json contentArray = json::array();

                  // ── Thinking block (Extended Thinking round-trip) ────────────────
                  // The API requires the full thinking object including its "signature"
                  // field to be sent back verbatim.  We store it as a JSON object so
                  // nothing is lost between turns.
                  //
                  // Context-reduction strategy: only round-trip the thinking block for
                  // the very last assistant turn that contains one.  All earlier thinking
                  // blocks are stripped – they can consume 5 000–20 000 tokens each and
                  // the model neither needs nor uses them once the turn has passed.
                  if (item.contains("thinking") && currentIdx == lastThinkingAssistantIdx) {
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
                              toolUse["type"] = "tool_use";
                              toolUse["id"]   = tc.value("id", "");
                              toolUse["name"] =
                                  tc.contains("function") ? tc["function"].value("name", "") : "";
                              toolUse["input"] =
                                  tc.contains("function") && tc["function"].contains("arguments")
                                      ? tc["function"]["arguments"]
                                      : json::object();
                              contentArray.push_back(toolUse);
                              }
                        }

                  if (!contentArray.empty()) {
                        json converted;
                        converted["role"]    = "assistant";
                        converted["content"] = contentArray;
                        addMessage(converted);
                        }
                  else {
                        addMessage(item);
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
                        bool allStrings =
                            std::all_of(arr.begin(), arr.end(), [](const json& p) { return p.is_string(); });
                        if (allStrings) {
                              std::string s;
                              for (const auto& part : arr)
                                    s += part.get<std::string>();
                              cleaned["content"] = s;
                              }
                        // Otherwise keep the structured array as-is (e.g. text+image blocks).
                        }

                  // Embed screenshot/images if present as Anthropic image content blocks
                  if (cleaned.contains("images") && cleaned["images"].is_array() &&
                      !cleaned["images"].empty()) {
                        json contentArray = json::array();
                        for (const auto& imgItem : cleaned["images"]) {
                              std::string b64 = imgItem.is_string() ? imgItem.get<std::string>() : "";
                              contentArray.push_back({
                                       {  "type",      "image"                                           },
                                       {"source",
                                        {{"type", "base64"}, {"media_type", "image/jpeg"}, {"data", b64}}}
                                    });
                              }
                        cleaned.erase("images");

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
                        contentArray.push_back({
                                 {"type",      "text"},
                                 {"text", textContent}
                              });
                        cleaned["content"] = contentArray;
                        }
                  else if (cleaned.contains("image")) {
                        std::string b64 = cleaned["image"].get<std::string>();
                        cleaned.erase("image");

                        // Build a content block array: image block first, then text
                        json contentArray = json::array();
                        contentArray.push_back({
                                 {  "type",                                                           "image"},
                                 {"source", {{"type", "base64"}, {"media_type", "image/jpeg"}, {"data", b64}}}
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
                        contentArray.push_back({
                                 {"type",      "text"},
                                 {"text", textContent}
                              });
                        cleaned["content"] = contentArray;
                        }

                  addMessage(cleaned);
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
                  _inputTokens = item["message"]["usage"].value("input_tokens", size_t {0});
            return;
            }

      // message_delta carries the cumulative output token count.
      if (type == "message_delta") {
            if (item.contains("usage"))
                  _outputTokens = item["usage"].value("output_tokens", size_t {0});
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
                  currentThinkingBlock["thinking"] =
                      currentThinkingBlock["thinking"].get<std::string>() + thought;
                  }
            else if (dtype == "signature_delta") {
                  // The API streams the cryptographic signature of the thinking block.
                  // It must be sent back verbatim in subsequent turns.
                  currentThinkingBlock["signature"] =
                      currentThinkingBlock["signature"].get<std::string>() + delta.value("signature", "");
                  }
            else if (dtype == "input_json_delta") {
                  // Accumulate streamed JSON fragments for the current tool call.
                  if (!_currentToolCalls.empty() && delta.contains("partial_json")) {
                        auto& currentCall            = _currentToolCalls.back();
                        currentCall["arguments_str"] = currentCall["arguments_str"].get<std::string>() +
                                                       delta["partial_json"].get<std::string>();
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
      // Context-reduction strategy: cap individual tool results so that a single
      // large file-read or search result cannot flood the entire context window.
      // 12 000 chars ≈ 3 000 tokens – generous enough for most outputs while still
      // leaving room for conversation and thinking budgets.
      static constexpr size_t maxToolResultChars = 12000;

      try {
            for (auto& call : resolvedToolCalls) {
                  if (!call.contains("function") || !call["function"].is_object() ||
                      !call["function"].contains("name") || !call["function"]["name"].is_string()) {
                        Critical("ToolCall does not contain valid <name>");
                        continue;
                        }
                  std::string functionName = call["function"]["name"].get<std::string>();

                  // Arguments have been parsed from arguments_str by dataFinished()
                  // and are stored in call["function"]["arguments"] as a JSON object.
                  json args =
                      (call["function"].contains("arguments") && call["function"]["arguments"].is_object())
                          ? call["function"]["arguments"]
                          : json::object();

                  std::string result;
                  try {
                        result = agent->executeTool(functionName, args);
                        }
                  catch (const json::type_error& e) {
                        Critical("TypeError in executeTool: {}", e.what());
                        result = std::string("Error: tool failed (type error): ") + e.what();
                        }
                  catch (const json::parse_error& e) {
                        Critical("ParseError in executeTool: {}", e.what());
                        result = std::string("Error: tool failed (parse error): ") + e.what();
                        }
                  catch (const std::exception& e) {
                        Critical("Exception in executeTool: {}", e.what());
                        result = std::string("Error: tool failed: ") + e.what();
                        }
                  catch (...) {
                        Critical("Unknown exception in executeTool");
                        result = std::string("Error: tool failed: unknown exception");
                        }

                  // Truncate oversized tool results before they enter the history.
                  if (result.size() > maxToolResultChars) {
                        result.resize(maxToolResultChars);
                        result +=
                            "\n...[output truncated to " + std::to_string(maxToolResultChars) + " chars]";
                        }

                  json msg;
                  msg["role"]    = "tool";
                  msg["content"] = result;
                  msg["name"]    = functionName;
                  if (call.contains("id"))
                        msg["tool_call_id"] = call["id"];
                  msg["function"] = call["function"];

                  if (functionName == "extract_video_frames") {
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

                  // show on display
                  std::string thinking;
                  std::string text;
                  agent->logContent(msg, text, thinking);
                  agent->chatDisplay->handleIncomingChunk(thinking, text);

                  // Context-reduction: the session estimates the token footprint of
                  // the tool result itself (tokens == 0 -> estimateTokens()).
                  agent->session()->addRequest(msg, 0);

                  // Check if the user pressed stop while the tool was running.
                  if (agent->isToolStopped()) {
                        // Inject a tool result informing the LLM that execution was halted.
                        json stopMsg;
                        stopMsg["role"] = "tool";
                        stopMsg["content"] =
                            "[Tool execution was stopped by the user. Remaining tool calls were skipped.]";
                        stopMsg["name"] = "system";
                        agent->session()->addRequest(stopMsg, 10);
                        break;
                        }
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
                        call["function"]["arguments"] =
                            argsStr.empty() ? json::object() : json::parse(argsStr);
                        }
                  catch (const json::parse_error& e) {
                        Critical("Failed to parse tool arguments: {}", e.what());

                        std::string warning =
                            "\n\n[System Error: The tool call was truncated and could not be parsed. You "
                            "likely hit the max_tokens limit. Please try again, but split your work into "
                            "smaller steps. For example, use insert_lines/remove_lines instead of "
                            "write_file.]";
                        currentContent += warning;
                        agent->chatDisplay->handleIncomingChunk("", warning);

                        // Skip this broken tool call completely so it doesn't pollute the history
                        // or trigger a failed execution with empty arguments.
                        continue;
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

      // The API reports the total context for the request (prompt + completion),
      // not the size of this single message. Keep it for monitoring only and let
      // the session estimate the per-message tokens itself (tokens == 0).
      agent->session()->setReportedContextTokens(_inputTokens + _outputTokens);

      if (resolvedToolCalls.empty()) {
            // Plain text response — finalize the turn and let the history manager trim.
            agent->session()->setToolLoopActive(false);
            agent->session()->addResult(responseContent, 0);
            agent->stopAgent();
            }
      else {
            // Store the assistant turn (with tool_calls) and immediately execute the tools.
            // processTools() receives the fully resolved list by value so it is independent
            // of _currentToolCalls, which has already been cleared above.
            agent->session()->setToolLoopActive(true);
            responseContent["tool_calls"] = resolvedToolCalls;
            agent->session()->addRequest(responseContent, 0);
            processTools(std::move(resolvedToolCalls));
            }
      }
