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

#include <thread>
#include <vector>
#include <map>
#include <string>

#include "logger.h"
#include "file.h"
#include "types.h"

struct LScapabilities {
      };

//---------------------------------------------------------
//   LSclient
//---------------------------------------------------------

class LSclient : public QObject
      {
      Q_OBJECT
      Q_DISABLE_COPY_MOVE(LSclient)

      std::string _name;
      std::string _path;
      int id{1}; // current id

      std::map<int, Callback> callbacks;

      int stdinPipe[2], stdoutPipe[2], stderrPipe[2];
      int stopFd;
      std::thread* reader;
      std::atomic<bool> running{false};

      LScapabilities scap; // server capabilities
      LScapabilities ccap; // client capabilities

      Editor* editor;
      bool _initialized{false};

      void readerLoop();
      void processMessage(const std::string& message);

      bool write(const std::string& txt);
      bool writeMessage(const std::string& jsonPayload);

      void gotoRequest(const char* req, File* file, const Pos& cursor);
      void handleDiagnostics(const json& params);
      void astResponse(File* f, const json& msg);

      bool notification(const char* method, const json& params = json::object());
      bool request(const char* method, const json& params = json::object());

    private slots:
      void handleNotification(json msg);
      void handleResponse(int id, json msg);

    signals:
      void initializedChanged();
      void notificationReceived(json msg);
      void responseReceived(int id, json msg);
      void symbolSearchResult(const QString& result);

    public:
      explicit LSclient(Editor* e, const std::string& name);
      ~LSclient() override { stop(); }
      bool start(const std::string& path, const std::vector<std::string>& args);

      void stop();
      std::string name() const { return _name; }
      bool initializeRequest();
      bool initialized() const { return _initialized; }
      bool astProvider() const { return true; }

      void prepareRenameRequest(Kontext*);
      void renameRequest(Kontext*, const QString&, int, int);

      void completionRequest(Kontext*);
      void astRequest(File* file);
      void formattingRequest(Kontext*);

      bool didOpenNotification(File* file);
      bool didCloseNotification(File* file);
      bool didChangeNotification(File* file, const Patches& patches);
      void newDocument(const File*) {}
      void documentWillSave(const File*) {}
      void documentSaved(const File*) {}
      void gotoDefinition(File*, const Pos&);
      void gotoTypeDefinition(File* file, const Pos& cursor);
      void gotoImplementation(File* file, const Pos& cursor);
      void hover(File* file, const Pos& cursor);
      void symbolRequest(const QString& symbol);

      static LSclient* createClient(Editor* editor, const std::string& name);
      };
