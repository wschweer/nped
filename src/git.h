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
#include <vector>
#include <git2.h>
#include <QString>
#include "vectormodel.h"
#include "line.h"

//---------------------------------------------------------
//   GitHistory
//---------------------------------------------------------

class GitHistory
      {
    public:
      git_oid oid;
      QString commitSummary;
      };

//---------------------------------------------------------
//   GitList
//---------------------------------------------------------

class GitList : public VectorModel<GitHistory>
      {
    public:
      GitList(QObject* parent = nullptr) : VectorModel<GitHistory>(parent) {
            addRole("display", [](GitHistory* g) { return QVariant(g->commitSummary); });
            }
      };

//---------------------------------------------------------
//   Git
//---------------------------------------------------------

class Git
      {
      bool initialized{false};
      git_repository* repo{nullptr};
      const char* repo_path{"."};

      bool check_error(int error_code, const char* action);

    public:
      Git();
      ~Git();
      void init();
      bool isInitialized() const { return initialized; }
      QString getCurrentBranch();
      void getHistory(const QString&, std::vector<GitHistory*>&);
      Lines getFile(const git_oid* oid);
      };
