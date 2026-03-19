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

#include <string>
#include <QList>
#include <QVariant>
#include <QPoint>
#include "file.h"

class Kontext;
class UndoCommand;

//---------------------------------------------------------
//   UndoStack
//---------------------------------------------------------

class UndoStack : public QObject
      {
      Q_OBJECT

      UndoCommand* curCmd{nullptr};
      QList<UndoCommand*> list;
      int curIdx{0};
      int cleanIdx{0};
      bool _dirty{false};

    signals:
      void undoChanged();
      void dirtyChanged();

    public:
      UndoStack(QObject* parent = nullptr) : QObject(parent) {};
      ~UndoStack();

      void reset();
      void beginMacro();
      void endMacro(bool rollback = false);
      void push(UndoCommand*); // push & execute
      void push1(UndoCommand*);
      void pop();
      void setClean() {
            cleanIdx = curIdx;
            setDirty(false);
            } // this is set by project->save()
      bool canUndo() const { return curIdx > 0; }
      bool canRedo() const { return curIdx < list.size(); }
      bool dirty() const { return _dirty; }
      void setDirty(bool v) {
            if (_dirty != v) {
                  _dirty = v;
                  emit dirtyChanged();
                  }
            }
      bool isEmpty() const { return !canUndo() && !canRedo(); }
      UndoCommand* current() const { return curCmd; }
      void undo();
      void redo();
      };

//---------------------------------------------------------
//   UndoCommand
//---------------------------------------------------------

class UndoCommand
      {
      QList<UndoCommand*> childList;

    protected:
      File* file;
      virtual void flip() {}

    public:
      virtual const std::string name() { return "??"; }
      virtual ~UndoCommand();
      virtual void undo();
      virtual void redo();
      void appendChild(UndoCommand* cmd) { childList.append(cmd); }
      UndoCommand* removeChild() { return childList.takeLast(); }
      int childCount() const { return childList.size(); }
      void unwind();
      virtual void cleanup(bool undo);
      };

//---------------------------------------------------------
//   Patch
//---------------------------------------------------------

class Patch : public UndoCommand
      {
      std::vector<PatchItem> items;
      Cursor p1, p2;    // set cursor position at end of operation
      bool redo = true; // start with redo state

    public:
      const std::string name() override { return "Patch"; };
      Patch(File* k, const Pos& p, int r, const QString& i, const Cursor& o1 = Cursor(), const Cursor& o2 = Cursor()) {
            file = k;
            items.push_back({p, r, i});
            p1 = o1;
            p2 = o2;
            }
      Patch(File* k, const Cursor& o1 = Cursor(), const Cursor& o2 = Cursor()) {
            file = k;
            p1   = o1;
            p2   = o2;
            }
      void add(const PatchItem& i) { items.push_back(i); }
      virtual void flip() override;
      bool empty() const { return items.empty(); }
      int size() const { return items.size(); }
      };
