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
            Debug("****Summary Requested");
            size_t n = 0;
            for (auto it = _data.rbegin(); it != _data.rend(); ++it) {
                  n++;
                  if (it->content.value("role", "") == "user")
                        break;
                  }
            activeEntries = n;
            totalTokens   = 0;
            for (size_t i = _data.size() - activeEntries; i < _data.size(); ++i)
                  totalTokens += _data[i].tokens;
            summaryRequested = false;
            emit tokensChanged(totalTokens);
            return false;
            }

      // 2. Klassisches Rolling Window (nur activeEntries reduzieren statt Löschen)
      size_t n = activeEntries;
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
      if (n != activeEntries)
            Debug("****Reduced History from {} to {} entries", n, activeEntries);
//      if (hitLimit()) {
//          request summary
//            summaryRequested = true;
//            }
      emit tokensChanged(totalTokens);
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
      emit tokensChanged(totalTokens);
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
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   setActiveEntries
//---------------------------------------------------------

void HistoryManager::setActiveEntries(size_t a) {
      activeEntries   = std::min(a, _data.size());
      totalTokens     = 0;
      size_t startIdx = _data.size() - activeEntries;
      for (size_t i = startIdx; i < _data.size(); ++i)
            totalTokens += _data[i].tokens;
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   getActiveEntries
//---------------------------------------------------------

json HistoryManager::getActiveEntries() const {
      json arr        = json::array();
      size_t startIdx = _data.size() > activeEntries ? _data.size() - activeEntries : 0;

      // Always include the very first user request if history is truncated
      if (startIdx > 0 && !_data.empty())
            arr.push_back(_data[0].content);

      for (size_t i = startIdx; i < _data.size(); ++i)
            arr.push_back(_data[i].content);

      return arr;
      }

//---------------------------------------------------------
//   addRequest
//---------------------------------------------------------

void HistoryManager::addRequest(json content, size_t tokens) {
      _data.push_back({content, tokens});
      totalTokens += tokens;
      activeEntries++;
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void HistoryManager::clear() {
      _data.clear();
      totalTokens   = 0;
      activeEntries = 0;
      emit tokensChanged(totalTokens);
      }
