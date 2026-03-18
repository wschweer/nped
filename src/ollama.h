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
//   OllamaClient
//---------------------------------------------------------

class OllamaClient : public LLMClient
      {
      Q_OBJECT
      std::string currentContent;
      json _currentToolCalls;
      json tools;
      int currentRetryCount{0};
      bool isRetrying{false};
      int maxRetries{12};

      void processTools();

    public:
      OllamaClient(Agent*, Model* m, const std::vector<json>& mcps);
      virtual QString name() const override { return "ollama"; }
      virtual json prompt(QNetworkRequest* request) override;
      virtual void processJsonItem(const json& item) override;
      virtual void dataFinished() override;
      };
