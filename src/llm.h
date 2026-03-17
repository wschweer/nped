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

#include <QObject>
#include <QString>
#include <QUrl>
#include <vector>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

struct Model;
class QNetworkRequest;
class QNetworkReply;
class Agent;

//---------------------------------------------------------
//   LLMClient
//---------------------------------------------------------

class LLMClient : public QObject
      {
      Q_OBJECT

    protected:
      Agent* agent;

    public:
      Model* model;
      LLMClient(Agent* a, Model* m) : QObject(nullptr) {
            agent = a;
            model = m;
            }
      virtual ~LLMClient() = default;

      virtual QString name() const                   = 0;
      virtual json prompt(QNetworkRequest* request)  = 0;
      virtual void processJsonItem(const json& item) = 0;
      virtual void dataFinished()      = 0;

    signals:
      void incomingChunk(const QString& thought, const QString& text);
      };

extern LLMClient* llmFactory(Agent*, Model* m, const std::vector<json>& mcpTools);
