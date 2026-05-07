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

#include <QDir>
#include "session.h"
#include "agent.h"
#include "editor.h"

//---------------------------------------------------------
//   getLastSessionInfo
//    Sucht die aktuellste Datei und liefert Metadaten zurück
//---------------------------------------------------------

SessionInfo Session::sessionInfo() const {
      SessionInfo info;
      QString root = agent->editor()->projectRoot();
      if (root.isEmpty()) { // no session, no session info
            Debug("no editor root");
            return info;
            }

      QString sessionFolder = QDir::cleanPath(root + "/.nped");
      QDir dir(sessionFolder);

      QStringList filters;
      filters << "Session-*.json";
      QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::NoSort);
      QFileInfo latestFileInfo;
      for (const QFileInfo& fileInfo : files) {
            QStringList parts = fileInfo.baseName().split('-');
            if (parts.size() < 5) {
                  Debug("bad filename <{}>", fileInfo.baseName());
                  continue;
                  }

            QDate currentDate;
            // Handle both Old (Session-dd-MM-yyyy-n) and New (Session-yy-MM-dd-n) formats
            currentDate = QDate(parts[1].toInt() + 2000, parts[2].toInt(), parts[3].toInt());
            if (!currentDate.isValid()) {
                  Debug("invalid date in filename <{}>", fileInfo.baseName());
                  continue;
                  }

            int currentNumber = parts[4].toInt();

            if (!info.lastDate.isValid() || currentDate > info.lastDate ||
                (currentDate == info.lastDate && currentNumber > info.lastNumber)) {
                  info.lastDate   = currentDate;
                  info.lastNumber = currentNumber;
                  latestFileInfo  = fileInfo;
                  info.fileName   = fileInfo.absoluteFilePath();
                  }
            }
      return info;
      }

//---------------------------------------------------------
//   loadStatus
//    load last session
//---------------------------------------------------------

void Session::load(const QString& sessionPath) {
      QString fileToLoad = sessionPath;

      if (fileToLoad.isEmpty()) {
            auto info  = sessionInfo();
            fileToLoad = info.fileName;
            }

      if (!fileToLoad.isEmpty()) {
            std::ifstream i(fileToLoad.toStdString());
            if (i.is_open()) {
                  std::string content((std::istreambuf_iterator<char>(i)), std::istreambuf_iterator<char>());
                  // Not a valid full JSON object? Try JSON Lines fallback
                  std::istringstream iss(content);
                  std::string line;
                  json h            = json::array();
                  size_t actEntries = 0;
                  while (std::getline(iss, line)) {
                        if (line.empty())
                              continue;
                        try {
                              json obj = json::parse(line);
                              if (obj.contains("model")) {
                                    std::string modelName = obj["model"];
                                    agent->setCurrentModel(QString::fromStdString(modelName), false);
                                    }
                              if (obj.contains("activeEntries"))
                                    actEntries = obj["activeEntries"];
                              if (obj.contains("role") || obj.contains("parts") || obj.contains("content"))
                                    h.push_back(obj);
                              }
                        catch (...) {
                              }
                        }
                  setHistory(h);
                  // Only override activeEntries when an explicit value was found in the file.
                  // If actEntries == 0 (old session format without activeEntries metadata),
                  // setHistory() has already set activeEntries = data.size(), which is correct.
                  if (actEntries > 0)
                        setActiveEntries(actEntries);
                  savedEntries = data().size();
                  _name        = fileToLoad;
                  agent->updateSessionList();
                  agent->updateChatDisplay(true);
                  return;
                  }
            }
      Debug("no session found: start new one");
      // No last session found -> Start new session
      _name        = sessionName(true);
      savedEntries = 0;
      agent->updateSessionList();
      agent->addMessage("system", "<i>[System: No previous session found. New session started.]</i><br>");
      clear();
      }

//-----------------------------------------------------------------------------
//   save
//    save as json lines, so you cannot call json.dump(xx), only json.dump()
//-----------------------------------------------------------------------------

void Session::save() {
      if (_name.isEmpty() || empty())
            return;

      QString path = QFileInfo(_name).absolutePath();
      QDir dir;
      if (!dir.exists(path)) {
            bool created = dir.mkpath(path);
            if (!created) {
                  Critical("Could not create directory {}", path);
                  return;
                  }
            }

      std::ios_base::openmode mode = (savedEntries == 0) ? std::ios::out : std::ios::app;
      std::ofstream f(_name.toStdString(), mode);

      if (f.is_open()) {
            if (savedEntries == 0) {
                  json header;
                  header["model"]         = agent->currentModel().toStdString();
                  header["activeEntries"] = getActiveEntriesCount();
                  f << header.dump() << "\n";
                  }
            if (savedEntries < _data.size()) {
                  if (savedEntries > 0) {
                        json meta;
                        meta["activeEntries"] = getActiveEntriesCount();
                        f << meta.dump() << "\n";
                        }
                  for (size_t i = savedEntries; i < _data.size(); ++i)
                        f << _data[i].content.dump() << "\n";
                  savedEntries = _data.size();
                  }
            f.close();
            }
      else {
            Critical("Could not open session file for writing: {}", _name);
            }
      }

//---------------------------------------------------------
//   optimizeToolResponses
//---------------------------------------------------------

void Session::optimizeToolResponses() {
      int toolAge        = 0;
      size_t tokensSaved = 0;

      for (auto it = _data.rbegin(); it != _data.rend(); ++it) {
            std::string role = it->content.value("role", "");
            if (role == "tool" || role == "function") {
                  int maxLength = 0;
                  if (toolAge == 0)
                        maxLength = 4000;
                  else if (toolAge <= 2)
                        maxLength = 1000;
                  else if (toolAge <= 5)
                        maxLength = 250;
                  else
                        maxLength = 100;

                  if (it->content.contains("content") && it->content["content"].is_string()) {
                        std::string content = it->content["content"].get<std::string>();
                        if (content.length() > static_cast<size_t>(maxLength)) {
                              size_t keepHead       = static_cast<size_t>(maxLength * 0.4);
                              size_t keepTail       = static_cast<size_t>(maxLength * 0.6);
                              size_t truncatedChars = content.length() - keepHead - keepTail;

                              std::string marker =
                                  "\n\n... [" + std::to_string(truncatedChars) + " chars truncated] ...\n\n";
                              std::string newContent = content.substr(0, keepHead) + marker +
                                                       content.substr(content.length() - keepTail);

                              it->content["content"] = newContent;

                              size_t oldTokens = it->tokens;
                              size_t newTokens = newContent.length() / 4;
                              it->tokens       = newTokens;

                              if (oldTokens > newTokens)
                                    tokensSaved += (oldTokens - newTokens);
                              }
                        }
                  toolAge++;
                  }
            }

      if (tokensSaved > 0) {
            if (totalTokens >= tokensSaved)
                  totalTokens -= tokensSaved;
            else
                  totalTokens = 0;
            emit tokensChanged(totalTokens);
            }
      }

//---------------------------------------------------------------------------------------
//   trim
//---------------------------------------------------------------------------------------

bool Session::trim() {
      // 0. Drop redundant repeated tool calls
      std::map<std::string, std::string> toolSignatures;
      for (const auto& item : _data) {
            if (item.content.value("role", "") == "assistant" && item.content.contains("tool_calls")) {
                  for (const auto& tc : item.content["tool_calls"]) {
                        if (tc.contains("id") && tc.contains("function")) {
                              std::string id  = tc["id"].get<std::string>();
                              auto func       = tc["function"];
                              std::string sig = func.value("name", "") + ":";
                              if (func.contains("arguments")) {
                                    if (func["arguments"].is_string())
                                          sig += func["arguments"].get<std::string>();
                                    else
                                          sig += func["arguments"].dump();
                                    }
                              toolSignatures[id] = sig;
                              }
                        }
                  }
            }

      std::set<std::string> seenSignatures;
      for (auto it = _data.rbegin(); it != _data.rend(); ++it) {
            std::string role = it->content.value("role", "");
            if (role == "tool" || role == "function") {
                  std::string id = it->content.value("tool_call_id", "");
                  if (!id.empty() && toolSignatures.count(id)) {
                        std::string sig = toolSignatures[id];
                        if (seenSignatures.count(sig)) {
                              if (it->content.contains("content") && it->content["content"].is_string()) {
                                    std::string oldContent = it->content["content"].get<std::string>();
                                    std::string newContent = "[Repeated tool call omitted to save context]";
                                    if (oldContent != newContent) {
                                          it->content["content"] = newContent;
                                          it->tokens             = newContent.length() / 4;
                                          }
                                    }
                              }
                        else {
                              seenSignatures.insert(sig);
                              }
                        }
                  }
            }

      optimizeToolResponses();

      // Update tokens reflecting the modifications
      setActiveEntries(activeEntries);

      // 1. Wenn der letzte Turn eine Zusammenfassung war:
      // Wir setzen activeEntries auf 2 (Zusammenfassungs-Anfrage und Modell-Antwort).
      if (summaryRequested) {
            Debug("****Summary Requested");
            size_t n = 0;
            for (auto it = _data.rbegin(); it != _data.rend(); ++it) {
                  n++;
                  if (it->content.value("role", "") == "user")
                        break;
                  }
            activeEntries    = n;
            summaryRequested = false;
            setActiveEntries(activeEntries);
            return false;
            }

      // 2. Klassisches Rolling Window mit Berücksichtigung von Größe (totalTokens) und Anzahl (maxEntries)
      //    Safety: never trim below minEntries to guarantee the model always has
      //    some context, even when no "user" boundary is found.
      size_t n = activeEntries;
      while ((activeEntries > maxEntries || totalTokens > criticalTokenCount) && activeEntries > minEntries) {
            activeEntries--;
            while (activeEntries > minEntries) {
                  size_t idx    = _data.size() - activeEntries;
                  std::string r = _data[idx].content.value("role", "");
                  if (r == "user")
                        break;
                  activeEntries--;
                  }
            // Recalculate total tokens for the new active window
            setActiveEntries(activeEntries);
            }
      if (n != activeEntries)
            Debug("****Reduced History from {} to {} entries", n, activeEntries);

      return summaryRequested;
      }

//---------------------------------------------------------
//   addResult
//---------------------------------------------------------

bool Session::addResult(const json& content, size_t tokens) {
      _data.push_back({content, tokens});
      totalTokens += tokens;
      activeEntries++;
      bool needSummary = trim();
      emit tokensChanged(totalTokens);
      return needSummary;
      }

//---------------------------------------------------------
//   setHistory
//---------------------------------------------------------

void Session::setHistory(const json& h) {
      clear();
      for (const auto& item : h) {
            // Approximation: 4 chars per token
            size_t tokens = 0;
            if (item.contains("parts")) {
                  for (const auto& part : item["parts"])
                        if (part.contains("text"))
                              tokens += part["text"].get<std::string>().length() / 4;
                  }
            _data.push_back({item, tokens});
            // note: totalTokens will be recomputed by the caller using setActiveEntries,
            // or we assume all are active initially for backward compatibility
            totalTokens += tokens;
            activeEntries++;
            }
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   setActiveEntries
//---------------------------------------------------------

void Session::setActiveEntries(size_t a) {
      activeEntries   = std::min(a, _data.size());
      totalTokens     = 0;
      size_t startIdx = _data.size() - activeEntries;

      if (startIdx > 0 && !_data.empty())
            totalTokens += _data[0].tokens;

      for (size_t i = startIdx; i < _data.size(); ++i)
            totalTokens += _data[i].tokens;
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   getActiveEntries
//---------------------------------------------------------

json Session::getActiveEntries() const {
      json arr        = json::array();
      size_t startIdx = _data.size() > activeEntries ? _data.size() - activeEntries : 0;

      // Always include the very first user request if history is truncated
      if (startIdx > 0 && !_data.empty())
            arr.push_back(_data[0].content);

      for (size_t i = startIdx; i < _data.size(); ++i)
            arr.push_back(_data[i].content);

      return arr;
      }

//---------------------------------------------------------
//   addRequest
//---------------------------------------------------------

void Session::addRequest(json content, size_t tokens) {
      _data.push_back({content, tokens});
      totalTokens += tokens;
      activeEntries++;
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void Session::clear() {
      _data.clear();
      totalTokens   = 0;
      activeEntries = 0;
      savedEntries  = 0;
      emit tokensChanged(totalTokens);
      }

//---------------------------------------------------------
//   sessionName
//---------------------------------------------------------

QString Session::sessionName(bool getNext) const {
      SessionInfo info = sessionInfo();
      QDate today      = QDate::currentDate();

      int nextNumber = info.lastNumber + 1; // default: increment
      if (!getNext && info.lastNumber != 0)
            nextNumber = info.lastNumber; // reuse existing number
      else
            nextNumber = info.lastNumber + 1;

      // old Format: Session-dd-MM-yyyy-n.json
      // new Format: Session-yy-MM-dd-n.json

      QString root = agent->editor()->projectRoot();
      if (root.isEmpty()) // no session, no session info
            return QString();

      QString sessionFolder = QDir::cleanPath(root + "/.nped");
      return QString("%1/Session-%2-%3.json")
          .arg(sessionFolder)
          .arg(today.toString("yy-MM-dd"))
          .arg(nextNumber);
      }
