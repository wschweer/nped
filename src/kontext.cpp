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

#include "logger.h"
#include "kontext.h"
#include "editor.h"
#include "editwin.h"
#include "undo.h"
// #include "lsclient.h"

//---------------------------------------------------------
//   Kontext
//---------------------------------------------------------

Kontext::Kontext(Editor* e, File* f) : _file(f), editor(e) {
      _selection.mode = SelectionMode::NoSelect;
      _file->reference();
      }

//---------------------------------------------------------
//   setSelectionMode
//---------------------------------------------------------

void Kontext::setSelectionMode(SelectionMode sm) {
      _selection.mode = sm;

      if (sm != SelectionMode::NoSelect)
            _selection.cursor = _cursor;
      }

//---------------------------------------------------------
//   atLineEnd
//---------------------------------------------------------

bool Kontext::atLineEnd() const {
      //      const Lines& t = text();
      if (_cursor.filePos.row >= rows() || _cursor.filePos.row < 0)
            return true;
      int n = file()->columns(_cursor.filePos.row);
      if (_cursor.filePos.col >= n)
            return true;
      return false;
      }

//---------------------------------------------------------
//   screenRows
//---------------------------------------------------------

int Kontext::screenRows() const {
      return editor->editWidget()->rows();
      }

//---------------------------------------------------------
//   screenColumns
//---------------------------------------------------------

int Kontext::screenColumns() const {
      return editor->editWidget()->columns();
      }

//---------------------------------------------------------
//   moveRow
//---------------------------------------------------------

int Kontext::moveRow(int row, int delta, int* screenDy) const {
      if (screenDy)
            *screenDy = 0;
      while (delta > 0 && row < rows() - 1) {
            --delta;
            ++row;
            if (screenDy)
                  *screenDy += 1;
            while (folded(row) && row < rows() - 1)
                  ++row;
            }
      while (delta < 0 && row > 0) {
            ++delta;
            if (row > 0) {
                  --row;
                  if (screenDy)
                        *screenDy -= 1;
                  }
            while (folded(row) && row > 0)
                  --row;
            }
      return row;
      }

//---------------------------------------------------------
//   updateSelection
//---------------------------------------------------------

void Kontext::updateSelection() {
      _selection.end = _cursor.filePos;
      if (endSelect().row >= rows())
            endSelect().row = rows() - 1;
      }

//---------------------------------------------------------
//   flipSelectionCursor
//---------------------------------------------------------

void Kontext::flipSelectionCursor() {
      _selection.flip();
      setCursorAbs(Cursor(_selection.end, Pos(_selection.end.col, _cursor.screenPos.row)));
      }

//---------------------------------------------------------
//   selectionRect
//---------------------------------------------------------

QRect Kontext::selectionRect() const {
      int x = std::min(startSelect().col, endSelect().col);
      int y = std::min(startSelect().row, endSelect().row);
      int w = std::abs(startSelect().col - endSelect().col) + 1;
      int h = std::abs(startSelect().row - endSelect().row) + 1;
      return QRect(x, y, w, h);
      }

//---------------------------------------------------------
//   setSelection
//---------------------------------------------------------

void Kontext::setSelection(const QRect& r) {
      //      QRect nr         = r.normalized();
      _selection.start = r.topLeft();
      _selection.end   = r.bottomRight();
      if (startSelect().row >= rows())
            startSelect().row = rows() - 1;
      if (endSelect().row >= rows())
            endSelect().row = rows() - 1;
      }

//---------------------------------------------------------
//   screenLines
//---------------------------------------------------------

int Kontext::screenLines() const {
      return editor->editWidget()->textSize().height();
      }

//---------------------------------------------------------
//   movePrevWord
//---------------------------------------------------------

void Kontext::movePrevWord() {
      const Line& s = line(_cursor.filePos.row);
      int n         = s.size();
      int i         = _cursor.filePos.col;
      if (i >= n)
            i = n;
      if (i == 0)
            return;

      if ((i == n) || (!s[i - 1].isSpace() && s[i].isSpace()))
            --i;
      else if (s[i - 1].isSpace() && !s[i].isSpace())
            --i;
      if (!s[i].isSpace()) {
            while (i && !s[i].isSpace())
                  --i;
            }
      else {
            while (i && s[i].isSpace())
                  --i;
            while (i && !s[i].isSpace())
                  --i;
            }
      if (i > 0)
            ++i;
      moveCursorRel(i - _cursor.filePos.col, 0);
      }

//---------------------------------------------------------
//   trenner
//    is word separator char
//---------------------------------------------------------

static bool trenner(QChar c) {
      return c.isSpace() || c == '.';
      }

//---------------------------------------------------------
//   moveNextWord
//---------------------------------------------------------

void Kontext::moveNextWord() {
      //      const auto& t = text();
      if (_cursor.filePos.row >= rows()) {
            Debug("no text");
            return;
            }
      const Line& s = line(_cursor.filePos.row);
      int n         = s.size();

      int i = _cursor.filePos.col;
      if (i >= n)
            return;

      if (trenner(s[i])) {
            while (i < n && trenner(s[i]))
                  ++i;
            }
      else {
            while (i < n && !trenner(s[i]))
                  ++i;
            while (i < n && trenner(s[i]))
                  ++i;
            }
      moveCursorRel(i - _cursor.filePos.col, 0);
      }

//---------------------------------------------------------
//   currentLine
//---------------------------------------------------------

const QString& Kontext::currentLine() const {
      return line(_cursor.filePos.row).qstring();
      }

//---------------------------------------------------------
//   selectionText
//---------------------------------------------------------

QString Kontext::selectionText() {
      QString s;

      switch (_selection.mode) {
            case SelectionMode::RowSelect: {
                  QStringList sl;
                  for (int i = 0; i < selection().height(); ++i)
                        sl << line(selection().y() + i).qstring();
                  s  = sl.join('\n');
                  s += "\n";
                  } break;

            case SelectionMode::CharSelect: {
                  Pos p1 = startSelect();
                  Pos p2 = endSelect();
                  if (p1.row > p2.row || (p1.row == p2.row && p1.col > p2.col))
                        std::swap(p1, p2);

                  if (p1.row == p2.row) {
                        s = line(p1.row).qstring().mid(p1.col, p2.col - p1.col + 1);
                        }
                  else {
                        s = line(p1.row).qstring().mid(p1.col) + "\n";
                        for (int i = p1.row + 1; i < p2.row; ++i)
                              s += line(i).qstring() + "\n";
                        s += line(p2.row).qstring().left(p2.col + 1);
                        }
                  } break;

            case SelectionMode::ColSelect:
                  for (int i = selection().y(); i < selection().y() + selection().height(); ++i) {
                        QString ss = line(i).mid(selection().x(), selection().width());
                        while (ss.size() < selection().width())
                              ss += " ";
                        s += (ss + "\n");
                        }
                  break;
            case SelectionMode::NoSelect: break;
            }
      // Debug("<{}>", s);
      return s;
      }

//---------------------------------------------------------
//   createFunction
//    this macro function creates a function body:
//
//    //---------------------------------------------------------
//    //   mops
//    //---------------------------------------------------------
//
//    void mops() {
//          <cursor>
//          }
//
//---------------------------------------------------------

void Kontext::createFunction(const QString& s) {
      Debug("{}", s);
      //      int x      = _cursor.filePos.col;
      int y             = _cursor.filePos.row;
      QString prototype = QString("//---------------------------------------------------------\n"
                                  "//   %1\n"
                                  "//---------------------------------------------------------\n"
                                  "\n"
                                  "void %1() {\n"
                                  "\n"
                                  "      }\n")
                              .arg(s);

      if (y < rows() && !line(y).empty())
            prototype += "\n";
      int off = 5;
      if (y > 2) {
            if (!line(y - 1).empty()) {
                  prototype.push_front("\n");
                  ++off;
                  }
            }
      // move the cursor into the body of the function
      Cursor c1(Pos(6, y + off), Pos(6, -1));
      file()->undo()->push(new Patch(file(), Pos(0, y), 0, prototype, c1, _cursor));
      }

//---------------------------------------------------------
//   createFunctionHeader
//---------------------------------------------------------

void Kontext::createFunctionHeader() {
      if (editor->endSelectionMode())
            return;
      QString s = editor->pickWord();
      if (s.isEmpty())
            return;
      int x      = _cursor.filePos.col;
      int y      = _cursor.filePos.row;
      QString ss = QString("//---------------------------------------------------------\n"
                           "//   %1\n"
                           "//---------------------------------------------------------\n")
                       .arg(s);

      if (y < rows() && !line(y).empty())
            ss += "\n";
      int off = 4;
      if (y > 2) {
            if (!line(y - 1).empty()) {
                  ss.push_front("\n");
                  ++off;
                  }
            }
      editor->undoPatch(Pos(0, y), 0, ss, Cursor(Pos(x, y + off), Pos(-1, -1)), _cursor);
      editor->update();
      }

//---------------------------------------------------------
//   gotoLine
//---------------------------------------------------------

void Kontext::gotoLine(const QString& s) {
      int line = s.toInt() - 1;
      moveCursorAbs(-1, line);
      }

//---------------------------------------------------------
//   columns
//---------------------------------------------------------

int Kontext::columns() const {
      return line(fileRow()).size();
      }

//---------------------------------------------------------
//   fixCursor
//---------------------------------------------------------

void Kontext::fixCursor() {
      int lastScreenRow   = screenRows();
      lastScreenRow      -= _cursor.fileRow() >= rows() - 2 ? 1 : 1 + ContextLines;
      int firstScreenRow  = _cursor.fileRow() == 0 ? 0 : ContextLines;
      if (_cursor.screenPos.row > lastScreenRow)
            _cursor.screenPos.row = lastScreenRow;
      else if (_cursor.screenPos.row < firstScreenRow)
            _cursor.screenPos.row = firstScreenRow;
      _cursor.screenPos.col = std::clamp(_cursor.screenPos.col, 0, screenColumns());
      _cursor.filePos.row   = std::max(0, _cursor.filePos.row);
      _cursor.filePos.col   = std::max(0, _cursor.filePos.col);
      updateSelection();
      emit cursorChanged();
      editor->update();
      }

//---------------------------------------------------------
//   fixScreenCursor
//---------------------------------------------------------

void Kontext::fixScreenCursor(Cursor& c) {
      int lastScreenRow  = screenRows() - (c.fileRow() >= rows() - 2 ? 1 : 1 + ContextLines);
      int firstScreenRow = c.fileRow() == 0 ? 0 : ContextLines;

      if (c.screenPos.row > lastScreenRow)
            c.screenPos.row = lastScreenRow;
      else if (c.screenPos.row < firstScreenRow)
            c.screenPos.row = firstScreenRow;
      if (c.screenPos.col < 0)
            c.screenPos.col = 0;
      if (c.screenPos.col >= screenColumns())
            c.screenPos.col = screenColumns() - 1;
      }

//---------------------------------------------------------
//   moveCursorRel
//    bool roll   default: false
//---------------------------------------------------------

void Kontext::moveCursorRel(int dx, int dy, MoveType type) {
      int lastRow = rows() - 1;
      if (type == MoveType::Page) {
            while (dy > 0 && _cursor.filePos.row < lastRow) {
                  --dy;
                  int nrow = nextRow(_cursor.filePos.row);
                  if (nrow != -1)
                        _cursor.filePos.row = nrow;
                  }
            while (dy < 0 && _cursor.filePos.row > 0) {
                  ++dy;
                  int nrow = previousRow(_cursor.filePos.row);
                  if (nrow != -1)
                        _cursor.filePos.row = nrow;
                  }
            _cursor.filePos.col += dx;

            // increase context:
            int k = 3;
            if (screenRows() >= k * 2)
                  _cursor.screenPos.row = std::clamp(_cursor.screenPos.row, k, screenRows() - k);
            }
      if (type == MoveType::Roll) {
            _cursor.screenPos.row += dy;
            _cursor.screenPos.col += dx;
            int lastScreenRow      = screenRows() - (_cursor.fileRow() >= rows() - 2 ? 1 : 1 + ContextLines);
            int firstScreenRow     = _cursor.fileRow() == 0 ? 0 : ContextLines;

            if (_cursor.screenPos.row > lastScreenRow) {
                  int n = _cursor.screenPos.row - lastScreenRow;
                  while (n--)
                        _cursor.filePos.row = previousRowIfAvailable(_cursor.filePos.row);
                  _cursor.screenPos.row = lastScreenRow;
                  }
            else if (_cursor.screenPos.row < firstScreenRow) {
                  int n = firstScreenRow - _cursor.screenPos.row;
                  while (n--)
                        _cursor.filePos.row = nextRowIfAvailable(_cursor.filePos.row);
                  _cursor.screenPos.row = firstScreenRow;
                  }
            }
      else if (type == MoveType::Normal) {
            int row            = fileRow();
            int screenPosDelta = 0;
            while (dy > 0 && row < lastRow) {
                  --dy;
                  ++screenPosDelta;
                  int nrow = nextRow(row);
                  if (nrow != -1)
                        row = nrow;
                  }
            while (dy < 0 && row > 0) {
                  ++dy;
                  --screenPosDelta;
                  int nrow = previousRow(row);
                  if (nrow != -1)
                        row = nrow;
                  }
            _cursor.screenPos.row += screenPosDelta;
            _cursor.filePos.row    = row;
            _cursor.filePos.col   += dx;
            _cursor.screenPos.col += dx;
            }
      fixCursor();
      }

//---------------------------------------------------------
//   moveCursorAbs
//    if col < 0 then ignore
//    if row < 0 then ignore
//---------------------------------------------------------

void Kontext::moveCursorAbs(int col, int row) {
      if (col >= 0) {
            _cursor.filePos.col   = std::max(0, col);
            _cursor.screenPos.col = std::clamp(col, 0, screenColumns());
            }
      if (row >= 0) {
            file()->unfold(row);
            _cursor.filePos.row = row;

            // We want to move the cursor without scrolling the screen
            // if possible.
            }
      fixCursor();
      }

//---------------------------------------------------------
//   setCursorAbs
//---------------------------------------------------------

void Kontext::setCursorAbs(const Cursor& c) {
      if (c.filePos.col >= 0)
            _cursor.filePos.col = c.filePos.col;
      if (c.filePos.row >= 0)
            _cursor.filePos.row = c.filePos.row;
      if (c.screenPos.col >= 0)
            _cursor.screenPos.col = c.screenPos.col;
      if (c.screenPos.row >= 0)
            _cursor.screenPos.row = c.screenPos.row;
      _cursor.filePos.row   = std::clamp(_cursor.filePos.row, 0, rows() - 1);
      _cursor.filePos.col   = std::max(0, _cursor.filePos.col);
      _cursor.screenPos.row = std::clamp(_cursor.screenPos.row, 0, screenRows());
      _cursor.screenPos.col = std::clamp(_cursor.screenPos.col, 0, screenColumns());
      updateSelection();
      emit cursorChanged();
      editor->update();
      }

//---------------------------------------------------------
//   moveCursorTopLine
//---------------------------------------------------------

void Kontext::moveCursorTopLine() {
      int n = -screenRow() + ContextLines;
      moveCursorRel(0, n);
      }

//---------------------------------------------------------
//   moveCursorBottomLine
//---------------------------------------------------------

void Kontext::moveCursorBottomLine() {
      int n = screenLines() - screenRow() - ContextLines - 1;
      moveCursorRel(0, n);
      }

//---------------------------------------------------------
//   switchToLineMap
//    move to the right line in map
//    p   - current cursor position
//    map - line list
//---------------------------------------------------------

void Kontext::switchToLineMap(const Lines& map) {
      int i   = 0;
      int row = _cursor.filePos.row;
      //      Debug("row {} size {}", row, map.size());
      for (; i < int(map.size()); ++i) {
            int y = map[i].tag().row;
            //            Debug("{} -- {}", y, row);
            if (y == row)
                  break;
            if (y > row) {
                  i = std::max(0, i - 1);
                  break;
                  }
            }
      moveCursorAbs(0, i);
      }

//---------------------------------------------------------
//   setViewMode
//    map cursorposition between different views if
//    possible
//---------------------------------------------------------

void Kontext::setViewMode(ViewMode m) {
      ViewMode old = _viewMode;

      _viewMode = m;
      switch (_viewMode) {
            case ViewMode::File: {
                  int row;
                  const Lines* map{nullptr};
                  switch (old) {
                        case ViewMode::Outline:
                              //
                              map = &file()->outline();
                              break;
                        case ViewMode::Annotations: //
                              map = &file()->bugs();
                              break;
                        case ViewMode::SearchResults: //
                              map = &file()->searchResults();
                              break;
                        case ViewMode::GitVersion: break;
                        case ViewMode::GitDiff: break;
                        case ViewMode::File: break;    // cannot happen
                        case ViewMode::WebView: break; // cannot happen
                        }
                  if (map) {
                        if (_cursor.filePos.row >= map->size())
                              row = 0;
                        else
                              row = map->at(_cursor.filePos.row).tag().row;
                        moveCursorAbs(0, row);
                        }
                  } break;

            case ViewMode::Outline: switchToLineMap(file()->outline()); break;
            case ViewMode::Annotations: switchToLineMap(file()->bugs()); break;
            case ViewMode::SearchResults: switchToLineMap(file()->searchResults()); break;
            case ViewMode::GitVersion: break;
            case ViewMode::GitDiff: break;
            case ViewMode::WebView: break;
            }
      updateSelection();
      editor->update();
      }

//---------------------------------------------------------
//   line
//---------------------------------------------------------

const Line& Kontext::line(int row) const {
      const Lines* lines = nullptr;
// Debug("{}", int(_viewMode));
      switch (_viewMode) {
            default: break;
            case ViewMode::File:       lines = &file()->fileText(); break;
            case ViewMode::Outline:  lines = &file()->outline(); break;
            case ViewMode::GitVersion: lines = &file()->gitVersion(); break;
            case ViewMode::GitDiff:    lines = &file()->gitVersion(); break;
            case ViewMode::Annotations:       lines = &file()->bugs(); break;
            }
      static const Line emptyLine;
      if (!lines || lines->empty() || row >= lines->size())
            return emptyLine;
      return lines->at(row);
      }

//---------------------------------------------------------
//   rows
//---------------------------------------------------------

int Kontext::rows() const {
      int n = 0;
      switch (_viewMode) {
            default:
            case ViewMode::File: n = file()->fileText().size(); break;
            case ViewMode::Outline: n = file()->outline().size(); break;
            case ViewMode::GitVersion: n = file()->gitVersion().size(); break;
            case ViewMode::Annotations: n = file()->bugs().size(); break;
            }
      //      if (readOnly() && n > 0)
      //            n -= 1;
      return n;
      }

//---------------------------------------------------------
//   maxLineLength
//---------------------------------------------------------

int Kontext::maxLineLength() const {
      int maxLen         = 0;
      const Lines* lines = nullptr;
      switch (_viewMode) {
            default:
            case ViewMode::File: lines = &file()->fileText(); break;
            case ViewMode::Outline: lines = &file()->outline(); break;
            case ViewMode::GitVersion: lines = &file()->gitVersion(); break;
            case ViewMode::Annotations: lines = &file()->bugs(); break;
            }
      if (lines) {
            for (const auto& line : *lines)
                  if (line.size() > maxLen)
                        maxLen = line.size();
            }
      return maxLen;
      }

//---------------------------------------------------------
//   posValid
//---------------------------------------------------------

bool Kontext::posValid(const Pos& pos) const {
      if (pos.col < 0 || pos.row < 0)
            return false;
      if (pos.col > file()->columns(pos.row))
            return false;
      if (pos.row >= rows())
            return false;
      return true;
      }

//---------------------------------------------------------
//   nextRow
//    return next visible row
//    return -1 if there is no next row
//---------------------------------------------------------

int Kontext::nextRow(int row) const {
      if (row >= rows() - 1)
            return -1;
      ++row;
      while (row < (rows() - 1) && folded(row))
            ++row;
      if (folded(row))
            return -1;
      return row;
      }

//---------------------------------------------------------
//   previousRow
//    return previous visible row
//    return -1 if there is no previous row
//---------------------------------------------------------

int Kontext::previousRow(int row) const {
      if (row <= 0)
            return -1;
      --row;
      while (row > 0 && folded(row))
            --row;
      if (folded(row))
            return -1;
      return row;
      }

//---------------------------------------------------------
//   folded
//    check if row is part of a foldable area and return
//    true if its folded (unvisible)
//---------------------------------------------------------

bool Kontext::folded(int row) const {
      if (row >= rows())
            return false;
      const Line& l = line(row);
      FoldMark mark = l.fold();
      if (mark != FoldMark::Fold)
            return false;
      // go back to begin of folded area to find out if
      // area is folded by checking the arrow mark
      while (row--) {
            const Line& l = line(row);
            FoldMark mark = l.fold();
            if (mark == FoldMark::Begin)
                  return l.label() == QChar(0x25b6);
            }
      return false;
      }

//---------------------------------------------------------
//   readOnly
//---------------------------------------------------------

bool Kontext::readOnly() const {
      return file()->readOnly() || _viewMode != ViewMode::File;
      }
