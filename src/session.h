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

#include <QDateTime>
#include <nlohmann/json.hpp>
#include "types.h"

class Agent;

//---------------------------------------------------------
//   SessionInfo
//---------------------------------------------------------

struct SessionInfo {
      QString fileName;
      int lastNumber = 0;
      QDate lastDate;
      };

//---------------------------------------------------------
//   Session
//---------------------------------------------------------

class Session : public QObject
      {
      Q_OBJECT

      Agent* agent;
      size_t savedEntries {0};
      QString _name;

    signals:
      void tokensChanged(size_t tokens);

    public:
      Session(Agent* a, QObject* parent = nullptr) : QObject(parent), agent(a) {}
      struct SessionItem {
            json content;
            size_t tokens{0};
            };
      std::vector<SessionItem> _data;
      static constexpr size_t maxEntries         = 20;     // Rolling window: keep at most 40 messages
      static constexpr size_t criticalTokenCount = 30000; // Trigger summary/trim if context exceeds ~30k tokens
      size_t totalTokens{0};
      size_t activeEntries{0};
      bool summaryRequested{false};

      void clear();
      int messages() const  { return _data.size(); }
      bool empty() const    { return _data.empty(); }
      bool hitLimit() const { return totalTokens > criticalTokenCount; }
      void optimizeToolResponses();
      bool trim();
      bool addResult(const json& content, size_t tokens);
      void addRequest(json content, size_t tokens = 0);
      const std::vector<SessionItem>& data() const { return _data; }
      json getActiveEntries() const;
      void setHistory(const json& h);
      void setActiveEntries(size_t a);
      size_t getActiveEntriesCount() const { return activeEntries; }

      void load(const QString& sessionPath);
      void save();

      SessionInfo sessionInfo() const;
      QString sessionName(bool getNext) const;
      QString name() const { return _name;  }
      void setName(const QString& v) { _name = v; }
      };
