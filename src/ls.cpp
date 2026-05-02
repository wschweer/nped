//=============================================================================
//  nped Program Editor
//
//  Copyright (C) 2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include <QJsonArray>
#include <QJsonObject>
#include "logger.h"
#include "ls.h"

//---------------------------------------------------------
//   serialize TextStyles
//---------------------------------------------------------

void LanguageServersConfig::fromJson(const QJsonArray& array) {
      clear();
      for (int i = 0; i < array.size(); ++i) {
            QJsonObject o = array[i].toObject();
            LanguageServerConfig s;
            s.name    = o["name"].toString();
            s.command = o["command"].toString();
            s.args    = o["args"].toString();
            append(s);
            }
      }

QJsonArray LanguageServersConfig::toJson() const {
      QJsonArray array;
      for (const auto& s : *this) {
            QJsonObject o;
            o["name"]    = s.name;
            o["command"] = s.command;
            o["args"]    = s.args;
            array.append(o);
            }
      return array;
      }

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void LanguageServersConfig::reset() {
      clear();

      for (const auto& ft :
                 {LanguageServerConfig("clangd", "clangd",
                                 "--query-driver --log=error --completion-style=detailed "
                                 "--compile-commands-dir=build --background-index"),
            LanguageServerConfig("vscode-html", "vscode-html-languageserver", "--stdio"),
            LanguageServerConfig("pylsp", "pylsp", ""), LanguageServerConfig("qmlls", "qmlls", "")}) {
            push_back(ft);
            }
      }
