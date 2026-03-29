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

#include <QStringList>
#include <QRect>

#include "file.h"

class Editor;
class File;
class Kontext;
class QPoint;

//---------------------------------------------------------
//   ViewMode
//---------------------------------------------------------

enum class ViewMode {
      File,      // text source
      Functions, // filter functions and methods
      Bugs,      // ls annotations
      GitVersion,
      SearchResults,
      WebView
      };

//---------------------------------------------------------
//   MoveType
//---------------------------------------------------------

enum class MoveType {
      Normal, // move FilePos + ScreenPos, adjust ScreenPos
      Roll,   // move ScreenPos, adjust FilePos
      Page    // move FilePos, ScreenPos is the same
      };

static const int ContextLines = 1;

//---------------------------------------------------------
//   Kontext
//---------------------------------------------------------

class Kontext : public QObject
      {
      Q_OBJECT

      File* _file;
      Cursor _cursor;

      Selection _selection;
      ViewMode _viewMode{ViewMode::File};

      void switchToLineMap(const Lines& map);
      int screenLines() const;
      int screenRows() const;
      int screenColumns() const;
      void fixCursor();
      //
    signals:
      void cursorChanged();

    public:
      Editor* editor;
      Kontext(Editor* e, File* f);
      File* file() { return _file; }
      const File* file() const { return _file; }
      ViewMode viewMode() const { return _viewMode; }
      int moveRow(int row, int delta, int* screenDy = nullptr) const;
      int screenCol() const { return _cursor.screenPos.col; }
      int screenRow() const { return _cursor.screenPos.row; }
      int fileCol() const { return _cursor.filePos.col; }
      int fileRow() const { return _cursor.filePos.row; }
      int columns() const; // number of columns in current text line
      int rows() const;    // number of lines in File
      Pos screenPos() const { return _cursor.screenPos; }
      Pos& screenPos() { return _cursor.screenPos; }
      void setScreenPos(const Pos& p) { _cursor.screenPos = p; }
      void setFilePos(const Pos& p) { _cursor.filePos = p; }
      Pos filePos() const { return _cursor.filePos; }
      Pos& filePos() { return _cursor.filePos; }
      Cursor& cursor() { return _cursor; }
      const Cursor& cursor() const { return _cursor; }
      int screenColumnOffset() const { return fileCol() - screenCol(); }
      int screenRowOffset() const { return fileRow() - screenRow(); }
      bool showCursor() const { return true; }
      void movePrevWord();
      void moveNextWord();
      const QString& currentLine() const;
      void flipSelectionCursor();

      Selection& selection() { return _selection; }
      QString selectionText();
      const Cursor& selectionStartCursor() const { return _selection.cursor; }
      QRect selectionRect() const;
      void setSelection(const QRect& r);
      void setSelectionMode(SelectionMode sm);
      void updateSelection();
      Pos startSelect() const { return _selection.start; }
      Pos& startSelect() { return _selection.start; }
      Pos endSelect() const { return _selection.end; }
      Pos& endSelect() { return _selection.end; }

      int c_compound();
      void setViewMode(ViewMode mode);
      void createFunction(const QString&);
      void createFunctionHeader();
      bool atLineEnd() const;
      void gotoLine(const QString&);
      bool cursorValid() const { return _cursor.filePos.row >= 0 && _cursor.filePos.row < _file->rows(); }
      void moveCursorRel(int dx, int dy, MoveType moveType = MoveType::Normal);
      void moveCursorAbs(int col, int row);
      void setCursorAbs(const Cursor&);
      void moveCursorTopLine();
      void moveCursorBottomLine();
      void fixScreenCursor(Cursor& c);
      void toggleViewMode();
      };
