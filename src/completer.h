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

#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>
#include <QKeyEvent>
#include <QAbstractItemView>

//---------------------------------------------------------
//   Completer
//---------------------------------------------------------

class Completer : public QLineEdit
      {
      Q_OBJECT
      Q_PROPERTY(QStringList suggestions READ suggestions WRITE setSuggestions)

      QCompleter* m_completer;
      QStringList m_suggestionList;
      QStringList m_history;
      int m_historyIndex = 0;

    protected:
      void keyPressEvent(QKeyEvent* event) override;

    public slots:
      void completeText(const QString& completion) {
            // Ersetzt den aktuellen Text durch die Auswahl
            setText(completion);
            selectAll(); // Optional: Alles markieren für schnelles Weitertippen
            }

    public:
      explicit Completer(QWidget* parent = nullptr) : QLineEdit(parent), m_completer(new QCompleter(this)) {
            m_completer->setCaseSensitivity(Qt::CaseInsensitive);
            m_completer->setCompletionMode(QCompleter::PopupCompletion);
            setCompleter(m_completer);

            // Verbindung: Wenn ein Vorschlag (per Klick/Enter) gewählt wird,
            // wird er in das LineEdit geschrieben.
            connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated), this, &Completer::completeText);
            connect(this, &QLineEdit::returnPressed, this, [this]() { addHistory(text()); });
            }
      void setSuggestions(const QStringList& list) {
            m_suggestionList = list;
            m_completer->setModel(new QStringListModel(m_suggestionList, m_completer));
            }
      QStringList suggestions() const { return m_suggestionList; }
      QStringList history() const { return m_history; }
      void setHistory(const QStringList& h) {
            m_history      = h;
            m_historyIndex = m_history.size();
            }
      void addHistory(const QString& txt) {
            if (!txt.isEmpty()) {
                  if (m_history.isEmpty() || m_history.last() != txt)
                        m_history.append(txt);
                  m_historyIndex = m_history.size();
                  }
            }
      };
