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
      Q_PROPERTY(bool supportsThinking MEMBER supportsThinking)
      Q_PROPERTY(double temperature MEMBER temperature)
      Q_PROPERTY(double topP MEMBER topP)
      Q_PROPERTY(double topK MEMBER topK)
      Q_PROPERTY(int maxTokens MEMBER maxTokens)
      Q_PROPERTY(int num_ctx MEMBER num_ctx)
      Q_PROPERTY(int num_predict MEMBER num_predict)
      Q_PROPERTY(bool stream MEMBER stream)

    public:
      bool dynamic{false}; // True if a model was added by the system and not the user,
                           // for example automatically detected ollama models. This models are
                           // not saved. If the user edits an model, it will change to dynamic = false.

      QString name;
      QString modelIdentifier;
      QString baseUrl;
      QString apiKey;
      QString api;                                 // "ollama", "gemini", "anthropic", "openai"
      bool supportsThinking               = false; ///< true: model supports Extended Thinking (e.g. claude-3-7-sonnet)
      double temperature                  = -1.0;  ///< <0: use API default
      double topP                         = -1.0;  ///< <0: use API default
      double topK                         = -1.0;  ///< <0: use API default
      int maxTokens                       = -1;    ///< <0: use per-client default
      int num_ctx                         = -1;
      int num_predict                     = -1;
      bool stream                         = true;
      bool operator==(const Model&) const = default;
      json toJson() const;
      Model() {}
      Model(const json&);
      };

using Models = QList<Model>;

extern Models fromJson(const json& array);
extern json toJson(const Models& models);

Q_DECLARE_METATYPE(Model)
Q_DECLARE_METATYPE(Models)
