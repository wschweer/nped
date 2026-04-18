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

#include "dashboard.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QFrame>
#include <QStyleOption>
#include <QPainter>

//---------------------------------------------------------
//   Dashboard
//---------------------------------------------------------

Dashboard::Dashboard(QWidget* parent) : QWidget(parent) {
      setObjectName("dashboardWidget");

      // Use a frame to better control the background/border/shadow look
      // but keeping it as a QWidget for now and applying style to the container
      QVBoxLayout* mainLayout = new QVBoxLayout(this);
      mainLayout->setContentsMargins(0, 0, 0, 0);
      mainLayout->setSpacing(2);

      hLayout1 = new QHBoxLayout();
      hLayout1->setContentsMargins(0, 0, 0, 0);
      hLayout1->setSpacing(4);
      mainLayout->addLayout(hLayout1);

      hLayout2 = new QHBoxLayout();
      hLayout2->setContentsMargins(0, 0, 0, 0);
      hLayout2->setSpacing(4);
      mainLayout->addLayout(hLayout2);

      tokenLabel = new QLabel("Tokens: 0", this);
      hLayout2->insertWidget(-1, tokenLabel, 0, Qt::AlignRight);
      }

void Dashboard::paintEvent(QPaintEvent*) {
      QStyleOption opt;
      opt.initFrom(this);
      QPainter p(this);
      style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
      }

//---------------------------------------------------------
//   addWidget
//---------------------------------------------------------

void Dashboard::addWidget(QWidget* widget, int row) {
      (row == 0 ? hLayout1 : hLayout2)->insertWidget(0, widget);
      }

//---------------------------------------------------------
//   addStretch
//---------------------------------------------------------

void Dashboard::addStretch(int row) {
      (row == 0 ? hLayout1 : hLayout2)->insertStretch(0);
      }

//---------------------------------------------------------
//   addAction
//---------------------------------------------------------

void Dashboard::addAction(QAction* action, int row) {
      QToolButton* button = new QToolButton(this);
      button->setDefaultAction(action);
      button->setToolButtonStyle(Qt::ToolButtonIconOnly);
      (row == 0 ? hLayout1 : hLayout2)->insertWidget(0, button);
      }

//---------------------------------------------------------
//   setTokenCount
//---------------------------------------------------------

void Dashboard::setTokenCount(size_t tokens) {
      tokenLabel->setText(QString("Tokens: %1 k").arg(tokens / 1000));
      }
