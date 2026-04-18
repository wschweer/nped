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

#include "ollama.h"
#include "agent.h"
#include "chatdisplay.h"
#include "session.h"

//---------------------------------------------------------
//   OllamaClient
//---------------------------------------------------------

OllamaClient::OllamaClient(Agent* a, Model* m, const std::vector<json>& mcps) : LLMClient(a, m) {
      setTools(mcps);
      }

void OllamaClient::setTools(const std::vector<json>& mcps) {
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
//    prepare prompt for Ollama
//---------------------------------------------------------

json OllamaClient::prompt(QNetworkRequest* request) {
      request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

      QUrl url(model->baseUrl);
      request->setUrl(url);

      json requestJson;
      requestJson["model"]  = model->modelIdentifier.toStdString();
      requestJson["stream"] = model->stream;

      json options;
      if (model->num_ctx > 0)
            options["num_ctx"] = model->num_ctx;
      if (model->num_predict > 0)
            options["num_predict"] = model->num_predict;    // -1 = infinite
      if (model->temperature >= 0.0)
            options["temperature"] = model->temperature;
      if (model->topP >= 0.0)
            options["top_p"] = model->topP;
      if (model->topK >= 0.0)
            options["top_k"] = model->topK;
      if (!options.empty())
            requestJson["options"] = options;

      if (!tools.empty())
            requestJson["tools"] = tools;

      json history = json::array();

      //=========================================
      // add manifest to prompt
      //=========================================

      json jmanifest;
      auto manifest = agent->getManifest();
      // think hack for gemma4
      jmanifest["content"] = model->supportsThinking ? "<|think|> " + manifest : manifest;
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
                  jmsg["tool_name"] = msg["name"];
            if (msg.contains("tool_call_id"))
                  jmsg["tool_call_id"] = msg["tool_call_id"];
            // Embed screenshot: Ollama multimodal uses "images" array with base64 strings
            if (msg.contains("images") && msg["images"].is_array()) {
                  jmsg["images"] = msg["images"];
            } else if (msg.contains("image")) {
                  jmsg["images"] = json::array({msg["image"].get<std::string>()});
            }
            history.push_back(jmsg);
            }
      requestJson["messages"] = history;
      currentContent.clear();
      currentThinking.clear();
      _buffer.clear();
      _currentToolCalls.clear();
      _isThinking = false;
      return requestJson;
      };

//---------------------------------------------------------
//   processJsonItem
//    Hilfsfunktion für die Verarbeitung eines einzelnen JSON-Items
//---------------------------------------------------------

void OllamaClient::processJsonItem(const json& item) {
      if (item.contains("error")) {
            std::string err = item["error"].is_string() ? item["error"].get<std::string>() : item["error"].dump();
            agent->chatDisplay->handleIncomingChunk("", "\n**Error:** " + err + "\n");
            currentContent += "\nError: " + err + "\n";
            return;
            }
      if (!item.contains("message"))
            return;
      const auto& message = item["message"];
      if (!message.is_object())
            return;

      if (message.contains("content") && message["content"].is_string()) {
            std::string s = message["content"].get<std::string>();
            if (!s.empty()) {
                  std::string textChunk;
                  std::string thinkChunk;

                  for (char c : s) {
                        _buffer += c;
                        if (!_isThinking) {
                              if ("<think>" == _buffer || "<thought>" == _buffer || "<|channel>thought" == _buffer) {
                                    _isThinking     = true;
                                    currentContent += _buffer;
                                    _buffer.clear();
                                    }
                              else if (std::string("<think>").find(_buffer) != 0 &&
                                       std::string("<thought>").find(_buffer) != 0 &&
                                       std::string("<|channel>thought").find(_buffer) != 0) {
                                    textChunk      += _buffer;
                                    currentContent += _buffer;
                                    _buffer.clear();
                                    }
                              }
                        else if ("</think>" == _buffer || "</thought>" == _buffer || "<channel|>" == _buffer) {
                              _isThinking     = false;
                              currentContent += _buffer;
                              _buffer.clear();
                              }
                        else if (std::string("</think>").find(_buffer) != 0 &&
                                 std::string("</thought>").find(_buffer) != 0 &&
                                 std::string("<channel|>").find(_buffer) != 0) {
                              thinkChunk      += _buffer;
                              currentContent  += _buffer;
                              currentThinking += _buffer;
                              _buffer.clear();
                              }
                        }

                  if (!thinkChunk.empty() || !textChunk.empty())
                        agent->chatDisplay->handleIncomingChunk(thinkChunk, textChunk);
                  }
            }

      if (message.contains("thinking") && message["thinking"].is_string()) {
            std::string thinkingStr = message["thinking"].get<std::string>();
            if (!thinkingStr.empty()) {
                  currentThinking += thinkingStr;
                  agent->chatDisplay->handleIncomingChunk(thinkingStr, "");
                  }
            }

      if (message.contains("tool_calls") && message["tool_calls"].is_array())
            for (const auto& toolCall : message["tool_calls"])
                  _currentToolCalls.push_back(toolCall);
      }

//---------------------------------------------------------
//   processTools
//    process the functionCall requests from ollama
//---------------------------------------------------------

void OllamaClient::processTools() {
      try {
            for (const auto& call : _currentToolCalls) {
                  if (!call.contains("function")) {
                        Critical("ToolCall does not contain <function>");
                        continue;
                        }
                  json fc   = call["function"];
                  json args = fc["arguments"];
                  if (args.is_string())
                        args = json::parse(args.get<std::string>());
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
                  agent->chatDisplay->handleIncomingChunk(thinking, text);

                  // Don't leak 'function' field to Ollama prompt if it doesn't need it
                  // Wait, prompt() only copies what it needs (role, content, tool_calls, name).
                  // So we can leave it in msg.
                  agent->session()->addRequest(msg, 0);
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

void OllamaClient::dataFinished() {
      if (!_buffer.empty()) {
            if (_isThinking)
                  agent->chatDisplay->handleIncomingChunk(_buffer, "");
            else
                  agent->chatDisplay->handleIncomingChunk("", _buffer);
            currentContent += _buffer;
            _buffer.clear();
            }

      json responseContent;
      responseContent["role"]    = "assistant";
      responseContent["content"] = currentContent;
      if (!currentThinking.empty()) {
            responseContent["thinking"] = currentThinking;
            currentThinking.clear();
            }
      if (!_currentToolCalls.empty())
            responseContent["tool_calls"] = _currentToolCalls;

      currentContent.clear();

      size_t totalTokens = 0;

      if (_currentToolCalls.empty()) {
            agent->session()->addResult(responseContent, totalTokens);
            agent->enableInput(true);
            }
      else {
            agent->session()->addRequest(responseContent, totalTokens);
            processTools();
            }
      }
