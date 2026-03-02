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

#include <QVBoxLayout>
#include <QTextOption>
#include "completion.h"
#include "editor.h"
#include "kontext.h"
#include "lsclient.h"
#include "editwin.h"

//---------------------------------------------------------
//   CompletionsPopup
//---------------------------------------------------------

CompletionsPopup::CompletionsPopup(QWidget* parent) : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
      setFrameStyle(QFrame::Box | QFrame::Plain);
      setAttribute(Qt::WA_ShowWithoutActivating);
      setLineWidth(0);
      setFocusPolicy(Qt::NoFocus);
      setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

      QVBoxLayout* layout = new QVBoxLayout(this);
      list                = new QListWidget(nullptr);
      list->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
      layout->addWidget(list);
      setMinimumWidth(10);
      setMinimumHeight(10);
      setMaximumHeight(250);
      }

//---------------------------------------------------------
//   setList
//---------------------------------------------------------

void CompletionsPopup::setList(const Completions& l) {
      list->clear();
      for (const auto& t : l)
            list->addItem(t.label);
      list->setCurrentRow(0);
      }

//---------------------------------------------------------
//   down
//---------------------------------------------------------

void CompletionsPopup::down() {
      if (list->currentRow() < list->count() - 1)
            list->setCurrentRow(list->currentRow() + 1);
      }

//---------------------------------------------------------
//   up
//---------------------------------------------------------

void CompletionsPopup::up() {
      if (list->currentRow() > 0)
            list->setCurrentRow(list->currentRow() - 1);
      }

//---------------------------------------------------------
//   requestCompletions
//---------------------------------------------------------

void Editor::requestCompletions() {
      if (completionsPopup->isVisible()) {
            int idx                = completionsPopup->currentIndex();
            const PatchItem& item  = completions[idx].patch;
            Pos p2                 = item.startPos;
            p2.col                += item.insertText.size();
            undoPatch(item.startPos, item.toRemove, item.insertText, Cursor(p2, Pos()), kontext()->cursor());

            completionsPopup->hide();
            return;
            }
      kontext()->file()->languageClient()->completionRequest(kontext());
      }

//---------------------------------------------------------
//   showCompletions
//---------------------------------------------------------

void Editor::showCompletions(const Completions& l) {
      if (l.empty())
            return;
      completions = l;
      auto s      = kontext()->cursor().screenPos;
      auto k      = editWidget()->charPosToPixel(s) + QPoint(50, 0);
//      auto pt     = editWidget()->mapToGlobal(k);
      QFontMetricsF fm(completionsPopup->font());
      qreal _fw = fm.horizontalAdvance("x", QTextOption());
      qreal _fh = fm.lineSpacing();
      completionsPopup->setList(l);
      int w = 0;
      for (const auto& s : completions)
            w = std::max(w, int(s.label.size()));
      completionsPopup->setGeometry(k.x(), k.y(), w * _fw, _fh * (l.size() + 1));
      completionsPopup->show();
      editWidget()->setFocus();
      }
