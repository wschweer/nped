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

    public:
      OpenAiClient(Model* m, const std::vector<json>& mcps);
      virtual json prompt(QNetworkRequest* request, const json& chatHistory) override;
      virtual void dataReceived(Agent*, QNetworkReply*) override {}
      virtual void dataFinished(Agent*, QNetworkReply*) override {};
      };
