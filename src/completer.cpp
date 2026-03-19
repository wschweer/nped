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
#include "completer.h"

//---------------------------------------------------------
//   keyPressEvent
//---------------------------------------------------------

void Completer::keyPressEvent(QKeyEvent* event) {
      if (m_completer && m_completer->popup()->isVisible()) {
            switch (event->key()) {
                  case Qt::Key_Tab: {
                        // Durch das Popup navigieren
                        QAbstractItemView* popup = m_completer->popup();
                        QModelIndex currentIndex = popup->currentIndex();
                        int rowCount             = m_completer->completionCount();

                        int nextRow = (currentIndex.isValid()) ? (currentIndex.row() + 1) % rowCount : 0;

                        QModelIndex nextIndex = m_completer->completionModel()->index(nextRow, 0);
                        popup->setCurrentIndex(nextIndex);
                        return;
                        }
                  case Qt::Key_Enter:
                  case Qt::Key_Return: {
                        // Manuelle Übernahme des aktuell markierten Eintrags im Popup
                        QModelIndex index  = m_completer->popup()->currentIndex();
                        QString completion = m_completer->completionModel()->data(index).toString();
                        completeText(completion);
                        m_completer->popup()->hide();
                        return;
                        }
                  case Qt::Key_Escape: m_completer->popup()->hide(); return;
                  default: break;
                  }
            }
      else {
            switch (event->key()) {
                  case Qt::Key_Up:
                        if (m_historyIndex > 0) {
                              m_historyIndex--;
                              setText(m_history[m_historyIndex]);
                              }
                        return;
                  case Qt::Key_Down:
                        if (m_historyIndex < m_history.size() - 1) {
                              m_historyIndex++;
                              setText(m_history[m_historyIndex]);
                              }
                        else if (m_historyIndex == m_history.size() - 1) {
                              m_historyIndex++;
                              clear();
                              }
                        return;
                  default: break;
                  }
            }

      QLineEdit::keyPressEvent(event);

      // Popup zeigen, wenn Text getippt wird
      if (!event->text().isEmpty() && event->key() != Qt::Key_Enter && event->key() != Qt::Key_Return) {
            m_completer->setCompletionPrefix(text());
            m_completer->complete();
            }
      }
