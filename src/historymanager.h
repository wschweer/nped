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
      bool summaryRequested{false};

    public:

      void clear() {
            _data.clear();
            totalTokens = 0;
            }
      int messages() const { return _data.size(); }
      bool empty() const { return _data.empty(); }
      bool hitLimit() const { return totalTokens > criticalTokenCount; }
      bool trim();
      bool addResult(const json& content, size_t tokens);
      void addRequest(json content, size_t tokens = 0) {
            _data.push_back({content, tokens});
            totalTokens += tokens;
            }
      const std::vector<HistoryItem>& data() const {
            return _data;
            }
      void setHistory(const json& h);
      };
