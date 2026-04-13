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

//---------------------------------------------------------
//   LScapabilities
//---------------------------------------------------------

struct LScapabilities {
      bool astProvider{false};
      bool documentFormattingProvider{false};
      bool renameProvider{false};
      bool hoverProvider{false};
      bool definitionProvider{false};
      bool completionProvider{false};
      void read(const json& cap) {
            astProvider                = cap.value("astProvider", false);
#if 0
            documentFormattingProvider = cap.value("documentFormattingProvider", false);
            renameProvider             = cap.value("renameProvider", false);
            hoverProvider              = cap.value("hoverProvider", false);
            definitionProvider         = cap.value("definitionProvider", false);
            completionProvider         = cap.contains("completionProvider");
#endif
            }
      };

//---------------------------------------------------------
//   LSclient
//---------------------------------------------------------

class LSclient : public QObject
      {
      Q_OBJECT
      Q_DISABLE_COPY_MOVE(LSclient)

      static constexpr int maxCompletions { 10 };

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
      bool processMessage(const std::string& message);

      bool write(const std::string& txt);
      bool writeMessage(const std::string& jsonPayload);

      void gotoRequest(const char* req, File* file, const Pos& cursor);
      void handleDiagnostics(const json& params);

      bool notification(const char* method, const json& params = json::object());
      bool request(const char* method, const json& params = json::object());

    private slots:
      void handleNotification(json msg);
      void handleResponse(int id, json msg);

    signals:
      void initializedChanged();
      void notificationReceived(json msg);
      void responseReceived(int id, json msg);
      void symbolSearchResult(const std::string& result);
      void referencesSearchResult(const std::string& result);
      void isRunning();

   public slots:
      bool initializeRequest();

    public:
      explicit LSclient(Editor* e, const std::string& name);
      ~LSclient() override;
      bool start(const std::string& path, const std::vector<std::string>& args);

      void stop();
      std::string name() const { return _name; }
      bool initialized() const { return _initialized; }
      bool astProvider() const { return scap.astProvider; }
      void prepareRenameRequest(Kontext*);
      void renameRequest(Kontext*, const QString&, int, int);

      void completionRequest(Kontext*);
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
      void documentSymbolRequest(File*);
      void referencesRequest(const QString& file, int line, int col);
      void indentRequest(const File*, int line);

      static LSclient* createClient(Editor* editor, const std::string& name);
      };
