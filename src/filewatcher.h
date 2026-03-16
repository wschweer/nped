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

#include <QObject>
#include <QSocketNotifier>
#include <QMap>
#include <sys/inotify.h>

//---------------------------------------------------------
//   FileWatcher
//---------------------------------------------------------

class FileWatcher : public QObject
      {
      Q_OBJECT

      int m_inotifyFd;
      QSocketNotifier* notifier;
      QMap<int, QString> watchMap; // Verknüpft Watch-ID mit Pfad

    private slots:
      void handleInotifyEvent();

    signals:
      void fileChanged(const QString& path);
      void fileDeleted(const QString& path);

    public:
      explicit FileWatcher(QObject* parent = nullptr);
      ~FileWatcher();

      bool addWatch(const QString& path);
      void removeWatch(const QString& path);

      };
