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

#pragma once

#include <iostream>
#include <fstream>
#include "logger.h"
#include "types.h"

//---------------------------------------------------------
//   HistoryManager
//---------------------------------------------------------

class HistoryManager
      {
    public:
      struct HistoryItem {
            json content;
            size_t tokens{0};
            };
      std::vector<HistoryItem> _data;
      const size_t maxEntries         = 30;
      const size_t criticalTokenCount = 30000; // Trigger summary if total context > 30k tokens
      size_t totalTokens{0};
      size_t activeEntries{0};
      bool summaryRequested{false};

    public:

      void clear() {
            _data.clear();
            totalTokens   = 0;
            activeEntries = 0;
            }
      int messages() const { return _data.size(); }
      bool empty() const { return _data.empty(); }
      bool hitLimit() const { return totalTokens > criticalTokenCount; }
      bool trim();
      bool addResult(const json& content, size_t tokens);
      void addRequest(json content, size_t tokens = 0) {
            _data.push_back({content, tokens});
            totalTokens += tokens;
            activeEntries++;
            }
      const std::vector<HistoryItem>& data() const { return _data; }
      json getActiveEntries() const {
            json arr        = json::array();
            size_t startIdx = _data.size() > activeEntries ? _data.size() - activeEntries : 0;
            for (size_t i = startIdx; i < _data.size(); ++i)
                  arr.push_back(_data[i].content);
            return arr;
            }
      void setHistory(const json& h);
      void setActiveEntries(size_t a) {
            activeEntries   = std::min(a, _data.size());
            totalTokens     = 0;
            size_t startIdx = _data.size() - activeEntries;
            for (size_t i = startIdx; i < _data.size(); ++i)
                  totalTokens += _data[i].tokens;
            }
      size_t getActiveEntriesCount() const { return activeEntries; }
      };
