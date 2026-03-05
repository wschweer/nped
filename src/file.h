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

#include "ast.h"
#include "git.h"
#include "line.h"
#include "types.h"

class Editor;
class Kontext;
enum class ViewMode;
class UndoStack;
class LSclient;
class File;

//---------------------------------------------------------
//   FileType
//---------------------------------------------------------

struct FileType {
      std::vector<const char*> extensions;
      const char* languageId; // language id to select the right language server
      const char* languageServer;
      int tab;         // tab expansion
      bool parse;      // connect to language server
      bool header;     // special handling of header files
      bool createTabs; // spaces are converted to tabs when writing the file
                       // on reading tabs are converted to spaces
      bool pymacros;   // handle internal macro expansion
      };

//---------------------------------------------------------
//   fileTypes
//    order matters
//---------------------------------------------------------

static const std::array<const FileType, 9> fileTypes = {
      FileType({".*\\.cpp"},     "cpp",      "clangd",   6,    true,  false, false, false),
      FileType({".*\\.c"},       "c",        "clangd",   6,    true,  false, false, false),
      FileType({".*\\.html"},    "html",     "vscode-html",   4,    false, false, false, false),
      FileType({".*\\.h"},       "cpp",      "clangd",   6,    true,  true,  false, true),
      FileType({".*\\.py"},      "python",   "pylsp",    6,    false, false, false, false),
      FileType({".*\\.qml"},     "qml",      "qmlls",    4,    false, false, false, false),
      FileType({".*\\.md"},      "markdown", "none",     6,    false, false, false, false),
      FileType({".*\\.html"},    "html",     "none",     6,    false, false, false, false),
      FileType({"Makefile"},     "makefile", "none",     6,    false, false, true,  false),
      };

static constexpr FileType defaultFileType = {
   std::vector<const char*>(), "", "none", 6, false, false, false, false};

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

      ViewMode _viewMode;
      Lines _kollaps;
      Lines _bugs;
      Lines _searchResults;
      Lines _fileText;
      Lines _gitVersion;

      std::vector<GitHistory*> _gitHistory;
      int _currentGitHistory { 0 };

      QString _languageId{"c++"};
      int _version{1};
      const FileType* fileType = &defaultFileType;

      QFile::Permissions mode{QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup |
                              QFile::ReadOther};
      QFileInfo _fi;
      int referenceCount{0};
      ASTNode astTopNode;
      LSclient* client{nullptr};

      int toOffset(const Pos&);
      void updateKollaps();
      void dumpLine(Lines& lines, int level, int clevel, const ASTNode& node) const;
      void lcOpen();
      bool levelcount(const Pos& p, int* level, const ASTNode& node) const;
      void markExpansion();
      bool posValid(const Pos& pos) const;

    signals:
      void fileChanged();           // trigger language server update?
      void cursorChanged(Cursor);   // undo requests a new kontext cursor position
      void modifiedChanged();       // signal to tabBar

    public slots:
      void toggleFold(int row);

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
      bool mustUpdateLS() const { return fileType->parse; }
      QString languageId() const { return fileType->languageId; }
      int tab() const { return fileType->tab; }
      bool pyMacros() const { return fileType->pymacros; }
      QString plainText() const { return _fileText.join('\n'); }
      int indent(const Pos& pos) const;

      void setKollaps(const Lines& map);
      void setBugs(const Lines& map);
      const Lines& kollaps() const { return _kollaps; }
      const Lines& bugs() const { return _bugs; }

      const std::vector<GitHistory*>& gitHistory() const { return _gitHistory; }
      std::vector<GitHistory*>& gitHistory() { return _gitHistory; }
      const GitHistory* gitHistory(int idx) const { return _gitHistory[idx]; }
      Lines& gitVersion() { return _gitVersion; }

      const Lines& searchResults() const { return _searchResults; }
      Lines& searchResults() { return _searchResults; }
      void setSearchResults(const Lines& map);
      const Lines& fileText() const { return _fileText; }
      const Line& fileText(int row) const;
      const Line& line(int y) const;
      int version() const { return _version; }
      void setAST(const ASTNode& node);
      void setLabel(int y, QChar c, QColor color = QColorConstants::Black);
      void clearLabel();

      int distance(const Pos&, const Pos&) const;
      int columns(int y) const { return (y < rows()) ? line(y).size() : 0; }
      int rows() const;
      bool parse() const { return fileType->parse; }
      LSclient* languageClient() { return client; }
      void setLSclient(LSclient* c) { client = c; }
      // editing:
      void patch(Patches& items);
      Pos advance(const Pos& p, int dist) const;

      bool readOnly() const;
      void expandMacros();
      void postprocessFormat();
      void updateAST();

      bool folded(int row) const;
      void setFoldFlag(int row, bool folded);
      bool isFoldable(int row) const;

      void makePretty();
      void markCpp();
      void addMark(int row, int idx1, int idx2, Marker m);
      bool clearSearchMarks();
      void showGitVersion(int);
      int currentGitHistory() const { return _currentGitHistory; }
      void foldAll(bool);
      void unfold(int row);
      int nextRow(int row) const;
      int nextRowIfAvailable(int row) const { int nrow = nextRow(row); return (nrow == -1) ? row : nrow; }
      int previousRow(int row) const;
      int previousRowIfAvailable(int row) const { int prow = previousRow(row); return (prow == -1) ? row : prow; }

      bool searchReplace(const QString& search, const QString& replaceText);
      };
