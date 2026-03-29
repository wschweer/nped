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
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

class Editor;
struct Pos;

class EditWidget : public QWidget
      {
      Q_OBJECT

      Editor* editor;
      int hoverMark{-1};
      static const int BORDER = 2;

   protected:
      void paintEvent(QPaintEvent*) override;
      void keyPressEvent(QKeyEvent*) override;
      void wheelEvent(QWheelEvent*) override;
      void mousePressEvent(QMouseEvent*) override;
      void mouseMoveEvent(QMouseEvent*) override;
      void mouseReleaseEvent(QMouseEvent*) override;

   public:
      EditWidget(QWidget* parent, Editor* e);
      QSize visibleSize() const;
      int leftMargin() const;
      int rows() const { return textSize().height(); }
      int columns() const { return textSize().width(); }
      QSize textSize() const;
      bool darkMode() const;
      void paintLine(struct DrawingContext& dc, int fileRow, int y);
      QPointF charPosToPixel(const Pos& e);
      Pos pixelToChar(const QPointF& e);
      Pos screenPosToFilePos(Pos screenPos);
      int screenRowToFileRow(int screenRow);

   signals:
      void markerClicked(int row);
      };
