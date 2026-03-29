//=============================================================================
//  nped Program Editor
//
//  Copyright (C) 2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include <QJsonArray>
#include <QJsonObject>

#include "model.h"

//---------------------------------------------------------
//   serialize TextStyles
//---------------------------------------------------------

QJsonArray Models::toJson() const {
      QJsonArray array;
      for (const auto& m : *this) {
            if (m.isLocal)
                  continue;
            QJsonObject obj;
            obj["name"]               = m.name;
            obj["url"]                = m.baseUrl;
            obj["key"]                = m.apiKey;
            obj["modelId"]            = m.modelIdentifier;
            obj["api"]                = m.api;
            obj["supportsThinking"]   = m.supportsThinking;
            obj["temperature"]        = m.temperature;
            obj["topP"]               = m.topP;
            obj["maxTokens"]          = m.maxTokens;
            array.append(obj);
            }
      return array;
      }

void Models::fromJson(const QJsonArray& array) {
      clear();
      for (int i = 0; i < array.size(); ++i) {
            QJsonObject obj = array[i].toObject();

            Model m;
            m.name            = obj["name"].toString();
            m.baseUrl         = obj["url"].toString();
            m.apiKey          = obj["key"].toString();
            m.modelIdentifier = obj["modelId"].toString();
            m.api             = obj["api"].toString();
            m.isLocal         = false;
            // Optional fields – graceful fallback to struct defaults if absent
            if (obj.contains("supportsThinking"))
                  m.supportsThinking = obj["supportsThinking"].toBool();
            if (obj.contains("temperature"))
                  m.temperature = obj["temperature"].toDouble(-1.0);
            if (obj.contains("topP"))
                  m.topP = obj["topP"].toDouble(-1.0);
            if (obj.contains("maxTokens"))
                  m.maxTokens = obj["maxTokens"].toInt(-1);
            append(m);
            }
      }
