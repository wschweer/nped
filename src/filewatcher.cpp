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

#include <unistd.h>
#include <errno.h>
#include "filewatcher.h"

//---------------------------------------------------------
//   FileWatcher
//---------------------------------------------------------

FileWatcher::FileWatcher(QObject* parent) : QObject(parent) {
      // 1. inotify initialisieren (non-blocking ist hier wichtig für Qt)
      m_inotifyFd = inotify_init1(IN_NONBLOCK);

      if (m_inotifyFd == -1) {
            qCritical("Fehler beim Initialisieren von inotify: %s", strerror(errno));
            return;
            }

      // 2. QSocketNotifier in den Qt Event Loop hängen
      notifier = new QSocketNotifier(m_inotifyFd, QSocketNotifier::Read, this);
      connect(notifier, &QSocketNotifier::activated, this, &FileWatcher::handleInotifyEvent);
      }

FileWatcher::~FileWatcher() {
      if (m_inotifyFd != -1)
            close(m_inotifyFd);
      }

//---------------------------------------------------------
//   addWatch
//---------------------------------------------------------

bool FileWatcher::addWatch(const QString& path) {
      // Wir überwachen Änderungen, Löschen und Verschieben
      uint32_t mask = IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_ATTRIB;
      int wd        = inotify_add_watch(m_inotifyFd, path.toUtf8().constData(), mask);

      if (wd == -1)
            return false;

      watchMap.insert(wd, path);
      return true;
      }

//---------------------------------------------------------
//   handleInotifyEvent
//---------------------------------------------------------

void FileWatcher::handleInotifyEvent() {
      // Puffer für Events (inotify_event + Name falls vorhanden)
      char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

      ssize_t len = read(m_inotifyFd, buffer, sizeof(buffer));
      if (len <= 0)
            return;

      char* ptr = buffer;
      while (ptr < buffer + len) {
            struct inotify_event* event = reinterpret_cast<struct inotify_event*>(ptr);

            if (watchMap.contains(event->wd)) {
                  QString path = watchMap.value(event->wd);

                  if (event->mask & (IN_MODIFY | IN_ATTRIB))
                        emit fileChanged(path);

                  if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                        emit fileDeleted(path);
                        // Watch automatisch entfernen, da Datei weg ist
                        inotify_rm_watch(m_inotifyFd, event->wd);
                        watchMap.remove(event->wd);
                        }
                  }
            ptr += sizeof(struct inotify_event) + event->len;
            }
      }
