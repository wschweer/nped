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

#include <QInputDialog>

#include "editor.h"
#include "editwin.h"
#include "file.h"
#include "kontext.h"
#include "lsclient.h"
#include "logger.h"
#include "undo.h"

//---------------------------------------------------------
//    initSearch
//---------------------------------------------------------

bool Editor::initSearch(const QString& param) {
      QString pattern;

      if (param.isEmpty()) {
            searchPattern.setPattern("");
            doReplace = false;
            msg("empty pattern");
            return false;
            }
      doReplace = false;
      int idx   = param.lastIndexOf(QChar('/', 0));
      if ((idx > 0) && (param[idx - 1] != QChar('\\'))) {
            pattern   = param.left(idx);
            replace   = param.mid(idx + 1);
            doReplace = true;
            }
      else {
            pattern   = param;
            doReplace = false;
            }
      if (idx > 0 && pattern[idx - 1] == QChar('\\', 0))
            pattern.remove(idx - 1, 1);
      if (!pattern.isEmpty())
            searchPattern.setPattern(pattern);
      if (searchPattern.pattern().isEmpty()) {
            msg("nothing to search");
            return false;
            }
      if (!searchPattern.isValid()) {
            msg("bad regular expression");
            searchPattern.setPattern("");
            return false;
            }
      return true;
      }

//---------------------------------------------------------
//   createMatchList
//---------------------------------------------------------

bool Editor::createMatchList() {
      if (!searchPattern.isValid() || searchPattern.pattern().isEmpty()) {
            msg("no valid search pattern");
            return false;
            }
      File* file = kontext()->file();
      Lines matchLines;
      matches.clear();
      clearSearchMarks();
      for (int i = 0; i < kontext()->rows(); ++i) {
            int col = 0;
            for (;;) {
                  const QString& s = kontext()->line(i).qstring();
                  auto match       = searchPattern.match(s, col);
                  if (!match.hasMatch())
                        break;
                  int x1 = match.capturedStart(0);
                  int x2 = match.capturedEnd(0);
                  matches.push_back({i, x1, x2});
                  file->addMark(i, x1, x2, TextStyle::Search);
                  col = x2;
                  }
            }
      file->setSearchResults(matchLines);
      return matches.size() > 0;
      }

//---------------------------------------------------------
//   clearSearchMarks
//---------------------------------------------------------

void Editor::clearSearchMarks() {
      kontext()->file()->clearSearchMarks();
      }

//---------------------------------------------------------
//   searchNext
//---------------------------------------------------------

void Editor::searchNext() {
      if (createMatchList()) {
            for (const auto& m : matches) {
                  if ((m.line == kontext()->fileRow() && kontext()->fileCol() <= m.col1) ||
                      m.line > kontext()->fileRow()) {
                        if (doReplace) {
                              int len = m.col2 - m.col1;
                              Pos p1  = {m.col1, m.line};                       // replace position
                              Pos p2  = {int(m.col1 + replace.size()), m.line}; // position after replace
                              int rowOffset = kontext()->screenRowOffset();
                              undoPatch(
                                  p1, len, replace,
                                  Cursor(p2,
                                         Pos(p2.col, p2.row - rowOffset)), // file position - screen position
                                  Cursor(p1, Pos(p1.col, p1.row - rowOffset)));
                              }
                        else {
                              kontext()->moveCursorAbs(m.col2, m.line);
                              kontext()->file()->addMark(m.line, m.col1, m.col2, TextStyle::SearchHit);
                              }
                        return;
                        }
                  }
            }
      msg("<{}> not found", searchPattern.pattern());
      }

//---------------------------------------------------------
//   searchPrev
//---------------------------------------------------------

void Editor::searchPrev() {
      if (!createMatchList())
            return;
      //      auto cursor = kontext()->cursor();
      for (int i = int(matches.size()) - 1; i >= 0; --i) {
            auto m = matches[i];
            if ((m.line == kontext()->fileRow() && kontext()->fileCol() > m.col2) ||
                m.line < kontext()->fileRow()) {
                  kontext()->moveCursorAbs(m.col1, m.line);
                  return;
                  }
            }
      msg("{} not found", searchPattern.pattern());
      }

//---------------------------------------------------------
//   search
//---------------------------------------------------------

void Editor::search(const QString& s) {
      if (!initSearch(s))
            return;
      searchNext();
      }

//---------------------------------------------------------
//   rename
//---------------------------------------------------------

void Editor::rename() {
      if (kontext()->file()->languageClient())
            kontext()->file()->languageClient()->prepareRenameRequest(kontext());
      }

void Editor::rename(Kontext*, const QString& name, int row, int col1, int col2) {
      Debug("<{}> - {} {}-{}", name, row, col1, col2);

      bool ok {false};
      QString origin = QString("Rename: <%1> to:").arg(name);
      QString text   = QInputDialog::getText(this, tr("Global Rename"), origin, QLineEdit::Normal, name, &ok);
      if (ok && !text.isEmpty() && text != name)
            kontext()->file()->languageClient()->renameRequest(kontext(), text, row, col1);
      }
