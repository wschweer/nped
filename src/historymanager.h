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
#include <QObject>
#include "logger.h"
#include "types.h"

//---------------------------------------------------------
//   HistoryManager
//---------------------------------------------------------

class HistoryManager : public QObject
      {
      Q_OBJECT
    public:
      struct HistoryItem {
            json content;
            size_t tokens{0};
            };
      std::vector<HistoryItem> _data;
      static constexpr size_t maxEntries         = 40;     // Rolling window: keep at most 40 messages
      static constexpr size_t criticalTokenCount = 30000; // Trigger summary/trim if context exceeds ~30k tokens
      size_t totalTokens{0};
      size_t activeEntries{0};
      bool summaryRequested{false};

    public:
      void clear();
      int messages() const  { return _data.size(); }
      bool empty() const    { return _data.empty(); }
      bool hitLimit() const { return totalTokens > criticalTokenCount; }
      bool trim();
      bool addResult(const json& content, size_t tokens);
      void addRequest(json content, size_t tokens = 0);
      const std::vector<HistoryItem>& data() const { return _data; }
      json getActiveEntries() const;
      void setHistory(const json& h);
      void setActiveEntries(size_t a);
      size_t getActiveEntriesCount() const { return activeEntries; }

    signals:
      void tokensChanged(size_t tokens);
      };
