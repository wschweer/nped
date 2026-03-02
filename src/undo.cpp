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

#include <algorithm>
#include "undo.h"
#include "logger.h"
#include "kontext.h"
#include "file.h"
#include "editor.h"

//---------------------------------------------------------
//   UndoCommand
//---------------------------------------------------------

UndoCommand::~UndoCommand()
      {
      for (auto c : childList)
            delete c;
      }

//---------------------------------------------------------
//   UndoCommand::cleanup
//---------------------------------------------------------

void UndoCommand::cleanup(bool undo)
      {
      for (auto c : childList)
            c->cleanup(undo);
      }

//---------------------------------------------------------
//   undo
//---------------------------------------------------------

void UndoCommand::undo()
      {
      int n = childList.size();
      for (int i = n - 1; i >= 0; --i)
            childList[i]->undo();
      flip();
      }

//---------------------------------------------------------
//   redo
//---------------------------------------------------------

void UndoCommand::redo()
      {
      int n = childList.size();
      for (int i = 0; i < n; ++i)
            childList[i]->redo();
      flip();
      }

//---------------------------------------------------------
//   unwind
//---------------------------------------------------------

void UndoCommand::unwind()
      {
      while (!childList.isEmpty()) {
            UndoCommand* c = childList.takeLast();
            c->undo();
            delete c;
            }
      }

//---------------------------------------------------------
//   UndoStack
//---------------------------------------------------------

UndoStack::~UndoStack()
      {
      int idx = 0;
      for (auto c : list)
            c->cleanup(idx++ < curIdx);
      qDeleteAll(list);
      }

//---------------------------------------------------------
//   beginMacro
//---------------------------------------------------------

void UndoStack::beginMacro()
      {
      if (curCmd) {
            Fatal("already active");
            return;
            }
      curCmd = new UndoCommand();
      }

//-------------------------------------------------------------------
//   endMacro
//    the undo macro will not be recorded if rollback is true
//-------------------------------------------------------------------

void UndoStack::endMacro(bool rollback)
      {
      if (curCmd == 0) {
            Fatal("not active");
            return;
            }
      if (rollback || curCmd->childCount() == 0)
            delete curCmd;
      else {
            // remove redo stack
            while (list.size() > curIdx) {
                  UndoCommand* cmd = list.takeLast();
                  cmd->cleanup(false); // delete elements for which UndoCommand() holds ownership
                  delete cmd;
                  }
            list.append(curCmd);
            ++curIdx;
            setDirty(cleanIdx != curIdx);
            }
      curCmd = nullptr;
      emit undoChanged();
      }

//---------------------------------------------------------
//   push
//---------------------------------------------------------

void UndoStack::push(UndoCommand* cmd)
      {
      //      Debug("{}", cmd->name());
      if (_active) // do not record command if not active
            push1(cmd);
      cmd->redo(); // execute command
      }

//---------------------------------------------------------
//   push1
//---------------------------------------------------------

void UndoStack::push1(UndoCommand* cmd)
      {
      if (!curCmd)
            Fatal("no active command");
      else
            curCmd->appendChild(cmd);
      }

//---------------------------------------------------------
//   pop
//---------------------------------------------------------

void UndoStack::pop()
      {
      if (!curCmd) {
            Debug("no active command");
            }
      else {
            UndoCommand* cmd = curCmd->removeChild();
            cmd->undo();
            }
      }

//---------------------------------------------------------
//   undo
//---------------------------------------------------------

void UndoStack::undo()
      {
      if (canUndo()) {
            list[--curIdx]->undo();
            setDirty(cleanIdx != curIdx);
            }
      emit undoChanged();
      }

//---------------------------------------------------------
//   redo
//---------------------------------------------------------

void UndoStack::redo()
      {
      if (canRedo()) {
            list[curIdx++]->redo();
            setDirty(cleanIdx != curIdx);
            }
      else
            Critical("cannot");
      emit undoChanged();
      }

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void UndoStack::reset()
      {
      delete curCmd;
      qDeleteAll(list);
      list.clear();
      curIdx = cleanIdx = 0;
      setDirty(false);
      _active = true;
      emit undoChanged();
      }

//---------------------------------------------------------
//   flip
//---------------------------------------------------------

void Patch::flip()
      {
      std::reverse(items.begin(), items.end());
      file->patch(items);
      emit file->cursorChanged(redo ? p1 : p2);
      redo = !redo;
      }
