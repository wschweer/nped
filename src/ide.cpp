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

#include <QDir>
#include <QString>
#include <QFile>
#include <QLabel>
#include <QProcess>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "logger.h"
#include "editor.h"

//---------------------------------------------------------
//   initProject
//---------------------------------------------------------

bool Editor::initProject() {
      //
      // lookup CmakeLists.txt
      //
      QDir cwd(".");
      while (!cwd.exists("CMakeLists.txt")) {
            if (!cwd.cdUp()) {
                  Debug("no CMakeLists.txt found");
                  _projectMode = false;
                  _projectRoot = QDir::homePath();
                  Debug("not in project mode: <{}>", _projectRoot);
                  return false;
                  }
            }
      _projectRoot = cwd.absolutePath();
      Debug("project mode: <{}>", _projectRoot);
      _projectMode = true;

      // Check for GIT repository
      _git.init();
      _hasGit = _git.isInitialized();
      if (_hasGit) {
            _currentBranchName = _git.getCurrentBranch();
            Debug("git branch: <{}>", _currentBranchName);
            }
      if (branchLabel) {
            branchLabel->setVisible(_projectMode);
            branchLabel->setText(_hasGit ? _currentBranchName : QString());
            }

      return true;
      }
