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
#include "session.h"

//---------------------------------------------------------
//   OpenAiClient
//---------------------------------------------------------

OpenAiClient::OpenAiClient(Agent* a, Model* m, const std::vector<json>& mcps) : LLMClient(a, m) {
      setTools(mcps);
      }

void OpenAiClient::setTools(const std::vector<json>& mcps) {
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
      }

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
      requestJson["stream"] = model->stream;
      if (!tools.empty())
            requestJson["tools"] = tools;

      json history = json::array();

      // system message
      json jmanifest;
      jmanifest["content"] = agent->getManifest();
      jmanifest["role"]    = "system";
      history.push_back(jmanifest);

      for (const auto& msg : agent->session()->getActiveEntries()) {
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

            // Embed screenshot/images: OpenAI Vision uses content block array with image_url
            if (jmsg.value("role", "") == "user" && msg.contains("images") && msg["images"].is_array() &&
                !msg["images"].empty()) {
                  json contentArray = json::array();
                  for (const auto& img : msg["images"]) {
                        std::string dataUri = "data:image/jpeg;base64," + img.get<std::string>();
                        contentArray.push_back({
                                 {     "type",        "image_url"},
                                 {"image_url", {{"url", dataUri}}}
                              });
                        }

                  std::string textContent;
                  if (msg.contains("content")) {
                        if (msg["content"].is_string())
                              textContent = msg["content"].get<std::string>();
                        }
                  contentArray.push_back({
                           {"type",      "text"},
                           {"text", textContent}
                        });
                  jmsg["content"] = contentArray;
                  }
            else if (msg.contains("image")) {
                  std::string dataUri = "data:image/jpeg;base64," + msg["image"].get<std::string>();
                  json contentArray   = json::array();
                  contentArray.push_back({
                           {     "type",        "image_url"},
                           {"image_url", {{"url", dataUri}}}
                        });

                  std::string textContent;
                  if (msg.contains("content")) {
                        if (msg["content"].is_string())
                              textContent = msg["content"].get<std::string>();
                        }
                  contentArray.push_back({
                           {"type",      "text"},
                           {"text", textContent}
                        });
                  jmsg["content"] = contentArray;
                  }

            if (jmsg.value("role", "") != "user" && msg.contains("images") && msg["images"].is_array() &&
                !msg["images"].empty()) {
                  // Find the last user message in history
                  for (auto it = history.rbegin(); it != history.rend(); ++it) {
                        if ((*it).contains("role") && (*it)["role"] == "user") {
                              json& userMsg     = *it;
                              json contentArray = json::array();
                              if (userMsg.contains("content")) {
                                    if (userMsg["content"].is_string()) {
                                          contentArray.push_back({
                                                   {"type",                                "text"},
                                                   {"text", userMsg["content"].get<std::string>()}
                                                });
                                          }
                                    else if (userMsg["content"].is_array()) {
                                          contentArray = userMsg["content"];
                                          }
                                    }
                              for (const auto& img : msg["images"]) {
                                    std::string dataUri = "data:image/jpeg;base64," + img.get<std::string>();
                                    contentArray.push_back({
                                             {     "type",        "image_url"},
                                             {"image_url", {{"url", dataUri}}}
                                          });
                                    }
                              userMsg["content"] = contentArray;
                              break;
                              }
                        }
                  }

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
                  agent->chatDisplay->handleIncomingChunk("", s);
                  currentContent += s;
                  }
            }

      if (delta.contains("tool_calls")) {
            for (const auto& tc : delta["tool_calls"]) {
                  if (!tc.contains("index") || !tc["index"].is_number_integer()) {
                        Critical("OpenAI tool_call missing integer <index>");
                        continue;
                        }
                  int index = tc["index"].get<int>();
                  // Ensure array is large enough
                  while (_currentToolCalls.size() <= static_cast<size_t>(index))
                        _currentToolCalls.push_back(json::object());

                  auto& currentCall = _currentToolCalls[index];

                  if (tc.contains("id") && tc["id"].is_string())
                        currentCall["id"] = tc["id"];
                  if (tc.contains("type") && tc["type"].is_string())
                        currentCall["type"] = tc["type"];

                  if (tc.contains("function")) {
                        if (!currentCall.contains("function"))
                              currentCall["function"] = json::object();

                        const auto& func = tc["function"];
                        if (func.contains("name") && func["name"].is_string())
                              currentCall["function"]["name"] = func["name"];
                        if (func.contains("arguments")) {
                              if (!currentCall["function"].contains("arguments_str") ||
                                  !currentCall["function"]["arguments_str"].is_string())
                                    currentCall["function"]["arguments_str"] = "";
                              if (func["arguments"].is_string())
                                    currentCall["function"]["arguments_str"] =
                                        currentCall["function"]["arguments_str"].get<std::string>() +
                                        func["arguments"].get<std::string>();
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
                  json fc = call["function"];
                  if (!fc.is_object() || !fc.contains("name") || !fc["name"].is_string()) {
                        Critical("ToolCall function does not contain valid <name>");
                        continue;
                        }

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
                  std::string functionName = fc["name"].get<std::string>();

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

                  json msg;
                  msg["role"]    = "tool";
                  msg["content"] = result;
                  msg["name"]    = functionName;
                  if (call.contains("id"))
                        msg["tool_call_id"] = call["id"];
                  msg["function"] = fc; // For logContent

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

                  agent->session()->addRequest(msg, 0);

                  // Check if the user pressed stop while the tool was running.
                  if (agent->isToolStopped()) {
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

      // Let the session estimate per-message tokens (0 -> estimateTokens()).
      if (_currentToolCalls.empty()) {
            agent->session()->setToolLoopActive(false);
            agent->session()->addResult(responseContent, 0);
            agent->stopAgent();
            }
      else {
            agent->session()->setToolLoopActive(true);
            agent->session()->addRequest(responseContent, 0);
            processTools();
            }
      }
