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

#include <QNetworkRequest>
#include "openai.h"
#include "agent.h"

//---------------------------------------------------------
//   OpenAiClient
//---------------------------------------------------------

OpenAiClient::OpenAiClient(Model* m, const std::vector<json>& mcps) : LLMClient(m) {
      }

//---------------------------------------------------------
//   sendPrompt
//---------------------------------------------------------

json OpenAiClient::prompt(QNetworkRequest* request, const json& chatHistory) {
      request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
      json requestJson;
      requestJson["model"]    = model->modelIdentifier.toStdString();
      requestJson["stream"]   = true;
      requestJson["tools"]    = tools;
      requestJson["messages"] = chatHistory;
      return requestJson;
      };
