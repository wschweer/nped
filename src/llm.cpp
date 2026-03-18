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

#include <list>
#include "logger.h"
#include "ollama.h"
#include "gemini.h"
#include "anthropic.h"
#include "openai.h"
#include "agent.h"

static std::vector<LLMClient*> clientList;

//---------------------------------------------------------
//   name
//---------------------------------------------------------

QString LLMClient::name() const {
      return model->api;
      }

//---------------------------------------------------------
//   llmFactory
//---------------------------------------------------------

LLMClient* llmFactory(Agent* agent, Model* model, const std::vector<json>& mcps) {
      LLMClient* client = nullptr;
      for (const auto c : clientList)
            if (c->name() == model->api)
                  client = c;
      if (!client) {
            if (model->api == "gemini")
                  client = new GeminiClient(agent, model, mcps);
            else if (model->api == "ollama")
                  client = new OllamaClient(agent, model, mcps);
            else if (model->api == "anthropic")
                  client = new AnthropicClient(agent, model, mcps);
            else if (model->api == "openai")
                  client = new OpenAiClient(agent, model, mcps);
            else {
                  Critical("unknown llm interface <{}>", model->api);
                  client = new OllamaClient(agent, model, mcps);
                  client->model = model;
                  return client;
                  }
            if (client)
                  clientList.push_back(client);
            }
      if (client)
            client->model = model;
      return client;
      }
