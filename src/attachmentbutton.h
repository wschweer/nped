#pragma once
#include <QToolButton>
#include <QKeyEvent>

#include "logger.h"
class AttachmentButton : public QToolButton
      {
      Q_OBJECT
      int index;

    public:
      explicit AttachmentButton(int idx, QWidget* parent = nullptr) : QToolButton(parent), index(idx) {
            setFocusPolicy(Qt::ClickFocus);
            }
    signals:
      void deleteRequested(int index);

    protected:

      void keyPressEvent(QKeyEvent* event) override {
            Debug("=========");
            if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
                  emit deleteRequested(index);
            else
                  QToolButton::keyPressEvent(event);
            }
      };
