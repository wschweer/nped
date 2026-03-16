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
      editor = e;
      setFocusPolicy(Qt::NoFocus);
      setMouseTracking(true);
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
      return QSize((width() - EditWidget::BORDER) / editor->fw(), (height() - EditWidget::BORDER) / editor->fh());
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
            qreal s   = editor->fontSize() * ((delta > 0) ? 1.1 : 0.9);
            editor->setFontSize(s);
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
      return QPointF((e.col - leftMargin() - EditWidget::BORDER) * editor->fw(), (e.row * editor->fh() + EditWidget::BORDER));
      }

//---------------------------------------------------------
//   charPosToPixel
//    converts screen character position (column, line) into
//    screen pixel koodinates
//---------------------------------------------------------

Pos EditWidget::pixelToChar(const QPointF& e) {
      return Pos((e.x() - EditWidget::BORDER - leftMargin()) / editor->fw(), (e.y() - EditWidget::BORDER) / editor->fh());
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
      File* file = k->file();
      int n      = screenRow - c.screenRow();
      int row    = c.fileRow();
      if (n == 0)
            return row;
      if (n > 0)
            for (int i = 0; i < n; ++i)
                  row = file->nextRowIfAvailable(row);
      else
            for (int i = 0; i > n; --i)
                  row = file->previousRowIfAvailable(row);
      return row;
      }

//---------------------------------------------------------
//   mousePressEvent
//---------------------------------------------------------

void EditWidget::mousePressEvent(QMouseEvent* e) {
      QPoint pos = e->pos();
      Kontext* k = editor->kontext();
      //      const Cursor& c = k->cursor();
      Pos screenPos = pixelToChar(pos);
      Pos filePos   = screenPosToFilePos(screenPos);
      if (pos.x() >= 0 && pos.x() < leftMargin()) {
            emit markerClicked(filePos.row);
            return;
            }
      //      Debug("{} {} -- {} {}", filePos.col, filePos.row, screenPos.col, screenPos.row);

      Cursor c;
      c.filePos   = filePos;
      c.screenPos = screenPos;
      k->cursor() = c;

      auto mouseButton = e->button();
      if (mouseButton == Qt::MiddleButton) {
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
      setFocus(Qt::OtherFocusReason);
      }

//---------------------------------------------------------
//   mouseMoveEvent
//---------------------------------------------------------

void EditWidget::mouseMoveEvent(QMouseEvent* e) {
      QPoint p = e->pos();
      if (p.x() >= 0 && p.x() < leftMargin()) {
            Kontext* k = editor->kontext();
            int row    = screenRowToFileRow(int((p.y() - EditWidget::BORDER) / editor->fh()));
            row        = std::clamp(row, 0, std::max(0, k->file()->rows() - 1));
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
//   DrawingContext
//---------------------------------------------------------

struct DrawingContext {
      double fh, fw, fa;
      int xo, yo;
      QPainter* painter;
      QColor bgColor;
      QColor fgColor;
      QColor selectColor;
      QColor labelBGColor;
      int lb;
      int visibleColumns;
      QRect selection;
      };

//---------------------------------------------------------
//   paintLine
//---------------------------------------------------------

void EditWidget::paintLine(DrawingContext& dc, int fileRow, int y) {
      const Line& l = editor->kontext()->file()->line(fileRow);
      if (l.label() != ' ') {
            QRect s(leftMargin(), y - dc.fa, width() - leftMargin(), dc.fh);
            dc.painter->fillRect(s, dc.labelBGColor);
            dc.painter->setPen(l.labelColor());
            dc.painter->setFont(editor->font());
            dc.painter->drawText(EditWidget::BORDER, y, l.label());
            dc.painter->setPen(dc.fgColor);
            }

      //=============================================
      //    paint selection background
      //=============================================

      if (dc.selection.height() > 0 && fileRow >= dc.selection.top() && fileRow <= dc.selection.bottom()) {
            qreal yy = y - dc.fa;
            QRect r(dc.selection.left(), yy, dc.selection.width(), dc.fh + 2);
            dc.painter->fillRect(r, dc.selectColor);
            }
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

            QString ts                 = l.mid(col1, n);
            const MarkerDefinition* md = markerDefinitions.md(m.type);

            int x = dc.lb + (col1 - dc.xo) * dc.fw;
            if (md->bg.isValid()) {
                  qreal yy = y - dc.fa;
                  QRect r(x, yy, ts.size() * dc.fw, dc.fh);
                  dc.painter->fillRect(r, md->bg.isValid() ? md->bg : dc.bgColor);
                  }
            auto font = editor->font();
            font.setItalic(md->italic);
            font.setBold(md->bold);
            dc.painter->setPen(md->fg);
            dc.painter->setFont(font);
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
      const File* file     = k->file();
      const Cursor& cursor = k->cursor();
      const QRect r(e->rect());

      int lm = leftMargin();

      QPainter painter(this);
      painter.setRenderHint(QPainter::TextAntialiasing, true);

      DrawingContext dc;
      dc.fh = editor->fh();
      dc.fw = editor->fw();
      dc.fa = editor->fa();
      dc.xo = k->screenColumnOffset();
      //      dc.yo             = k->screenRowOffset();
      dc.painter        = &painter;
      dc.bgColor        = editor->bgColor();
      dc.fgColor        = editor->fgColor();
      dc.lb             = EditWidget::BORDER + lm; // left border in pixel
      dc.visibleColumns = visibleSize().width();

      int cr, cg, cb;
      dc.bgColor.getRgb(&cr, &cg, &cb);
      if (_darkMode) {
            cr += (255 - cr) / 4;
            cg += (255 - cg) / 4;
            cb += (255 - cb) / 4;
            }
      else {
            cr -= cr / 8;
            cb -= cb / 8;
            cg -= cg / 8;
            }
      dc.selectColor                   = QColor(cr, cg, cb);
      const QColor hoverMarkerBGColor  = dc.bgColor.darker(_darkMode ? 120 : -120);
      const QColor markerBGColor       = dc.bgColor.darker(_darkMode ? -120 : 120);
      dc.labelBGColor                  = QColor(100, 150, 255).darker(70);

      //
      // draw selection background
      //
      if (k->selectionMode() != SelectionMode::NoSelect) {
            QRect s = k->selection();
            dc.selection.setX((s.x() - dc.xo) * dc.fw + EditWidget::BORDER + lm);
            dc.selection.setWidth(s.width() * dc.fw);
            dc.selection.setY(s.y());
            dc.selection.setHeight(s.height());
            if (k->selectionMode() == SelectionMode::RowSelect) {
                  dc.selection.setX(r.x());
                  dc.selection.setWidth(r.width());
                  }
            }

      //
      // draw marker background
      //
      QRect s(0, 0, lm, height());
      QRect tr = s.intersected(r);
      painter.fillRect(tr, markerBGColor);
      if (hoverMark >= 0) {
            int cx = 0;
            int cy = (hoverMark - dc.yo) * dc.fh + EditWidget::BORDER;
            QRect r(cx, cy, lm, dc.fh);
            painter.fillRect(r, hoverMarkerBGColor);
            }

      int fileRow = cursor.fileRow();
      for (int i = cursor.screenRow(); i >= 0; --i) {
            int y = EditWidget::BORDER + dc.fa + i * dc.fh;
            if (fileRow < 0)
                  painter.fillRect(QRect(0, y - dc.fa - 1, width(), dc.fh + 2), markerBGColor);
            else {
                  paintLine(dc, fileRow, y);
                  fileRow = file->previousRow(fileRow);
                  }
            }
      fileRow = file->nextRow(cursor.fileRow());
      for (int i = cursor.screenRow() + 1; i < rows(); ++i) {
            int y = EditWidget::BORDER + dc.fa + i * dc.fh;
            if (fileRow < 0)
                  painter.fillRect(QRect(0, y - dc.fa - 1, width(), dc.fh + 2), markerBGColor);
            else {
                  paintLine(dc, fileRow, y);
                  fileRow = file->nextRow(fileRow);
                  }
            }

      //
      // draw cursor
      //
      if (k->showCursor()) {
            // draw cursor
            int cx = k->screenCol() * dc.fw + EditWidget::BORDER + lm;
            int cy = k->screenRow() * dc.fh + EditWidget::BORDER;
            QRect rr(cx, cy, dc.fw, dc.fh);
            painter.fillRect(rr, hasFocus() ? editor->fgColor() : QColor(160, 160, 160));
            painter.setPen(dc.bgColor);
            painter.setFont(editor->font());
            QString s = k->cursorValid() ? k->file()->line(k->fileRow())[k->fileCol()] : QString("");
            painter.drawText(cx, cy + dc.fa, s);
            }
      painter.end();
      }
