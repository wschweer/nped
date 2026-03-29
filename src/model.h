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
      Q_PROPERTY(QString name MEMBER name)
      Q_PROPERTY(QString modelIdentifier MEMBER modelIdentifier)
      Q_PROPERTY(QString baseUrl MEMBER baseUrl)
      Q_PROPERTY(QString apiKey MEMBER apiKey)
      Q_PROPERTY(QString api MEMBER api)
      Q_PROPERTY(bool isLocal MEMBER isLocal)
      // --- Advanced parameters
      Q_PROPERTY(bool supportsThinking MEMBER supportsThinking)
      Q_PROPERTY(double temperature MEMBER temperature)
      Q_PROPERTY(double topP MEMBER topP)
      Q_PROPERTY(int maxTokens MEMBER maxTokens)

    public:
      QString name;
      QString modelIdentifier;
      QString baseUrl;
      QString apiKey;
      QString api;                                       // "ollama", "gemini", "anthropic", "openai"
      bool isLocal                              = false; ///< true für Ollama
      bool supportsThinking                     = false; ///< true: model supports Extended Thinking (e.g. claude-3-7-sonnet)
      double temperature                        = -1.0;  ///< <0: use API default
      double topP                               = -1.0;  ///< <0: use API default
      int maxTokens                             = -1;    ///< <0: use per-client default
      bool operator==(const Model& other) const = default;
      };

//---------------------------------------------------------
//   Models
//---------------------------------------------------------

class Models : public QList<Model> {
   public:
      using QList<Model>::QList;
      void fromJson(const QJsonArray& array);
      QJsonArray toJson() const;
      };

Q_DECLARE_METATYPE(Model)
Q_DECLARE_METATYPE(Models)
