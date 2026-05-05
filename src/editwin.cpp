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
#include <QEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QClipboard>
#include <QApplication>
#include <QTimer>
#include <QLabel>

#include "editwin.h"
#include "editor.h"
#include "kontext.h"
#include "types.h"
//---------------------------------------------------------
//   EditWin
//---------------------------------------------------------

EditWidget::EditWidget(QWidget* parent, Editor* e) : QWidget(parent) {
      setAttribute(Qt::WA_OpaquePaintEvent);
      editor = e;
      setFocusPolicy(Qt::NoFocus);
      setMouseTracking(true);
}
//---------------------------------------------------------
//   darkMode
//---------------------------------------------------------

bool EditWidget::darkMode() const {
      return editor->darkMode();
}
//---------------------------------------------------------
//   keyPressEvent
//---------------------------------------------------------

void EditWidget::keyPressEvent(QKeyEvent* e) {
      QString s(e->text());
      Qt::KeyboardModifiers stat = e->modifiers();
      QChar c                    = QChar(0);
      // Debug("{} <{}> key {} {}", int(stat), s, e->key(), e->nativeVirtualKey());
      if (!s.isEmpty() && ((stat & (Qt::CTRL | Qt::ALT)) == 0))
            c = s[0];
      //
      // Umlaute aktivieren
      //
      if (e->key() == -1) {
            switch (e->nativeVirtualKey()) {
                  case 65105:
                  case 214:
                  case 246:
                  case 252:
                  case 220:
                  case 223:
                  case 228:
                  case 196: c = QLatin1Char(e->nativeVirtualKey()); break;
            }
      }
      if (e->key() == 16781905)
            c = QLatin1Char(96);
      else if (e->key() == 16781904)
            c = QLatin1Char(0xb4);

      // Sonderbehandlung:
      //    zweite Alt-Taste aktivieren:
      if (stat & Qt::ALT) {
            switch (e->key()) {
                  case '7': c = QLatin1Char('{'); break;
                  case '8': c = QLatin1Char('['); break;
                  case '9': c = QLatin1Char(']'); break;
                  case '0': c = QLatin1Char('}'); break;
                  case 0xdf: c = QLatin1Char('\\'); break;
                  case '+': c = QLatin1Char('~'); break;
                  case '<': c = QLatin1Char('|'); break;
                  case 'q': c = QLatin1Char('@'); break;
            }
      }
      if (c == QChar::CarriageReturn)
            c = '\n';
      if (c.isPrint() || c == '\n') {
            editor->startCmd();
            editor->input(c);
            editor->endCmd();
      }
}
//---------------------------------------------------------
//   visibleSize
//---------------------------------------------------------

QSize EditWidget::visibleSize() const {
      return QSize((width() - EditWidget::BORDER) / editor->fw(),
                   (height() - EditWidget::BORDER) / editor->fh());
}
//---------------------------------------------------------
//   leftMargin
//---------------------------------------------------------

int EditWidget::leftMargin() const {
      return editor->fw() + EditWidget::BORDER * 2;
}
//---------------------------------------------------------
//   textSize
//    return size of the widget in text lines/columns
//---------------------------------------------------------

QSize EditWidget::textSize() const {
      int w = floor((width() - leftMargin() - 2 * EditWidget::BORDER) / editor->fw());
      int h = floor((height() - EditWidget::BORDER - editor->fa()) / editor->fh() + 1);
      return QSize(w, h);
}
//---------------------------------------------------------
//   wheelEvent
//---------------------------------------------------------

void EditWidget::wheelEvent(QWheelEvent* ev) {
      if (ev->modifiers() & Qt::ControlModifier) {
            int delta = ev->angleDelta().y() / 120;
            qreal s   = editor->scale() * ((delta > 0) ? 1.1 : 0.9);
            editor->set_scale(s);
      }
      else {
            int m      = ev->angleDelta().y() > 0 ? 1 : -1;
            int amount = (ev->modifiers() & Qt::ShiftModifier ? 8 : 2) * m;
            editor->kontext()->moveCursorRel(0, amount, MoveType::Roll);
      }
}
//---------------------------------------------------------
//   pixelToChar
//    converts screen character position (column, line) into
//    screen pixel koodinates
//---------------------------------------------------------

QPointF EditWidget::charPosToPixel(const Pos& e) {
      return QPointF(e.col * editor->fw() + leftMargin() + EditWidget::BORDER,
                     e.row * editor->fh() + EditWidget::BORDER);
}
//---------------------------------------------------------
//   charPosToPixel
//    converts screen character position (column, line) into
//    screen pixel koodinates
//---------------------------------------------------------

Pos EditWidget::pixelToChar(const QPointF& e) {
      return Pos((e.x() - EditWidget::BORDER - leftMargin()) / editor->fw(),
                 (e.y() - EditWidget::BORDER) / editor->fh());
}
//---------------------------------------------------------
//   screenPosToFilePos
//---------------------------------------------------------

Pos EditWidget::screenPosToFilePos(Pos screenPos) {
      Kontext* k = editor->kontext();
      return Pos(screenPos.col + k->screenColumnOffset(), screenRowToFileRow(screenPos.row));
}
//---------------------------------------------------------
//   screenRowToFileRow
//---------------------------------------------------------

int EditWidget::screenRowToFileRow(int screenRow) {
      // we only know the cursor position and use it as a start
      Kontext* k = editor->kontext();
      Cursor c   = k->cursor();
      int n      = screenRow - c.screenRow();
      int row    = c.fileRow();
      if (n == 0)
            return row;
      if (n > 0)
            for (int i = 0; i < n; ++i)
                  row = k->nextRowIfAvailable(row);
      else
            for (int i = 0; i > n; --i)
                  row = k->previousRowIfAvailable(row);
      return row;
}
//---------------------------------------------------------
//   mousePressEvent
//---------------------------------------------------------

void EditWidget::mousePressEvent(QMouseEvent* e) {
      QPoint pos    = e->pos();
      Kontext* k    = editor->kontext();
      Pos screenPos = pixelToChar(pos);
      Pos filePos   = screenPosToFilePos(screenPos);
      if (pos.x() >= 0 && pos.x() < leftMargin()) {
            emit markerClicked(filePos.row);
            return;
      }

      Cursor c;
      c.filePos   = filePos;
      c.screenPos = screenPos;
      k->cursor() = c;

      auto mouseButton = e->button();
      if (mouseButton == Qt::LeftButton) {
            k->setSelectionMode(SelectionMode::CharSelect);
            k->startSelect() = filePos;
            k->endSelect()   = filePos;
      }
      else if (mouseButton == Qt::MiddleButton) {
            editor->startCmd();
            QClipboard* cb = QApplication::clipboard();
            QString txt    = cb->text(QClipboard::Clipboard);
            if (!txt.isEmpty()) {
                  Debug("paste <{}>", txt);
                  editor->input(txt);
            }
            editor->endCmd();
      }
      k->updateSelection();
      emit k->cursorChanged();
      editor->update();
      editor->hideCompletions();
      setFocus(Qt::OtherFocusReason);
}
//---------------------------------------------------------
//   mouseMoveEvent
//---------------------------------------------------------

void EditWidget::mouseMoveEvent(QMouseEvent* e) {
      QPoint p = e->pos();
      if (e->buttons() & Qt::LeftButton) {
            Kontext* k = editor->kontext();
            if (k->selection().mode == SelectionMode::CharSelect) {
                  Pos screenPos         = pixelToChar(p);
                  Pos filePos           = screenPosToFilePos(screenPos);
                  k->cursor().filePos   = filePos;
                  k->cursor().screenPos = screenPos;
                  k->updateSelection();
                  editor->update();
                  return;
            }
      }

      if (p.x() >= 0 && p.x() < leftMargin()) {
            Kontext* k = editor->kontext();
            int row    = screenRowToFileRow(int((p.y() - EditWidget::BORDER) / editor->fh()));
            row        = std::clamp(row, 0, std::max(0, k->rows() - 1));
            if (k->file()->isFoldable(row)) {
                  hoverMark = row;
                  update();
            }
      }
      else if (hoverMark >= 0) {
            hoverMark = -1;
            update();
      }
}
//---------------------------------------------------------
//   mouseReleaseEvent
//---------------------------------------------------------

void EditWidget::mouseReleaseEvent(QMouseEvent* e) {
      if (e->button() == Qt::LeftButton) {
            Kontext* k = editor->kontext();
            if (k->startSelect() == k->endSelect())
                  k->setSelectionMode(SelectionMode::NoSelect);
            editor->update();
      }
}
//---------------------------------------------------------
//   DrawingContext
//---------------------------------------------------------

struct DrawingContext {
      double fh, fw, fa;
      int xo, yo;
      QPainter* painter;
      int lb;
      int visibleColumns;
};
//---------------------------------------------------------
//   paintLine
//---------------------------------------------------------

void EditWidget::paintLine(DrawingContext& dc, int fileRow, int y) {
      const Line& l = editor->kontext()->line(fileRow);
      //*************************************************************
      //    paint selection background
      //*************************************************************

      auto xToPixel = [&](int x) { return dc.lb + (x - dc.xo) * dc.fw; };

      const Selection& s = editor->kontext()->selection();
      Pos start          = s.start;
      Pos end            = s.end;
      if (start.row > end.row)
            std::swap(start, end);
      if (fileRow >= start.row && fileRow <= end.row) {
            qreal yy = y - dc.fa;
            QRect r;
            switch (s.mode) {
                  case SelectionMode::NoSelect: break;
                  case SelectionMode::RowSelect:
                        //
                        r = QRect(xToPixel(0), yy, dc.visibleColumns * dc.fw, dc.fh + 2);
                        break;
                  case SelectionMode::ColSelect:
                        //
                        r = QRect(xToPixel(start.col), yy, s.width() * dc.fw, dc.fh + 2);
                        break;
                  case SelectionMode::CharSelect:
                        if (fileRow == start.row && fileRow == end.row)
                              r = QRect(xToPixel(s.start.col), yy, s.width() * dc.fw, dc.fh + 2);
                        else if (fileRow > start.row && fileRow < end.row)
                              r = QRect(xToPixel(0), yy, dc.visibleColumns * dc.fw, dc.fh + 2);
                        else if (fileRow == start.row)
                              r = QRect(xToPixel(start.col), yy, (dc.visibleColumns - start.col) * dc.fw,
                                        dc.fh + 2);
                        else if (fileRow == end.row)
                              r = QRect(xToPixel(0), yy, end.col * dc.fw, dc.fh + 2);
                        break;
            }
            dc.painter->fillRect(r, editor->textStyle(TextStyle::Selection).bg);
      }

      if (l.label() != ' ') {
            //*************************************************************
            //    paint background of annotated line
            //*************************************************************

            QRect s(leftMargin(), y - dc.fa, width() - leftMargin(), dc.fh);
            //            dc.painter->fillRect(s, editor->textStyle(TextStyle::MarkedLine).bg);
            dc.painter->fillRect(s, editor->textStyle(TextStyle::Gutter).bg);
            dc.painter->setPen(l.labelColor());
            dc.painter->setFont(editor->font());
            dc.painter->drawText(EditWidget::BORDER, y, l.label());
            dc.painter->setPen(editor->textStyle(TextStyle::Normal).fg);
      }

      //*************************************************************
      //    paint text
      //*************************************************************

      auto style = TextStyle::NonText;
      for (const auto& m : l.marks()) {
            if ((m.col2 - dc.xo) <= 0) // tag is left of visible area
                  continue;
            if (m.col1 - dc.xo >= dc.visibleColumns) // tag is right of visible area
                  continue;
            int col1 = m.col1;
            int n    = m.col2 - m.col1;
            if ((m.col1 - dc.xo) < 0) {
                  col1 += dc.xo - m.col1;
                  n    -= dc.xo - m.col1;
            }
            if (col1 - dc.xo + n > dc.visibleColumns)
                  n -= dc.visibleColumns - (col1 - dc.xo + n);

            QString ts = l.mid(col1, n);
            if (style != m.type) {
                  style     = m.type;
                  auto md   = editor->textStyle(style);
                  auto font = editor->font();
                  font.setItalic(md.italic);
                  font.setBold(md.bold);
                  dc.painter->setPen(md.fg);
                  dc.painter->setFont(font);
            }
            int x = dc.lb + (col1 - dc.xo) * dc.fw;
            dc.painter->drawText(x, y, ts);
      }
}
//---------------------------------------------------------
//   paintEvent
//---------------------------------------------------------

void EditWidget::paintEvent(QPaintEvent* e) {
      Kontext* k = editor->kontext();
      if (!k)
            return;

      //      const File* file     = k->file();
      const Cursor& cursor = k->cursor();
      QRect r(e->rect());

      int lm = leftMargin();

      auto md = editor->textStyle(TextStyle::Normal);
      QPainter painter(this);
      painter.setRenderHint(QPainter::TextAntialiasing, true);

      r.setLeft(std::max(lm, r.x()));
      painter.fillRect(r, md.bg);

      DrawingContext dc;
      dc.painter        = &painter;
      dc.fh             = editor->fh();
      dc.fw             = editor->fw();
      dc.fa             = editor->fa();
      dc.xo             = k->screenColumnOffset();
      dc.lb             = EditWidget::BORDER + lm; // left border in pixel
      dc.visibleColumns = visibleSize().width();

      const QColor hoverMarkerBGColor = md.bg.darker(darkMode() ? 120 : -120);
      const QColor nonTextColor       = editor->textStyle(TextStyle::NonText).bg;

      //*************************************************************
      // draw marker background
      //*************************************************************

      QRect tr(0, 0, lm, height());
      painter.fillRect(tr, editor->textStyle(TextStyle::Gutter).bg);
      if (hoverMark >= 0) {
            int cx = 0;
            int cy = (hoverMark - dc.yo) * dc.fh + EditWidget::BORDER;
            QRect r(cx, cy, lm, dc.fh);
            painter.fillRect(r, hoverMarkerBGColor);
      }

      //*************************************************************
      //    Draw text from cursor position to start of screen.
      //    This allows for easy skipping of folded text parts by
      //    calling kontext->previousRow(row).
      //*************************************************************

      int fileRow = cursor.fileRow();
      for (int i = cursor.screenRow(); i >= 0; --i) {
            int y = EditWidget::BORDER + dc.fa + i * dc.fh;
            if (fileRow < 0)
                  painter.fillRect(QRect(0, y - dc.fa - 1, width(), dc.fh + 2), nonTextColor);
            else {
                  paintLine(dc, fileRow, y);
                  fileRow = k->previousRow(fileRow);
            }
      }

      //*************************************************************
      //    draw text from cursor position to end of screen
      //*************************************************************

      fileRow = k->nextRow(cursor.fileRow());
      for (int i = cursor.screenRow() + 1; i < rows(); ++i) {
            int y = EditWidget::BORDER + dc.fa + i * dc.fh;
            if (fileRow < 0)
                  painter.fillRect(QRect(0, y - dc.fa - 1, width(), dc.fh + 2), nonTextColor);
            else {
                  paintLine(dc, fileRow, y);
                  fileRow = k->nextRow(fileRow);
            }
      }

      //*************************************************************
      // draw cursor
      //*************************************************************

      auto scol = [&](int col) { return col * dc.fw + EditWidget::BORDER + lm; };

      if (k->showCursor() && hasFocus()) {
            // draw cursor
            int cx = scol(k->screenCol());
            int cy = k->screenRow() * dc.fh + EditWidget::BORDER;
            QRect rr(cx, cy, dc.fw + 1, dc.fh + 1);
            painter.fillRect(rr, editor->textStyle(TextStyle::Cursor).bg);
            painter.setPen(editor->textStyle(TextStyle::Cursor).fg);
            painter.setFont(editor->font());
            QString s = k->cursorValid() ? k->line(k->fileRow())[k->fileCol()] : QString("");
            painter.drawText(cx, cy + dc.fa, s);
      }
      painter.end();
}
