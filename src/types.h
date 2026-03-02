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

#include <QPoint>
#include <nlohmann/json.hpp>

class File;
using json     = nlohmann::json;
using Callback = std::function<void(const json&)>;

//---------------------------------------------------------
//   Pos
//---------------------------------------------------------

struct Pos {
      int col{0};
      int row{0};
      void set(int r, int c) {
            row = r;
            col = c;
            }
      QPoint point() const { return QPoint(col, row); }
      Pos(const QPoint& p) : col(p.x()), row(p.y()) {}
      Pos(int x, int y) : col(x), row(y) {}
      Pos() {}
      Pos operator+(const Pos& p) const { return Pos(col + p.col, row + p.row); }
      Pos operator+(const QPoint& p) const { return Pos(col + p.x(), row + p.y()); }
      bool operator==(const Pos& p) const { return col == p.col && row == p.row; }
      };

//---------------------------------------------------------
//   Cursor
//---------------------------------------------------------

struct Cursor {
      Pos filePos;
      Pos screenPos;
      int screenCol() const { return screenPos.col; }
      int screenRow() const { return screenPos.row; }
      int fileCol() const { return filePos.col; }
      int fileRow() const { return filePos.row; }
      Cursor() {}
      Cursor(const Pos& p1, const Pos& p2) : filePos(p1), screenPos(p2) {}
      };

//---------------------------------------------------------
//   Selection
//---------------------------------------------------------

enum class SelectionMode { NoSelect, RowSelect, ColSelect };

struct Selection {
      SelectionMode mode;
      Cursor cursor;
      Pos start;
      Pos end;
      };

//---------------------------------------------------------
//   Range
//---------------------------------------------------------

struct Range {
      Pos start;
      Pos end;
      json toJson() {
            json range;
            json startJson;
            json endJson;
            startJson["line"]      = start.row;
            startJson["character"] = start.col;
            endJson["line"]        = end.row;
            endJson["character"]   = end.col;
            range["start"]         = startJson;
            range["end"]           = endJson;
            return range;
            }
      Range(const json& range) {
            start.col = range["start"]["character"];
            start.row = range["start"]["line"];
            end.col   = range["end"]["character"];
            end.row   = range["end"]["line"];
            }
      Range() {}
      Range(const Pos& s, const Pos& e) : start(s), end(e) {}
      };

//---------------------------------------------------------
//   PatchItem
//---------------------------------------------------------

struct PatchItem {
      Pos startPos;
      int toRemove{0};
      QString insertText;
      void setRange(const Range& r, File*);

      bool empty() const { return toRemove == 0 && insertText.isEmpty(); }
      };

using Patches = std::vector<PatchItem>;
