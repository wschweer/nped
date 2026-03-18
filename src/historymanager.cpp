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

#include "historymanager.h"

//---------------------------------------------------------------------------------------
//   trim
//---------------------------------------------------------------------------------------

bool HistoryManager::trim() {
      // 1. Wenn der letzte Turn eine Zusammenfassung war:
      // Wir löschen ALLES vor dieser Zusammenfassung, da sie nun der neue Kontext-Anker ist.
      if (summaryRequested) {
            if (_data.size() >= 2) {
                  HistoryItem lastEntry = _data.back();
                  HistoryItem prevEntry = _data[_data.size() - 2];
                  _data.clear();
                  _data.push_back(prevEntry); // user summary request
                  _data.push_back(lastEntry); // model summary response
                  totalTokens = prevEntry.tokens + lastEntry.tokens;
                  }
            else if (_data.size() >= 1) {
                  HistoryItem lastEntry = _data.back();
                  _data.clear();
                  _data.push_back(lastEntry);
                  totalTokens = lastEntry.tokens;
                  }
            summaryRequested = false;
            return false;
            }

      // 2. Klassisches Rolling Window (von vorne kürzen)
      while (_data.size() > maxEntries) {
            if (_data.empty())
                  break;
            totalTokens -= _data.front().tokens;
            _data.erase(_data.begin());
            while (!_data.empty()) {
                  std::string r = _data.front().content.value("role", "");
                  if (r == "user")
                        break;
                  totalTokens -= _data.front().tokens;
                  _data.erase(_data.begin());
                  }
            }
      if (hitLimit()) {
            // request summary
            std::string text = "Please provide a concise technical summary of our conversation so far. "
                               "Focus specifically on the results obtained from the tool calls and the final "
                               "conclusions reached. Discard the raw, voluminous data output from the tools, "
                               "but retain the key facts, parameters used, and the current state of the task. "
                               "This summary will serve as the new starting point for our context, "
                               "so ensure no critical logical step is lost.";
            json msg;
            msg["role"]  = "user";
            msg["parts"] = json::array({{{"text", text}}});
            addRequest(msg, text.length() / 4);
            summaryRequested = true;
            }
      return summaryRequested;
      }

//---------------------------------------------------------
//   addResult
//---------------------------------------------------------

bool HistoryManager::addResult(const json& content, size_t tokens) {
      _data.push_back({content, tokens});
      totalTokens      += tokens;
      bool needSummary  = trim();
      return needSummary;
      }

//---------------------------------------------------------
//   setHistory
//---------------------------------------------------------

void HistoryManager::setHistory(const json& h) {
      clear();
      for (const auto& item : h) {
            // Approximation: 4 chars per token
            size_t tokens = 0;
            if (item.contains("parts")) {
                  for (const auto& part : item["parts"])
                        if (part.contains("text"))
                              tokens += part["text"].get<std::string>().length() / 4;
                  }
            _data.push_back({item, tokens});
            totalTokens += tokens;
            }
      }
