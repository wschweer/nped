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

#include <stdio.h>
#include <string.h>
#include <QByteArray>
#include <QDateTime>
#include <QToolButton>
#include <QDir>

#include "editor.h"
#include "editwin.h"
#include "file.h"
#include "kontext.h"
// #include "lsclient.h"
#include "logger.h"
#include "line.h"
#include "git.h"

//---------------------------------------------------------
//   Git
//---------------------------------------------------------

Git::Git()
      {
      }

Git::~Git()
      {
      git_repository_free(repo);
      git_libgit2_shutdown();
      }

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void Git::init()
      {
      git_libgit2_init();

      QDir cwd = QDir::current();
      repo_path = cwd.canonicalPath().toLocal8Bit();

      for (;;) {
            if (git_repository_open(&repo, repo_path) >= 0) {
                  initialized = true;
                  return;
                  }
            if (!cwd.cdUp())
                  break;
            repo_path = cwd.canonicalPath().toLocal8Bit();
            }
      Debug("no git repository found");
      }

//---------------------------------------------------------
//   check_error
//    Helper-Funktion zur Fehlerprüfung
//---------------------------------------------------------

bool Git::check_error(int error_code, const char* action)
      {
      if (error_code < 0) {
            const git_error* e = git_error_last();
            Critical("Fehler bei {}: {}/{} - {}", action, error_code, e->klass, e->message);
            return false;
            }
      return true;
      }

//---------------------------------------------------------
//   getHistory
//---------------------------------------------------------

void Git::getHistory(const QString& fn, std::vector<GitHistory*>& gh)
      {
      if (!initialized)
            return;
      const char* file_path = strdup(fn.toStdString().c_str());
//      Debug("walk <{}>", fn);
//      Debug("walk <{}>", file_path);

      for (auto i : gh)
            delete i;
      gh.clear();
      gh.push_back(new GitHistory(git_oid(), "-------HEAD------"));

      git_revwalk* walker = NULL;
      check_error(git_revwalk_new(&walker, repo), "Revwalk erstellen");

      // Standard-Sortierung (wie git log)
      git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
      check_error(git_revwalk_push_head(walker), "HEAD pushen");

      // --- Der wichtige Teil: Diff-Optionen mit Pathspec ---
      git_diff_options diff_opts;
      git_diff_options_init(&diff_opts, GIT_DIFF_OPTIONS_VERSION);

      // Wir übergeben den Dateipfad als "pathspec"
      diff_opts.pathspec.count   = 1;
      diff_opts.pathspec.strings = (char**)&file_path;
      // Wichtige Flags für die Pfadprüfung
      diff_opts.flags |= GIT_DIFF_INCLUDE_UNMODIFIED | GIT_DIFF_DISABLE_PATHSPEC_MATCH;
      // ----------------------------------------------------

      git_oid oid;
//      char short_hash[10] = {0}; // Für 7-stelligen Hash + Null-Terminator

      // Schleife durch alle Commits
      while (git_revwalk_next(&oid, walker) == 0) {
            git_commit* commit    = NULL;
            git_tree* tree        = NULL;
            git_commit* parent    = NULL;
            git_tree* parent_tree = NULL;
            git_diff* diff        = NULL;

            check_error(git_commit_lookup(&commit, repo, &oid), "Commit-Lookup");
            check_error(git_commit_tree(&tree, commit), "Commit-Tree holen");

            int error = 0;

            // Diff gegen Eltern-Tree oder (beim Root-Commit) gegen leeren Tree
            if (git_commit_parentcount(commit) > 0) {
                  // Normaler Commit: Diff gegen den ersten Parent
                  check_error(git_commit_parent(&parent, commit, 0), "Parent holen");
                  check_error(git_commit_tree(&parent_tree, parent), "Parent-Tree holen");
                  error = git_diff_tree_to_tree(&diff, repo, parent_tree, tree, &diff_opts);
                  }
            else {
                  // Root-Commit: Diff gegen einen leeren Tree
                  error = git_diff_tree_to_tree(&diff, repo, NULL, tree, &diff_opts);
                  }
            check_error(error, "Diff berechnen");

            // Prüfen, ob der Diff (gefiltert auf unsere Datei) Änderungen enthält
            if (git_diff_num_deltas(diff) > 0) {
                  git_tree_entry* entry = nullptr;
                  const git_oid* blob_oid;
                  int error = git_tree_entry_bypath(&entry, tree, file_path);
                  if (error >= 0) {
                        blob_oid                 = git_tree_entry_id(entry);
                        const char* summary      = git_commit_summary(commit);
                        const git_time_t t       = git_commit_time(commit);
                        const int offset_minutes = git_commit_time_offset(commit);
                        QTimeZone tz(offset_minutes * 60);
                        QDateTime time = QDateTime::fromSecsSinceEpoch(t, tz);
                        if (git_tree_entry_type(entry) == GIT_OBJECT_BLOB) {
                              QString s = time.toString("dd.MM.yy hh:mm  ") + QString(summary);
                              gh.push_back(new GitHistory(*blob_oid, s));
                              char oid_str[GIT_OID_HEXSZ + 1];
                              git_oid_tostr(oid_str, sizeof(oid_str), blob_oid);
                              // Debug("blob: {} {}", oid_str, s);
                              }
                        }
                  }
            // Aufräumen für die nächste Iteration
            git_diff_free(diff);
            git_tree_free(parent_tree);
            git_commit_free(parent);
            git_tree_free(tree);
            git_commit_free(commit);
            }

      // Finale Bereinigung
      git_revwalk_free(walker);
      free((void*)file_path);
      }

//---------------------------------------------------------
//   getFile
//    oid   - BLOB OID
//---------------------------------------------------------

Lines Git::getFile(const git_oid* oid)
      {
      if (!initialized)
            return Lines();
      git_blob* blob = nullptr;

      if (!check_error(git_blob_lookup(&blob, repo, oid), "Blob-Lookup")) {
            char oid_str[GIT_OID_HEXSZ + 1];
            git_oid_tostr(oid_str, sizeof(oid_str), oid);
            Debug("blob: {}", oid_str);
            return Lines("\n\ninvalid BLOB\n\n");
            }

      const void* content = git_blob_rawcontent(blob);
//      Debug("BLOB <{}>", (char*)content);
      git_off_t size      = git_blob_rawsize(blob);

      Lines l(QString::fromUtf8((const char*)content, size));
      for (const auto& ll : l)
            Debug("<{}>", ll.qstring());
      Debug("Lines == {}", l.size());

      git_blob_free(blob);
      return l;
      }

//---------------------------------------------------------
//   updateGitHistory
//---------------------------------------------------------

void Editor::updateGitHistory()
      {
      if (!kontext() || !kontext()->file())
            return;
      QString fileName = kontext()->file()->fileName();
      _git.getHistory(fileName, kontext()->file()->gitHistory());
      gitList.set(kontext()->file()->gitHistory());
      gitPanel->setCurrentIndex(gitList.index(kontext()->file()->currentGitHistory(), 0));
      update();
      }

//---------------------------------------------------------
//   showGitVersion
//---------------------------------------------------------

void File::showGitVersion(int row)
      {
      _currentGitHistory = row;
      if (row == 0) {
            editor->kontext()->setViewMode(ViewMode::File);
            }
      else {
            auto history = gitHistory(row);
            auto oid     = history->oid;
            char oid_str[GIT_OID_HEXSZ + 1];
            git_oid_tostr(oid_str, sizeof(oid_str), &oid);
            Lines lines = editor->git()->getFile(&oid);

            _gitVersion = lines;
            if (_viewMode == ViewMode::GitVersion) {
                  makePretty();
                  editor->editWidget()->update();
                  }
            if (editor->kontext()->viewMode() != ViewMode::GitVersion)
                  editor->kontext()->setViewMode(ViewMode::GitVersion);
            makePretty();
            }
      }
