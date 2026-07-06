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

#include "llm.h"

//---------------------------------------------------------
//   Gemini2Client
//---------------------------------------------------------

class Gemini2Client : public LLMClient
      {
      Q_OBJECT
      std::string _lastInteractionId;
      std::string _currentStepType;
      std::string _accumulatedText;
      std::string _accumulatedThought;
      json _lastUsageMetadata;
      json _currentToolCalls;
      json tools;

      void sanitizeSchemaRecursive(json& schema, bool isRoot);
      void processTools();

    public:
      Gemini2Client(Agent*, Model* m, const std::vector<json>& mcps);
      virtual void setTools(const std::vector<json>& mcps) override;
      virtual QString name() const override { return "gemini2"; }
      virtual json prompt(QNetworkRequest* request) override;
      virtual void processJsonItem(const json& item) override;
      virtual void dataFinished() override;
      };
