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
#include <QPoint>
#include <QColor>

#include "marker.h"
#include "types.h"

class File;

static constexpr char macroProperty[]  = "//##P";
static constexpr char macroProperty0[] = "//##P0";
static constexpr char macroProperty1[] = "//##P1";
static constexpr char macroProperty2[] = "//##P2";
static constexpr char macroProperty3[] = "//##P3";
static constexpr char macroProperty4[] = "//JSON";

static constexpr char startReplaceSignature[] = "//##<";
static constexpr char endReplaceSignature[]   = "//##>";

//---------------------------------------------------------
//   Mark
//---------------------------------------------------------

class Mark
      {
    public:
      int col1{0};
      int col2{};
      Marker type{Marker::Normal};
      bool operator==(const Mark& other) const { return col1 == other.col1 && col2 == other.col2 && type == other.type; }
      };

//---------------------------------------------------------
//   Marks
//---------------------------------------------------------

class Marks : public std::vector<Mark>
      {
    public:
      void add(const Mark& m);
      void init(int n) {
            clear();
            push_back(Mark(0, n, Marker::Normal));
            }
      };

//---------------------------------------------------------
//   Label
//    a one character marker on the left of every textline
//---------------------------------------------------------

struct Label {
      QChar text{QChar(' ')};
      QColor color{QColorConstants::Black};
      Pos tag;
      };

//---------------------------------------------------------
//   FoldMark
//---------------------------------------------------------

enum class FoldMark { No, Begin, Fold };

//---------------------------------------------------------
//   Line
//---------------------------------------------------------

class Line : QString
      {
      Label _label;
      FoldMark _fold{FoldMark::No};

      Marks _marks;

    public:
      Line() {}
      Line(const QString& s, const Pos& t = Pos(), QChar m = QChar(' '), QColor c = QColorConstants::Black) : QString(s) {
            _label.tag   = t;
            _label.text  = m;
            _label.color = c;
            _marks.init(s.size());
            }
      Pos tag() const { return _label.tag; }
      void setTag(const Pos& v) { _label.tag = v; }
      QChar label() const { return _label.text; }
      QColor labelColor() const { return _label.color; }
      void setLabel(QChar c, QColor color = QColorConstants::Black);
      void setFold(FoldMark m, QColor color = QColorConstants::Black);
      FoldMark fold() const { return _fold; }
      void addMark(int idx1, int idx2, Marker m) { _marks.add(Mark(idx1, idx2, m)); }
      const Marks& marks() const { return _marks; }
      Marks& marks() { return _marks; }
      void removeMark(const Mark&);

      void markCppToken(int col1, int col2, const QString& token);
      int size() const { return QString::size(); }
      auto operator<=>(const Line& l) const { return _label.tag.row <=> l._label.tag.row; }
      QString mid(int idx) const { return QString::mid(idx); }
      QString mid(int idx, int len) const { return QString::mid(idx, len); }
      QString left(int len) const { return QString::left(len); }
      const QString& qstring() const { return static_cast<const QString&>(*this); }
      QChar operator[](int idx) const { return idx >= size() ? QChar(' ') : QString::at(idx); }
      bool empty() const { return QString::isEmpty(); }
      void insert(int idx, QChar c) {
            QString::insert(idx, c);
            _marks.init(size());
            }
      void removeAt(int idx) {
            QString::removeAt(idx);
            _marks.init(size());
            }
      void operator+=(const Line& l) {
            QString::append(static_cast<QString>(l));
            _marks.init(size());
            }
      bool startsWith(const QString& s) const { return QString::startsWith(s); }
      bool startsWith(const char* s) const { return QString::startsWith(s); }
      bool contains(const QChar& c) const { return QString::contains(c); }
      bool clearSearchMarks();
      bool clearPrettyMarks();
      auto begin() { return QString::begin(); }
      auto begin() const { return QString::begin(); }
      auto end() { return QString::end(); }
      auto end() const { return QString::end(); }
      bool isString() const;
      };

//---------------------------------------------------------
//   Lines
//---------------------------------------------------------

class Lines : public QList<Line>
      {
    public:
      Lines() {}
      Lines(const QStringList&);
      Lines(const QString& s) : Lines(s.split('\n')) {}
      Lines(const QVector<Line> l);
      QString join(QChar c) const;
      QStringList toStringList();
      void push_back(const Line& s) { QVector<Line>::push_back(s); }
      void push_back(const QString& s, const Pos& p) { QVector<Line>::push_back(Line(s, p)); }
      Lines sliced(int start, int len) const { return Lines(QVector<Line>::sliced(start, len)); }
      QString removeText(const Pos& pos, int n);
      void insertText(const Pos& pos, const QString& text);
      bool nextLineIsEmpty(int i) const;
      bool prevLineIsEmpty(int i) const;
      };
