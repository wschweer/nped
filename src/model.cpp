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

#include "logger.h"
#include "model.h"

//---------------------------------------------------------
//   serialize TextStyles
//---------------------------------------------------------

json Models::toJson() const {
      json array = json::array();
      for (const auto& m : *this) {
            if (m.dynamic)
                  continue;
            json obj;
            obj["name"]             = m.name.toStdString();
            obj["url"]              = m.baseUrl.toStdString();
            obj["key"]              = m.apiKey.toStdString();
            obj["modelId"]          = m.modelIdentifier.toStdString();
            obj["api"]              = m.api.toStdString();
            obj["supportsThinking"] = m.supportsThinking;
            obj["temperature"]      = m.temperature;
            obj["topP"]             = m.topP;
            obj["topK"]             = m.topK;
            obj["maxTokens"]        = m.maxTokens;
            obj["num_ctx"]          = m.num_ctx;
            obj["num_predict"]      = m.num_predict;
            obj["stream"]           = m.stream;
            array.push_back(obj);
            }
      return array;
      }

//---------------------------------------------------------
//   fromJson
//---------------------------------------------------------

void Models::fromJson(const json& array) {
      clear();
      try {
            for (const json& obj : array) {
                  Model m;
                  m.name            = QString::fromStdString(obj["name"]);
                  m.baseUrl         = QString::fromStdString(obj["url"]);
                  m.apiKey          = QString::fromStdString(obj["key"]);
                  m.modelIdentifier = QString::fromStdString(obj["modelId"]);
                  m.api             = QString::fromStdString(obj["api"]);

                  // Optional fields – graceful fallback to struct defaults if absent
                  m.supportsThinking = obj.value("supportsThinking", false);
                  m.temperature      = obj.value("temperature", -1.0);
                  m.topP             = obj.value("topP", -1.0);
                  m.topK             = obj.value("topK", -1.0);
                  m.maxTokens        = obj.value("maxTokens", -1);
                  m.num_ctx          = obj.value("num_ctx", -1);
                  m.num_predict      = obj.value("num_predict", -1);
                  m.stream           = obj.value("stream", true);
                  append(m);
                  }
            }
      catch (const json::parse_error& e) {
            Debug("Parse Error: {}", e.what());
            }
      catch (const json::type_error& e) {
            Debug("TypeError: {}", e.what());
            }
      catch (...) {
            Critical("Unexpected error");
            }
      }
