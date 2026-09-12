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

#include <QDir>
#include <QHash>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <cstddef>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>
#include "logger.h"

using json = nlohmann::json;

//---------------------------------------------------------
//   LlmInfo
//    Metadata which was discovered from the LLM provider for a
//    single model.  It is used to manage the chat history
//    dynamically: the effective context window is the key value
//    for Session::contextBudget().
//
//    Currently filled by OllamaInfo from the Ollama endpoints
//    /api/tags (digest, capabilities), /api/show (trained
//    context length, architecture, Modelfile) and /api/ps
//    (context size actually used at runtime).
//---------------------------------------------------------

struct LlmInfo {
      QString modelId;      ///< provider model identifier
      QString digest;       ///< digest; used to detect changed models
      QString architecture; ///< e.g. "qwen2", "llama", "gemma4"
      QString family;
      QString parameterSize;           ///< e.g. "1.8B"
      QString quantization;            ///< e.g. "Q4_K_M"
      size_t trainedContextLength {0}; ///< context size of the trained model
      size_t runtimeContextLength {0}; ///< num_ctx Ollama really uses (0: unknown)
      size_t embeddingLength {0};
      int blockCount {0};
      int modelfileNumCtx {0};  ///< num_ctx baked into the Modelfile (0: none)
      QStringList capabilities; ///< completion, tools, thinking, vision, audio
      bool valid() const { return !modelId.isEmpty(); }
      bool hasCapability(const QString& c) const { return capabilities.contains(c); }
      bool supportsTools() const { return hasCapability(QStringLiteral("tools")); }
      bool supportsVision() const { return hasCapability(QStringLiteral("vision")); }
      bool supportsThinking() const { return hasCapability(QStringLiteral("thinking")); }
      ///< Context window to use for the history budget: prefer the size the
      ///< provider actually uses at runtime, fall back to the trained size.
      ///< Returns 0 if nothing has been discovered yet.
      size_t effectiveContextLength() const {
            return runtimeContextLength ? runtimeContextLength : trainedContextLength;
            }
      ///< One line human readable summary, used for logs and tool tips.
      std::string summary() const {
            std::string s = architecture.isEmpty() ? family.toStdString() : architecture.toStdString();
            if (!parameterSize.isEmpty())
                  s += " " + parameterSize.toStdString();
            if (!quantization.isEmpty())
                  s += " " + quantization.toStdString();
            size_t ctx = effectiveContextLength();
            if (ctx) {
                  s += std::format(" ctx={}", ctx);
                  if (trainedContextLength && trainedContextLength != ctx)
                        s += std::format(" (trained {})", trainedContextLength);
                  }
            if (modelfileNumCtx)
                  s += std::format(" modelfile_num_ctx={}", modelfileNumCtx);
            if (!capabilities.isEmpty())
                  s += " [" + capabilities.join(QStringLiteral(", ")).toStdString() + "]";
            return s;
            }
      json toJson() const {
            json caps = json::array();
            for (const auto& c : capabilities)
                  caps.push_back(c.toStdString());
            json j;
            j["modelId"]              = modelId.toStdString();
            j["digest"]               = digest.toStdString();
            j["architecture"]         = architecture.toStdString();
            j["family"]               = family.toStdString();
            j["parameterSize"]        = parameterSize.toStdString();
            j["quantization"]         = quantization.toStdString();
            j["trainedContextLength"] = trainedContextLength;
            j["runtimeContextLength"] = runtimeContextLength;
            j["embeddingLength"]      = embeddingLength;
            j["blockCount"]           = blockCount;
            j["modelfileNumCtx"]      = modelfileNumCtx;
            j["capabilities"]         = caps;
            return j;
            }
      static LlmInfo fromJson(const json& j) {
            LlmInfo i;
            if (!j.is_object())
                  return i;
            i.modelId              = QString::fromStdString(j.value("modelId", std::string()));
            i.digest               = QString::fromStdString(j.value("digest", std::string()));
            i.architecture         = QString::fromStdString(j.value("architecture", std::string()));
            i.family               = QString::fromStdString(j.value("family", std::string()));
            i.parameterSize        = QString::fromStdString(j.value("parameterSize", std::string()));
            i.quantization         = QString::fromStdString(j.value("quantization", std::string()));
            i.trainedContextLength = j.value("trainedContextLength", size_t(0));
            i.runtimeContextLength = j.value("runtimeContextLength", size_t(0));
            i.embeddingLength      = j.value("embeddingLength", size_t(0));
            i.blockCount           = j.value("blockCount", 0);
            i.modelfileNumCtx      = j.value("modelfileNumCtx", 0);
            if (j.contains("capabilities") && j["capabilities"].is_array())
                  for (const auto& c : j["capabilities"])
                        if (c.is_string())
                              i.capabilities << QString::fromStdString(c.get<std::string>());
            return i;
            }
      };

//---------------------------------------------------------
//   LlmInfoStore
//    Persistent cache of discovered LLM metadata.  The
//    information is keyed by the provider model identifier and
//    stored in the application config directory (llm_info.json)
//    so that it survives restarts and is available before the
//    first request is sent ("spätere Verwendung").
//---------------------------------------------------------

class LlmInfoStore
      {
      QHash<QString, LlmInfo> _cache;
      QString _path;

    public:
      LlmInfoStore() : _path(defaultPath()) {}
      static QString defaultPath() {
            QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            QDir().mkpath(dir);
            return dir + "/llm_info.json";
            }
      const QString& path() const { return _path; }
      int count() const { return _cache.size(); }
      void load() {
            std::ifstream f(_path.toStdString());
            if (!f.is_open())
                  return;
            try {
                  json j;
                  f >> j;
                  _cache.clear();
                  for (const auto& [key, value] : j.items())
                        _cache.insert(QString::fromStdString(key), LlmInfo::fromJson(value));
                  Debug("loaded LLM info for {} model(s) from <{}>", _cache.size(), _path);
                  }
            catch (const std::exception& e) {
                  Warning("cannot read LLM info cache <{}>: {}", _path, e.what());
                  }
            }
      bool save() const {
            json j = json::object();
            for (auto it = _cache.constBegin(); it != _cache.constEnd(); ++it)
                  j[it.key().toStdString()] = it.value().toJson();

            QSaveFile file(_path);
            if (!file.open(QIODeviceBase::WriteOnly | QIODeviceBase::Text)) {
                  Warning("cannot write LLM info cache <{}>", _path);
                  return false;
                  }
            file.write(QByteArray::fromStdString(j.dump(2)));
            return file.commit();
            }
      ///< Returns the cached info for a model (invalid if unknown).
      LlmInfo get(const QString& modelId) const { return _cache.value(modelId); }
      bool contains(const QString& modelId) const { return _cache.contains(modelId); }
      ///< Store (or update) the info for a model.
      void put(const LlmInfo& info) {
            if (info.valid())
                  _cache.insert(info.modelId, info);
            }
      ///< Merge freshly discovered information into the cache.  Returns true
      ///< when the stored entry changed (i.e. the cache should be saved).
      bool merge(const LlmInfo& info) {
            if (!info.valid())
                  return false;
            if (_cache.value(info.modelId).toJson() == info.toJson())
                  return false;
            _cache.insert(info.modelId, info);
            return true;
            }
      void clear() { _cache.clear(); }
      };
