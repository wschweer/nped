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
      // Wir setzen activeEntries auf 2 (Zusammenfassungs-Anfrage und Modell-Antwort).
      if (summaryRequested) {
            if (_data.size() >= 2) {
                  HistoryItem lastEntry = _data.back();
                  HistoryItem prevEntry = _data[_data.size() - 2];
                  activeEntries         = 2;
                  totalTokens           = prevEntry.tokens + lastEntry.tokens;
                  }
            else if (_data.size() >= 1) {
                  activeEntries = 1;
                  totalTokens   = _data.back().tokens;
                  }
            summaryRequested = false;
            return false;
            }

      // 2. Klassisches Rolling Window (nur activeEntries reduzieren statt Löschen)
      while (activeEntries > maxEntries) {
            if (activeEntries == 0)
                  break;
            size_t idx   = _data.size() - activeEntries;
            totalTokens -= _data[idx].tokens;
            activeEntries--;
            while (activeEntries > 0) {
                  idx           = _data.size() - activeEntries;
                  std::string r = _data[idx].content.value("role", "");
                  if (r == "user")
                        break;
                  totalTokens -= _data[idx].tokens;
                  activeEntries--;
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
      totalTokens += tokens;
      activeEntries++;
      bool needSummary = trim();
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
            // note: totalTokens will be recomputed by the caller using setActiveEntries,
            // or we assume all are active initially for backward compatibility
            totalTokens += tokens;
            activeEntries++;
            }
      }
