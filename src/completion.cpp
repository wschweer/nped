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
      layout->setSpacing(6);  // default
      setMinimumWidth(10);
      setMinimumHeight(10);
      connect(list, &QListWidget::itemActivated, [this] (QListWidgetItem* item) {
            emit applyCompletion(list->row(item));
            });
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
            applyCompletion(idx);
            }
      else
            kontext()->file()->languageClient()->completionRequest(kontext());
      }

//---------------------------------------------------------
//   applyCompletion
//---------------------------------------------------------

void Editor::applyCompletion(int idx)
      {
      const PatchItem& item  = completions[idx].patch;
      Pos p2                 = item.startPos;
      p2.col                += item.insertText.size();
      Cursor c1 = kontext()->cursor();    // current cursor position
      Cursor c2(p2, Pos(p2.col, -1));     // cursor after insert operation
      undoPatch(item.startPos, item.toRemove, item.insertText, c2, c1);
      hideCompletions();
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
      QFontMetricsF fm(completionsPopup->listFont(), completionsPopup);
      qreal _fh = fm.lineSpacing();
      completionsPopup->setList(l);
      double w = 0;
      for (const auto& s : completions)
            w = std::max(w, fm.horizontalAdvance(s.label));
      auto cm = 12 + 30;
      completionsPopup->setGeometry(k.x(), k.y(), int(w) + cm, (_fh+1) * l.size() + cm);
      completionsPopup->show();
      editWidget()->setFocus();
      }
