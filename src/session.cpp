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

#include <QDir>
#include "session.h"
#include "agent.h"
#include "editor.h"

//---------------------------------------------------------
//   getLastSessionInfo
//    Sucht die aktuellste Datei und liefert Metadaten zurück
//---------------------------------------------------------

SessionInfo Session::sessionInfo() const {
      SessionInfo info;
      QString root = agent->editor()->projectRoot();
      if (root.isEmpty()) { // no session, no session info
            Debug("no editor root");
            return info;
            }

      QString sessionFolder = QDir::cleanPath(root + "/.nped");
      QDir dir(sessionFolder);

      QStringList filters;
      filters << "Session-*.json";
      QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::NoSort);
      QFileInfo latestFileInfo;
      for (const QFileInfo& fileInfo : files) {
            QStringList parts = fileInfo.baseName().split('-');
            if (parts.size() < 5) {
                  Debug("bad filename <{}>", fileInfo.baseName());
                  continue;
                  }

            QDate currentDate;
            // Handle both Old (Session-dd-MM-yyyy-n) and New (Session-yy-MM-dd-n) formats
            currentDate = QDate(parts[1].toInt() + 2000, parts[2].toInt(), parts[3].toInt());
            if (!currentDate.isValid()) {
                  Debug("invalid date in filename <{}>", fileInfo.baseName());
                  continue;
                  }

            int currentNumber = parts[4].toInt();

            if (!info.lastDate.isValid() || currentDate > info.lastDate ||
                (currentDate == info.lastDate && currentNumber > info.lastNumber)) {
                  info.lastDate   = currentDate;
                  info.lastNumber = currentNumber;
                  latestFileInfo  = fileInfo;
                  info.fileName   = fileInfo.absoluteFilePath();
                  }
            }
      return info;
      }

//---------------------------------------------------------
//   loadStatus
//    load last session
//---------------------------------------------------------

void Session::load(const QString& sessionPath) {
      QString fileToLoad = sessionPath;

      if (fileToLoad.isEmpty()) {
            auto info  = sessionInfo();
            fileToLoad = info.fileName;
            }

      if (!fileToLoad.isEmpty()) {
            std::ifstream i(fileToLoad.toStdString());
            if (i.is_open()) {
                  std::string content((std::istreambuf_iterator<char>(i)), std::istreambuf_iterator<char>());
                  // Not a valid full JSON object? Try JSON Lines fallback
                  std::istringstream iss(content);
                  std::string line;
                  json h            = json::array();
                  size_t actEntries = 0;
                  while (std::getline(iss, line)) {
                        if (line.empty())
                              continue;
                        try {
                              json obj = json::parse(line);
                              if (obj.contains("model")) {
                                    std::string modelName = obj["model"];
                                    agent->setCurrentModel(QString::fromStdString(modelName), false);
                                    }
                              if (obj.contains("activeEntries"))
                                    actEntries = obj["activeEntries"];
                              if (obj.contains("role") || obj.contains("parts") || obj.contains("content"))
                                    h.push_back(obj);
                              }
                        catch (...) {
                              }
                        }
                  setHistory(h);
                  // Only override activeEntries when an explicit value was found in the file.
                  // If actEntries == 0 (old session format without activeEntries metadata),
                  // setHistory() has already set activeEntries = data.size(), which is correct.
                  if (actEntries > 0)
                        setActiveEntries(actEntries);
                  savedEntries = data().size();
                  _name        = fileToLoad;
                  agent->updateSessionList();
                  agent->updateChatDisplay(true);
                  return;
                  }
            }
      Debug("no session found: start new one");
      // No last session found -> Start new session
      _name        = sessionName(true);
      savedEntries = 0;
      agent->updateSessionList();
      agent->addMessage("system", "<i>[System: No previous session found. New session started.]</i><br>");
      clear();
      }

//-----------------------------------------------------------------------------
//   save
//    save as json lines, so you cannot call json.dump(xx), only json.dump()
//-----------------------------------------------------------------------------

void Session::save() {
      if (_name.isEmpty() || empty())
            return;

      QString path = QFileInfo(_name).absolutePath();
      QDir dir;
      if (!dir.exists(path)) {
            bool created = dir.mkpath(path);
            if (!created) {
                  Critical("Could not create directory {}", path);
                  return;
                  }
            }

      std::ios_base::openmode mode = (savedEntries == 0) ? std::ios::out : std::ios::app;
      std::ofstream f(_name.toStdString(), mode);

      if (f.is_open()) {
            if (savedEntries == 0) {
                  json header;
                  header["model"]         = agent->currentModel().toStdString();
                  header["activeEntries"] = getActiveEntriesCount();
                  f << header.dump(-1, ' ', false, json::error_handler_t::replace) << "\n";
                  }
            if (savedEntries < _data.size()) {
                  if (savedEntries > 0) {
                        json meta;
                        meta["activeEntries"] = getActiveEntriesCount();
                        f << meta.dump(-1, ' ', false, json::error_handler_t::replace) << "\n";
                        }
                  for (size_t i = savedEntries; i < _data.size(); ++i)
                        f << _data[i].content.dump(-1, ' ', false, json::error_handler_t::replace) << "\n";
                  savedEntries = _data.size();
                  }
            f.close();
            }
      else {
            Critical("Could not open session file for writing: {}", _name);
            }
      }

//---------------------------------------------------------
//   estimateTokens
//    Best-effort token estimate for a single message (~4 chars/token).
//    Handles all message shapes used by the providers: plain "content"
//    strings, Gemini "parts" arrays and assistant "tool_calls".
//---------------------------------------------------------

size_t Session::estimateTokens(const json& content) {
      size_t chars = 0;
      try {
            if (content.contains("content")) {
                  if (content["content"].is_string())
                        chars += content["content"].get<std::string>().length();
                  else if (!content["content"].is_null())
                        chars += content["content"].dump(-1, ' ', false,
                                     json::error_handler_t::replace).length();
                  }
            if (content.contains("parts") && content["parts"].is_array()) {
                  for (const auto& part : content["parts"]) {
                        if (part.contains("text") && part["text"].is_string())
                              chars += part["text"].get<std::string>().length();
                        if (part.contains("functionResponse"))
                              chars += part["functionResponse"].dump(-1, ' ', false,
                                           json::error_handler_t::replace).length();
                        }
                  }
            if (content.contains("tool_calls") && content["tool_calls"].is_array())
                  chars += content["tool_calls"].dump(-1, ' ', false,
                               json::error_handler_t::replace).length();
            if (content.contains("images") && content["images"].is_array())
                  chars += content["images"].size() * 4000; // crude: ~1k tokens per image
            }
      catch (...) {
            }
      return chars / 4;
      }

//---------------------------------------------------------
//   recalcTokens
//    Recompute totalTokens from the currently active window
//    without emitting a signal (callers batch updates).
//---------------------------------------------------------

void Session::recalcTokens() {
      totalTokens     = 0;
      size_t startIdx = _data.size() - activeEntries;

      // The very first message is always kept in the payload when the window
      // is truncated (see getActiveEntries), so it must be part of the budget.
      if (startIdx > 0 && !_data.empty())
            totalTokens += _data[0].tokens;

      for (size_t i = startIdx; i < _data.size(); ++i)
            totalTokens += _data[i].tokens;
      }

//---------------------------------------------------------
//   discoveredContextWindow
//    Return the context window of the current model as
//    discovered from the provider (Ollama /api/tags, /api/show,
//    /api/ps).  The runtime value (num_ctx actually in use) is
//    preferred, otherwise the trained context length.  Returns 0
//    if no provider metadata is available (e.g. for cloud
//    providers configured manually).
//---------------------------------------------------------

size_t Session::discoveredContextWindow() const {
      const LlmInfo info = agent->currentLlmInfo();
      if (info.runtimeContextLength)
            return info.runtimeContextLength;

      // A num_ctx explicitly configured for an Ollama model is what the user
      // sends to the provider, so it takes precedence over the (possibly much
      // larger) trained context length until /api/ps confirms the runtime value.
      const Model& m = agent->currentModelObj();
      json cfg       = m.configJson();
      if (cfg.contains("num_ctx") && cfg["num_ctx"].is_number_integer())
            return cfg["num_ctx"].get<size_t>();

      return info.trainedContextLength;
      }

//---------------------------------------------------------
//   contextBudget
//    Token budget for the active window, derived from the current
//    model's context window. Trigger trimming at ~75 % of it to leave
//    room for the response and to stay below the hard provider limit.
//
//    Precedence:
//      1. context window discovered from the provider (LlmInfo:
//         runtime num_ctx, configured num_ctx, trained size)
//      2. configuration:"contextWindow" (explicit user override)
//      3. defaultTokenBudget fallback
//---------------------------------------------------------

size_t Session::contextBudget() const {
      size_t window = discoveredContextWindow();

      if (window == 0) {
            const Model& m = agent->currentModelObj();
            json cfg       = m.configJson();
            if (cfg.contains("contextWindow") && cfg["contextWindow"].is_number_integer())
                  window = cfg["contextWindow"].get<size_t>();
            }

      if (window == 0)
            return defaultTokenBudget; // no context window known

      return window / 100 * budgetRatioPercent;
      }

//---------------------------------------------------------
//   optimizeToolResponses
//---------------------------------------------------------

void Session::optimizeToolResponses() {
      int toolAge = 0;

      for (auto it = _data.rbegin(); it != _data.rend(); ++it) {
            std::string role = it->content.value("role", "");
            if (role == "tool" || role == "function") {
                  int maxLength = 0;
                  if (toolAge == 0)
                        maxLength = 4000 * 2;
                  else if (toolAge <= 2)
                        maxLength = 1000 * 2;
                  else if (toolAge <= 5)
                        maxLength = 250 * 2;
                  else
                        maxLength = 100 * 2;

                  if (it->content.contains("content") && it->content["content"].is_string()) {
                        std::string content = it->content["content"].get<std::string>();
                        if (content.length() > static_cast<size_t>(maxLength)) {
                              size_t keepHead       = static_cast<size_t>(maxLength * 0.4);
                              size_t keepTail       = static_cast<size_t>(maxLength * 0.6);
                              size_t truncatedChars = content.length() - keepHead - keepTail;

                              std::string marker =
                                  "\n\n... [" + std::to_string(truncatedChars) + " chars truncated] ...\n\n";
                              std::string newContent = content.substr(0, keepHead) + marker +
                                                       content.substr(content.length() - keepTail);

                              it->content["content"] = newContent;
                              it->tokens             = newContent.length() / 4;
                              }
                        }
                  toolAge++;
                  }
            }

      recalcTokens();
      }

//---------------------------------------------------------------------------------------
//   trim
//---------------------------------------------------------------------------------------

void Session::trim() {
      // Reflect the message that was just appended before deciding anything.
      recalcTokens();

      // Cache the budget once — contextBudget() parses the model configuration.
      const size_t budget = contextBudget();

      const bool overBudget = effectiveTokens() > budget;
      const bool overCount  = activeEntries > maxEntries;
      if (!overBudget && !overCount) {
            emit tokensChanged(totalTokens);
            return;
            }

      // While a tool loop is running we must not drop messages (the assistant's
      // tool_calls and their tool results form an inseparable unit), but we must
      // still defend the token budget. Only shrink oversized tool outputs.
      if (_toolLoopActive) {
            optimizeToolResponses(); // recalculates tokens internally
            // We have acted; wait for a fresh provider report before trusting the
            // (now stale) reported context again.
            lastReportedContextTokens = 0;
            emit tokensChanged(totalTokens);
            return;
            }

      // Cheap reductions first: collapse repeated tool calls, then shrink old
      // tool outputs. Only worth doing when the budget is actually exceeded.
      if (overBudget) {
            // 0. Drop redundant repeated tool calls
            std::map<std::string, std::string> toolSignatures;
            for (const auto& item : _data) {
                  if (item.content.value("role", "") == "assistant" &&
                      item.content.contains("tool_calls")) {
                        for (const auto& tc : item.content["tool_calls"]) {
                              if (tc.contains("id") && tc.contains("function")) {
                                    std::string id  = tc["id"].get<std::string>();
                                    auto func       = tc["function"];
                                    std::string sig = func.value("name", "") + ":";
                                    if (func.contains("arguments")) {
                                          if (func["arguments"].is_string())
                                                sig += func["arguments"].get<std::string>();
                                          else
                                                sig += func["arguments"].dump(
                                                    -1, ' ', false, json::error_handler_t::replace);
                                          }
                                    toolSignatures[id] = sig;
                                    }
                              }
                        }
                  }

            std::set<std::string> seenSignatures;
            for (auto it = _data.rbegin(); it != _data.rend(); ++it) {
                  std::string role = it->content.value("role", "");
                  if (role == "tool" || role == "function") {
                        std::string id = it->content.value("tool_call_id", "");
                        if (!id.empty() && toolSignatures.count(id)) {
                              std::string sig = toolSignatures[id];
                              if (seenSignatures.count(sig)) {
                                    if (it->content.contains("content") &&
                                        it->content["content"].is_string()) {
                                          std::string oldContent =
                                              it->content["content"].get<std::string>();
                                          std::string newContent =
                                              "[Repeated tool call omitted to save context]";
                                          if (oldContent != newContent) {
                                                it->content["content"] = newContent;
                                                it->tokens             = newContent.length() / 4;
                                                }
                                          }
                                    }
                              else {
                                    seenSignatures.insert(sig);
                                    }
                              }
                        }
                  }

            optimizeToolResponses();
            recalcTokens();

            // We have acted on the estimate; the reported context is stale now.
            lastReportedContextTokens = 0;
            }

      // Rolling window: shrink the active window until both the message cap and
      // the token budget are satisfied, staying above minEntries and always
      // cutting at a "user" turn boundary so turns stay coherent.
      size_t n = activeEntries;
      while ((activeEntries > maxEntries || effectiveTokens() > budget) &&
             activeEntries > minEntries) {
            activeEntries--;
            while (activeEntries > minEntries) {
                  size_t idx    = _data.size() - activeEntries;
                  std::string r = _data[idx].content.value("role", "");
                  if (r == "user")
                        break;
                  activeEntries--;
                  }
            recalcTokens();
            // Once we have trimmed, our own estimate is authoritative until the
            // provider reports the new (smaller) context.
            lastReportedContextTokens = 0;
            }

      setActiveEntries(activeEntries); // recompute + single tokensChanged signal

      if (n != activeEntries)
            Debug("****Reduced History from {} to {} entries ({} tokens)", n, activeEntries,
                totalTokens);
      }

//---------------------------------------------------------
//   addResult
//---------------------------------------------------------

void Session::addResult(const json& content, size_t tokens) {
      if (tokens == 0)
            tokens = estimateTokens(content);
      _data.push_back({content, tokens});
      activeEntries++;
      trim(); // recomputes totalTokens, shrinks tool outputs and/or the window
      }

//---------------------------------------------------------
//   setHistory
//---------------------------------------------------------

void Session::setHistory(const json& h) {
      clear();
      for (const auto& item : h) {
            size_t tokens = estimateTokens(item);
            _data.push_back({item, tokens});
            }
      activeEntries = _data.size();
      recalcTokens();
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   setActiveEntries
//---------------------------------------------------------

void Session::setActiveEntries(size_t a) {
      activeEntries = std::min(a, _data.size());
      recalcTokens();
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   getActiveEntries
//---------------------------------------------------------

json Session::getActiveEntries() const {
      json arr        = json::array();
      size_t startIdx = _data.size() > activeEntries ? _data.size() - activeEntries : 0;

      // Always include the very first request if history is truncated — but only
      // when it is a "user" message. Injecting a tool result at the head would
      // produce an invalid conversation for Anthropic/OpenAI (tool result without
      // a preceding tool_use).
      if (startIdx > 0 && !_data.empty() &&
          _data[0].content.value("role", "") == "user")
            arr.push_back(_data[0].content);

      for (size_t i = startIdx; i < _data.size(); ++i)
            arr.push_back(_data[i].content);

      int maxImagesInContext = 2;
      int imagesFound        = 0;

      for (int i = arr.size() - 1; i >= 0; --i) {
            bool hasImage = false;

            if (arr[i].contains("images") && !arr[i]["images"].empty()) {
                  hasImage = true;
                  if (imagesFound >= maxImagesInContext)
                        arr[i].erase("images");
                  }
            else if (arr[i].contains("image")) {
                  hasImage = true;
                  if (imagesFound >= maxImagesInContext)
                        arr[i].erase("image");
                  }

            if (hasImage) {
                  imagesFound++;
                  if (imagesFound > maxImagesInContext) {
                        std::string placeholder = "\n[Image removed from history]";
                        if (arr[i].contains("content") && arr[i]["content"].is_string()) {
                              arr[i]["content"] = arr[i]["content"].get<std::string>() + placeholder;
                              }
                        else if (arr[i].contains("parts") && arr[i]["parts"].is_array()) {
                              if (!arr[i]["parts"].empty() && arr[i]["parts"][0].contains("text") &&
                                  arr[i]["parts"][0]["text"].is_string()) {
                                    arr[i]["parts"][0]["text"] =
                                        arr[i]["parts"][0]["text"].get<std::string>() + placeholder;
                                    }
                              }
                        }
                  }
            }

      return arr;
      }

//---------------------------------------------------------
//   addRequest
//---------------------------------------------------------

void Session::addRequest(json content, size_t tokens) {
      if (tokens == 0)
            tokens = estimateTokens(content);
      _data.push_back({content, tokens});
      activeEntries++;
      // Also trim on requests so a long tool loop (which only issues addRequest)
      // cannot grow the context unbounded — inside the loop trim() is limited to
      // shrinking oversized tool outputs (see trim()).
      trim();
      }

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void Session::clear() {
      _data.clear();
      totalTokens               = 0;
      lastReportedContextTokens = 0;
      activeEntries             = 0;
      savedEntries              = 0;
      _toolLoopActive           = false;
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   sessionName
//---------------------------------------------------------

QString Session::sessionName(bool getNext) const {
      SessionInfo info = sessionInfo();
      QDate today      = QDate::currentDate();

      int nextNumber = info.lastNumber + 1; // default: increment
      if (!getNext && info.lastNumber != 0)
            nextNumber = info.lastNumber; // reuse existing number
      else
            nextNumber = info.lastNumber + 1;

      // old Format: Session-dd-MM-yyyy-n.json
      // new Format: Session-yy-MM-dd-n.json

      QString root = agent->editor()->projectRoot();
      if (root.isEmpty()) // no session, no session info
            return QString();

      QString sessionFolder = QDir::cleanPath(root + "/.nped");
      return QString("%1/Session-%2-%3.json")
          .arg(sessionFolder)
          .arg(today.toString("yy-MM-dd"))
          .arg(nextNumber);
      }
