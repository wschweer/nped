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

#pragma once

#include <QWidget>
#include "types.h"

//---------------------------------------------------------
//   Model
//---------------------------------------------------------

struct Model {
      Q_GADGET
      Q_PROPERTY(QString name MEMBER name)
      Q_PROPERTY(QString modelIdentifier MEMBER modelIdentifier)
      Q_PROPERTY(QString baseUrl MEMBER baseUrl)
      Q_PROPERTY(QString apiKey MEMBER apiKey)
      Q_PROPERTY(QString api MEMBER api)
      // --- Advanced parameters
      Q_PROPERTY(int maxTokens MEMBER maxTokens)
      Q_PROPERTY(QString configuration MEMBER configuration)
      Q_PROPERTY(bool stream MEMBER stream)
      Q_PROPERTY(bool protected MEMBER protected_)

    public:
      bool dynamic {false}; // True if a model was added by the system and not the user,
                            // for example automatically detected ollama models. This models are
                            // not saved. If the user edits an model, it will change to dynamic = false.

      QString name;
      QString modelIdentifier;
      QString baseUrl;
      QString apiKey;
      QString api;        ///< "ollama", "gemini", "gemini2", "anthropic", "openai"
      int maxTokens = -1; ///< <0: use per-client default
      ///< Provider-native options as a JSON object string, passed through to
      ///< the provider unchanged, e.g. for Ollama
      ///<   {"temperature": 0.7, "top_p": 0.9, "num_ctx": 8192}
      ///< or for Anthropic {"thinking": {"type": "enabled", "budget_tokens": 4096}}.
      QString configuration;
      bool stream     = true;
      bool protected_ = true; ///< true: run agent tools in sandbox (bwrap); false: run on host machine
      bool operator==(const Model&) const = default;
      json toJson() const;
      Model() {}
      Model(const json&);
      ///< Parse the configuration JSON string into a JSON object ({} if empty/invalid).
      json configJson() const;
      };

using Models = QList<Model>;

extern Models fromJson(const json& array);
extern json toJson(const Models& models);

Q_DECLARE_METATYPE(Model)
Q_DECLARE_METATYPE(Models)
