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

#include <QWidget>
#include <QPointF>
#include "types.h"

class QEvent;
class QPaintEvent;
class QFocusEvent;
class QWheelEvent;
class QMouseEvent;
class Editor;
class Line;

struct DrawingContext;

//----------------------------------------------------------
//   EditWin
//----------------------------------------------------------

class EditWidget : public QWidget
      {
      Q_OBJECT

      Editor* editor;
      bool _darkMode{false};

      virtual void keyPressEvent(QKeyEvent* event) override;
      virtual void paintEvent(QPaintEvent* e) override;
      virtual void wheelEvent(QWheelEvent*) override;
      Pos pixelToChar(const QPointF& e);
      int leftMargin() const;
      int hoverMark{-1};
      void paintLine(DrawingContext& dc, int row, int y);
      Pos screenPosToFilePos(Pos screenPos);
      int screenRowToFileRow(int screenRow);

    protected:
      void mousePressEvent(QMouseEvent*) override;
      void mouseMoveEvent(QMouseEvent*) override;

    signals:
      void markerClicked(int row);

    public slots:
      void setDarkMode(bool v) { _darkMode = v; update(); }

    public:
      static constexpr qreal BORDER{4.0};
      EditWidget(QWidget* parent, Editor*);
      QSize textSize() const;
      int rows() const { return textSize().height(); }
      int columns() const { return textSize().width(); }
      QSize visibleSize() const;
      QPointF charPosToPixel(const Pos&);
      bool darkMode() const { return _darkMode; }
      };
