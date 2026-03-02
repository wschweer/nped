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
#include <unistd.h>
#include <signal.h>
#include <QActionGroup>
#include <QApplication>
#include <QCompleter>
#include <QDir>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QMenu>
#include <QMenuBar>
#include <QShortcut>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>

#include "editor.h"
#include "file.h"
#include "logger.h"

enum Codec readCodec  = Codec::UTF8;
enum Codec writeCodec = Codec::UTF8;
bool persistent       = true;

static const char* appName;

//---------------------------------------------------------
//   printVersion
//---------------------------------------------------------

static void printVersion(const char* name) {
      printf("%s: Version 1.0\n", name);
      }

//---------------------------------------------------------
//   usage
//---------------------------------------------------------

static void usage(const char* reason) {
      if (reason)
            printf("%s: %s\n", appName, reason);
      printf("usage: %s [options] file[s]\n", appName);
      printf("options: -v   print version\n"
             "         -l   use iso latin1 codec\n"
             "         -u   use utf8 codec (default)\n"
             "         -i   change file 'in place'\n"
             "         -t   do not remember settings\n");
      }

//---------------------------------------------------------
//   main
//---------------------------------------------------------

int main(int argc, char** argv) {
      int c;

      signal(SIGPIPE, SIG_IGN);

      QApplication app(argc, argv);
      app.setDesktopFileName("nped");
      appName = argv[0];

      QFile file(":/style.qss");
      if (file.open(QFile::ReadOnly)) {
            QTextStream stream(&file);
            qApp->setStyleSheet(stream.readAll());
            file.close();
            }

      while ((c = getopt(argc, argv, "vluit")) != EOF) {
            switch (c) {
                  case 'v': printVersion(argv[0]); return 1;
                  case 'l':
                        writeCodec = Codec::ISO_LATIN;
                        readCodec  = Codec::ISO_LATIN;
                        break;
                  case 'L':
                        writeCodec = Codec::UTF8;
                        readCodec  = Codec::ISO_LATIN;
                        break;
                  case 'u':
                        readCodec  = Codec::UTF8;
                        writeCodec = Codec::UTF8;
                        break;
                  case 't': persistent = false; break;
                  default: usage("bad argument"); return -1;
                  }
            }
      argc -= optind;
      argv += optind;

      Editor e(argc, argv);
      e.show();
      app.exec();
      return 0;
      }