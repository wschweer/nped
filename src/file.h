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

#include <array>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QPoint>
#include <QColor>

#include "git.h"
#include "line.h"
#include "types.h"
#include "filetype.h"

class Editor;
class Kontext;
enum class ViewMode;
class UndoStack;
class LSclient;
class File;


enum class Codec { ISO_LATIN, UTF8 };

//---------------------------------------------------------
//   File
//---------------------------------------------------------

class File : public QObject
      {
      Q_OBJECT

      bool created{true};
      bool _readOnly{false};
      UndoStack* _undo;
      QFile f;

      json _symbols;

      Lines _outline;
      Lines _bugs;
      Lines _searchResults;
      Lines _fileText;
      Lines _gitVersion;
      int _currentGitHistory{0};

      std::vector<GitHistory*> _gitHistory;

      int _version{1};
      FileType fileType = defaultFileType;

      QFile::Permissions mode{QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther};
      QFileInfo _fi;
      int referenceCount{0};
      LSclient* client{nullptr};

      int toOffset(const Pos&);
      void lcOpen();
      void markExpansion();

    signals:
      void fileChanged();         // trigger language server update?
      void cursorChanged(Cursor); // undo requests a new kontext cursor position
      void modifiedChanged();     // signal to tabBar
      void symbolsReady();        // symbols arrived from language server

    public slots:
      void toggleFold(int row);
      void onFileChangedOnDisk(const QString& path);

    public:
      File(Editor* e, const QFileInfo& f);
      ~File();
      Editor* editor;
      UndoStack* undo() { return _undo; }
      bool modified() const;
      bool load();
      bool save();
      bool dereference() { return --referenceCount <= 0; }
      void reference() { referenceCount += 1; }
      QString path() const { return _fi.absoluteFilePath(); }
      QString fileName() const { return _fi.fileName(); }
      QFileInfo fi() const { return _fi; }
      void setViewMode(ViewMode, const Pos& pt = Pos());
      QString languageId() const { return fileType.languageId; }
      int tab() const { return fileType.tabSize; }
      QString plainText() const { return _fileText.join('\n'); }
      int indent(const Pos& pos) const;
      void updateOutline();

      void setBugs(const Lines& map);
      const Lines& outline() const { return _outline; }
      const Lines& bugs() const { return _bugs; }
      const std::vector<GitHistory*>& gitHistory() const { return _gitHistory; }
      std::vector<GitHistory*>& gitHistory() { return _gitHistory; }
      const GitHistory* gitHistory(int idx) const { return _gitHistory[idx]; }
      Lines& gitVersion() { return _gitVersion; }
      const Lines& gitVersion() const { return _gitVersion; }
      void setGitVersion(const Lines& l) { _gitVersion = l; }
      const Lines& searchResults() const { return _searchResults; }
      Lines& searchResults() { return _searchResults; }
      void setSearchResults(const Lines& map);
      const Lines& fileText() const { return _fileText; }
      const Line& fileText(int row) const;
      int version() const { return _version; }
      void setLabel(int y, QChar c, QColor color = QColorConstants::Black);
      void clearLabel();

      int distance(Pos start, Pos end) const;
      int columns(int y) const { return (y < fileRows()) ? fileLine(y).size() : 0; }      // TODO
      LSclient* languageClient() { return client; }
      void setLSclient(LSclient* c) { client = c; }
      // editing:
      void patch(Patches& items);
      Pos advance(const Pos& p, int dist) const;

      int fileRows() const { return _fileText.size(); }
      const Line& fileLine(int row) const { return _fileText.at(row); }

      bool readOnly() const;
      void postprocessFormat();

      void setFoldFlag(int row, bool folded);
      bool isFoldable(int row) const;

      void makePretty();
      void markCpp();
      void addMark(int row, int idx1, int idx2, TextStyle::Style m);
      bool clearSearchMarks();
      void showGitVersion(int);
      int currentGitHistory() const { return _currentGitHistory; }
      void setCurrentGitHistory(int n) { _currentGitHistory = n; }
      void foldAll(bool);
      void unfold(int row);
      bool searchReplace(const QString& search, const QString& replaceText);
      void setSymbols(const json& j);
      const json& symbols() const { return _symbols; }
      };
