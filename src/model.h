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

//---------------------------------------------------------
//   Model
//---------------------------------------------------------

struct Model {
      Q_GADGET
      Q_PROPERTY(bool ollama MEMBER ollama)
      Q_PROPERTY(bool ollamaFound MEMBER ollamaFound)
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

    public:
      bool ollama{false}; // this is not a local ollama model
      bool ollamaFound{false};
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
      bool operator==(const Model&) const = default;
      };

//---------------------------------------------------------
//   Models
//---------------------------------------------------------

class Models : public QList<Model>
      {
    public:
      using QList<Model>::QList;
      void fromJson(const QJsonArray& array);
      QJsonArray toJson() const;
      };

Q_DECLARE_METATYPE(Model)
Q_DECLARE_METATYPE(Models)
