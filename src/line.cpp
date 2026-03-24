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
#include "line.h"

MarkerDefinitions markerDefinitions;

//---------------------------------------------------------
//   setFold
//---------------------------------------------------------

void Line::setFold(FoldMark m, QColor color) {
      _fold        = m;
      _label.color = color;
      }

//---------------------------------------------------------
//   setLabel
//---------------------------------------------------------

void Line::setLabel(QChar c, QColor color) {
      _label.text  = c;
      _label.color = color;
      }

//---------------------------------------------------------
//   clearSearchMarks
//---------------------------------------------------------

bool Line::clearSearchMarks() {
      bool rv = false;
      for (;;) {
            bool hasChanged = false;
            for (const auto& m : _marks) {
                  if (m.type == Marker::Search || m.type == Marker::SearchHit) {
                        removeMark(m);
                        hasChanged = true; // iterate again
                        rv         = true;
                        break;
                        }
                  }
            if (!hasChanged)
                  break;
            }
      return rv;
      }

//---------------------------------------------------------
//   clearPrettyMarks
//---------------------------------------------------------

bool Line::clearPrettyMarks() {
      bool rv = false;
      for (auto& m : _marks) {
            if (m.type != Marker::Search && m.type != Marker::Normal) {
                  m.type = Marker::Normal;
                  rv     = true;
                  // break;
                  }
            }
      return rv;
      }

//---------------------------------------------------------
//   removeMark
//---------------------------------------------------------

void Line::removeMark(const Mark& m) {
      if (_marks.size() <= 1)
            return;
      for (size_t i = 0; i < _marks.size(); ++i) {
            Mark& mark = _marks[i];
            if (mark == m)
                  mark.type = Marker::Normal;
            }
      }

//---------------------------------------------------------
//   nextLineIsEmpty
//    return true if next line is empty or at end of file
//---------------------------------------------------------

bool Lines::nextLineIsEmpty(int i) const {
      return (i >= (size() - 1)) || at(i + 1).empty();
      }

//---------------------------------------------------------
//   prevLineIsEmpty
//    return true if previous line is empty or at begin of file
//---------------------------------------------------------

bool Lines::prevLineIsEmpty(int i) const {
      return (i == 0) || at(i - 1).empty();
      }

//---------------------------------------------------------
//   isString
//---------------------------------------------------------

bool Line::isString() const {
      bool val = false;
      for (const auto& m : marks()) {
            // col1 points to first character in comment
            // Debug: special case: there is code before the comment
            //            if (m.type == Marker::String)
            //                  Debug("String {} {} <{}>", m.col1, m.col2, qstring());
            if (m.type == Marker::String && m.col1 == 0 && m.col2 == size()) {
                  val = true;
                  break;
                  }
            }
      return val;
      }

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

QString Lines::removeText(const Pos& pos, int nn) {
      int n = nn;
      QString rs;
      int x = pos.col;
      int y = pos.row;

      if (y >= size())
            return QString();
      Line* s = &(*this)[y];
      while (n--) {
            if (x >= s->size()) {
                  rs += '\n';
                  if (y + 1 < size()) {
                        *s += (*this)[y + 1];
                        remove(y + 1);
                        }
                  }
            else {
                  rs += (*s)[x];
                  s->removeAt(x);
                  }
            }
      if (nn != rs.size())
            Critical("bad match {} != {}", nn, rs.size());
      return rs;
      }

//---------------------------------------------------------
//    insertText
//---------------------------------------------------------

void Lines::insertText(const Pos& pos, const QString& text) {
      if (text.isEmpty())
            return;
      int x = pos.col;
      int y = pos.row;
      while (y >= size())
            push_back(Line());
      for (const auto& c : text) {
            if (c == '\n') {
                  // split line in left and right part, remove right part
                  // and prepend it to a new line

                  QString nextLine = (*this)[y].mid(x);  // copy right part
                  (*this)[y]       = (*this)[y].left(x); // remove right part

                  ++y;
                  x = 0;

                  insert(y, Line(nextLine));
                  }
            else {
                  (*this)[y].insert(x++, c);
                  }
            }
      }

//---------------------------------------------------------
//   Lines
//---------------------------------------------------------

Lines::Lines(const QStringList& sl) {
      for (const auto& s : sl)
            push_back(s);
      }

Lines::Lines(const QVector<Line> l) : QVector<Line>(l) {
      }

//---------------------------------------------------------
//   join
//---------------------------------------------------------

QString Lines::join(QChar c) const {
      QString s;
      for (const auto& ss : *this) {
            s += ss.qstring();
            s += c;
            }
      return s;
      }

//---------------------------------------------------------
//   toStringList
//---------------------------------------------------------

QStringList Lines::toStringList() {
      QStringList l;
      for (const auto& ss : *this)
            l << ss.qstring();
      return l;
      }

//---------------------------------------------------------
//   onFileChangedOnDisk
//---------------------------------------------------------
