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
#include <QHBoxLayout>
#include <QToolButton>
#include <QAction>
#include <QLabel>
#include <QVBoxLayout>
class Dashboard : public QWidget
      {
      Q_OBJECT

      QHBoxLayout* hLayout1;
      QHBoxLayout* hLayout2;
      QLabel* tokenLabel;

    public:
      explicit Dashboard(QWidget* parent = nullptr);
      void addWidget(QWidget* widget, int row = 0);
      void addAction(QAction* action, int row = 0);
      void setTokenCount(size_t tokens);
      };
