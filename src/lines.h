#pragma once
#include <QList>
#include <QStringList>
#include "line.h"

class Pos;

class Lines : public QList<Line>
      {
    public:
      Lines() {}
      Lines(const QStringList&);
      Lines(const QString& s) : Lines(s.split('\n')) {}
      Lines(const QVector<Line> l);
      QString join(QChar c) const;
      QStringList toStringList();
      void push_back(const Line& s) { QVector<Line>::push_back(s); }
      void push_back(const QString& s, const Pos& p); // Need to define this
      Lines sliced(int start, int len) const { return Lines(QVector<Line>::sliced(start, len)); }
      QString removeText(const Pos& pos, int n);
      void insertText(const Pos& pos, const QString& text);
      bool nextLineIsEmpty(int i) const;
      bool prevLineIsEmpty(int i) const;
      };
