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
//   OpenAiClient
//---------------------------------------------------------

class OpenAiClient : public LLMClient
      {
      Q_OBJECT
      std::string currentContent;
      json _currentToolCalls;
      json tools;

      void processTools();

    public:
      OpenAiClient(Agent* a, Model* m, const std::vector<json>& mcps);
      virtual void setTools(const std::vector<json>& mcps) override;
      virtual QString name() const override { return "openai"; }
      virtual json prompt(QNetworkRequest* request) override;
      virtual void processJsonItem(const json& item) override;
      virtual void dataFinished() override;
      };
