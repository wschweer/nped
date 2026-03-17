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
//   GeminiClient
//---------------------------------------------------------

class GeminiClient : public LLMClient
      {
      Q_OBJECT
      json currentContent;
      json _currentToolCalls;
      json tools;
      int currentRetryCount{0};
      bool isRetrying{false};
      int maxRetries{12};

      void processData();
      bool trimHistory(json& history, bool hasSummary);
      void processTools();

    public:
      GeminiClient(Agent*, Model* m, const std::vector<json>& mcps);
      virtual QString name() const override { return "gemini"; }
      virtual json prompt(QNetworkRequest* request) override;
      virtual void processJsonItem(const json& item) override;
      virtual void dataFinished() override;
      };
