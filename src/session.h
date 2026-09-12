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
#include <algorithm>
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

      ///< Recompute totalTokens from the active window (no signal emitted).
      void recalcTokens();

    signals:
      void tokensChanged(size_t tokens);

    public:
      Session(Agent* a, QObject* parent = nullptr) : QObject(parent), agent(a) {}
      struct SessionItem {
            json content;
            size_t tokens {0};
            };
      std::vector<SessionItem> _data;

      // Rolling window limits. The authoritative trigger is the token budget
      // (see contextBudget()); maxEntries is a coarse safety cap on the number
      // of active messages and minEntries a floor so that even a window made of
      // few very long messages can still be reduced.
      static constexpr size_t maxEntries         = 800;    // hard cap on active messages
      static constexpr size_t minEntries         = 100;    // never trim below this many
      static constexpr size_t defaultTokenBudget = 190000; // fallback budget (~90k tokens)

      size_t totalTokens {0};
      ///< Context size reported by the provider for the last request. Used as a
      ///< safety floor because our char/4 estimate can undercount real tokens.
      size_t lastReportedContextTokens {0};
      size_t activeEntries {0};
      bool _toolLoopActive {false};

      void clear();
      int messages() const { return _data.size(); }
      bool empty() const { return _data.empty(); }
      ///< Token count to compare against the budget: the larger of our estimate
      ///< and the context size the provider actually reported last time.
      size_t effectiveTokens() const { return std::max(totalTokens, lastReportedContextTokens); }
      bool hitLimit() const { return effectiveTokens() > contextBudget(); }
      ///< Best-effort token estimate (~4 chars/token) for a single message.
      static size_t estimateTokens(const json& content);
      ///< Token budget derived from the current model's context window.
      size_t contextBudget() const;
      ///< Context window discovered from the provider (LlmInfo).
      size_t discoveredContextWindow() const;
      ///< Fraction of the context window used as the history budget (75 %).
      static constexpr size_t budgetRatioPercent = 75;

      void optimizeToolResponses();
      void trim();
      void addResult(const json& content, size_t tokens = 0);
      void addRequest(json content, size_t tokens = 0);
      const std::vector<SessionItem>& data() const { return _data; }
      json getActiveEntries() const;
      void setHistory(const json& h);
      void setActiveEntries(size_t a);
      size_t getActiveEntriesCount() const { return activeEntries; }
      void setReportedContextTokens(size_t t) { lastReportedContextTokens = t; }
      void load(const QString& sessionPath);
      void save();

      SessionInfo sessionInfo() const;
      QString sessionName(bool getNext) const;
      QString name() const { return _name; }
      void setName(const QString& v) { _name = v; }
      void setToolLoopActive(bool v) { _toolLoopActive = v; }
      };
