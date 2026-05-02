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
//   toJson
//---------------------------------------------------------

json Model::toJson() const {
      json obj;
      obj["name"]             = name.toStdString();
      obj["url"]              = baseUrl.toStdString();
      obj["key"]              = apiKey.toStdString();
      obj["modelId"]          = modelIdentifier.toStdString();
      obj["api"]              = api.toStdString();
      obj["supportsThinking"] = supportsThinking;
      obj["temperature"]      = temperature;
      obj["topP"]             = topP;
      obj["topK"]             = topK;
      obj["maxTokens"]        = maxTokens;
      obj["num_ctx"]          = num_ctx;
      obj["num_predict"]      = num_predict;
      obj["stream"]           = stream;
      return obj;
      }

//---------------------------------------------------------
//   serialize TextStyles
//---------------------------------------------------------

json toJson(const Models& models) {
      json array = json::array();
      for (const auto& m : models) {
            // if (m.dynamic)
            //      continue;
            array.push_back(m.toJson());
            }
      return array;
      }

extern Models fromJson(const json& array) {
      Models models;
      for (const auto& m : array)
            models.push_back(Model(m));
      return models;
      }

//---------------------------------------------------------
//   fromJson
//---------------------------------------------------------

Model::Model(const json& obj) {
      try {
            name            = QString::fromStdString(obj["name"]);
            baseUrl         = QString::fromStdString(obj["url"]);
            apiKey          = QString::fromStdString(obj["key"]);
            modelIdentifier = QString::fromStdString(obj["modelId"]);
            api             = QString::fromStdString(obj["api"]);

            // Optional fields – graceful fallback to struct defaults if absent
            supportsThinking = obj.value("supportsThinking", false);
            temperature      = obj.value("temperature", -1.0);
            topP             = obj.value("topP", -1.0);
            topK             = obj.value("topK", -1.0);
            maxTokens        = obj.value("maxTokens", -1);
            num_ctx          = obj.value("num_ctx", -1);
            num_predict      = obj.value("num_predict", -1);
            stream           = obj.value("stream", true);
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
