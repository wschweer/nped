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
      obj["name"]        = name.toStdString();
      obj["url"]         = baseUrl.toStdString();
      obj["key"]         = apiKey.toStdString();
      obj["modelId"]     = modelIdentifier.toStdString();
      obj["api"]         = api.toStdString();
      obj["maxTokens"]   = maxTokens;
      obj["configuration"] = configuration.toStdString();
      obj["stream"]      = stream;
      obj["protected"]   = protected_;
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
            maxTokens = obj.value("maxTokens", -1);
            stream    = obj.value("stream", true);
            protected_ = obj.value("protected", true);

            // Advanced options are stored as a single JSON string holding the
            // provider-native options (e.g. Ollama "options" keys or Anthropic
            // "thinking").  An already-parsed object is accepted for convenience.
            if (obj.contains("configuration")) {
                  if (obj["configuration"].is_string())
                        configuration = QString::fromStdString(obj["configuration"].get<std::string>());
                  else if (obj["configuration"].is_object())
                        configuration = QString::fromStdString(obj["configuration"].dump(2));
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

//---------------------------------------------------------
//   configJson
//    Parse the 'configuration' string into a JSON object.
//    Returns an empty object when the string is blank or invalid JSON.
//---------------------------------------------------------

json Model::configJson() const {
      if (configuration.isEmpty())
            return json::object();
      try {
            json j = json::parse(configuration.toStdString());
            return j.is_object() ? j : json::object();
            }
      catch (...) {
            Debug("invalid configuration JSON for model <{}>", name.toStdString());
            return json::object();
            }
      }
