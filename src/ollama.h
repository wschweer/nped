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
#include <thread>
#include <atomic>

//---------------------------------------------------------
//   OllamaClient
//---------------------------------------------------------

class OllamaClient : public LLMClient
      {
      Q_OBJECT
      std::string currentContent;
      std::string currentThinking;
      std::string _buffer;
      bool _isThinking {false};
      json _currentToolCalls;
      json tools;
      int currentRetryCount {0};
      int maxRetries {12};
      std::atomic<bool> _abort {false};
      std::thread _thread;

      ///< Token accounting reported by Ollama for the last request
      ///< (prompt_eval_count = real size of the request context).
      size_t _lastPromptEvalCount {0};
      size_t _lastEvalCount {0};

      void processTools();

    public:
      OllamaClient(Agent*, Model* m, const std::vector<json>& mcps);
      ~OllamaClient();
      virtual void setTools(const std::vector<json>& mcps) override;
      virtual QString name() const override { return "ollama"; }
      virtual json prompt(QNetworkRequest* request) override;
      virtual void processJsonItem(const json& item) override;
      virtual void dataFinished() override;
      virtual void abort() override;
      };
