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
//   AnthropicClient
//---------------------------------------------------------

class AnthropicClient : public LLMClient
      {
      Q_OBJECT
      std::string currentContent;
      json currentThinkingBlock;        ///< Full thinking block object {type, thinking, signature} for round-trip
      json _currentToolCalls;
      json tools;
      size_t _inputTokens{0};           ///< From message_start usage
      size_t _outputTokens{0};          ///< Accumulated from message_delta usage

      void processTools(json resolvedToolCalls);

    public:
      AnthropicClient(Agent* a, Model* m, const std::vector<json>& mcps);
      virtual QString name() const override { return "anthropic"; }
      virtual json prompt(QNetworkRequest* request) override;
      virtual void processJsonItem(const json& item) override;
      virtual void dataFinished() override;
      };
