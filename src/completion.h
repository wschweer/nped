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

#include <QFrame>
#include <QListWidget>
#include <vector>
#include "file.h"

//---------------------------------------------------------
//   Completion
//---------------------------------------------------------

class Completion
      {
    public:
      QString label; // presented to the user in a list
      PatchItem patch;
      };

using Completions = std::vector<Completion>;

//---------------------------------------------------------
//   CompletionsPupup
//---------------------------------------------------------

class CompletionsPopup : public QFrame
      {
      Q_OBJECT

      QListWidget* list;

    public:
      CompletionsPopup(QWidget* parent = nullptr);
      void setList(const Completions& l);
      void up();
      void down();
      int currentIndex() const { return list->currentRow(); }
      };
