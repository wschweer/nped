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
#include <QToolButton>
#include <QKeyEvent>

#include "logger.h"

//---------------------------------------------------------
//   AttachmentButton
//---------------------------------------------------------

class AttachmentButton : public QToolButton {
    Q_OBJECT
    int index;

signals:
    void deleteRequested(int index);

protected:

    void keyPressEvent(QKeyEvent* event) override {
       Debug("=========");
        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            emit deleteRequested(index);
        } else {
            QToolButton::keyPressEvent(event);
        }
    }
public:
    explicit AttachmentButton(int idx, QWidget* parent = nullptr)
        : QToolButton(parent), index(idx) {
      setFocusPolicy(Qt::ClickFocus);
      }

};
