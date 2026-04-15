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

#include "filetype.h"
#include <QJsonArray>
#include <QJsonObject>

//---------------------------------------------------------
//   fromJson
//---------------------------------------------------------

void FileTypes::fromJson(const QJsonArray& array) {
      clear();
      for (int i = 0; i < array.size(); ++i) {
            QJsonObject o = array[i].toObject();
            FileType s;
            s.extensions     = o["extensions"].toString();
            s.languageId     = o["languageId"].toString();
            s.languageServer = o["languageServer"].toString();
            s.tabSize        = o["tabSize"].toInt();
            s.header         = o.contains("isHeader") ? o["isHeader"].toBool() : false;
            s.createTabs     = o.contains("hwTabs") ? o["hwTabs"].toBool() : false;
            append(s);
            }
      }

//---------------------------------------------------------
//   toJson
//---------------------------------------------------------

QJsonArray FileTypes::toJson() const {
      QJsonArray array;
      for (const auto& s : *this) {
            QJsonObject o;
            o["extensions"]     = s.extensions;
            o["languageId"]     = s.languageId;
            o["languageServer"] = s.languageServer;
            o["tabSize"]        = s.tabSize;
            o["header"]         = s.header;
            o["hwTabs"]         = s.createTabs;
            array.append(o);
            }
      return array;
      }

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void FileTypes::reset() {
      clear();

      for (const auto& ft : {
         FileType(".*\\.cpp$", "cpp", "clangd", 6, false, false),
         FileType(".*\\.c$", "c", "clangd", 6, false, false),
         FileType(".*\\.html$", "html", "vscode-html", 4, false, false),
         FileType(".*\\.h$", "cpp", "clangd", 6, true, false),
         FileType(".*\\.py$", "python", "pylsp", 6, false, false),
         FileType(".*\\.qml$", "qml", "qmlls", 4, false, false),
         FileType(".*\\.md$", "markdown", "none", 6, false, false),
         FileType("Makefile$", "makefile", "none", 6, false, true),
         FileType(".*\\.(jpg|jpeg|png|gif|svg|webp|pdf)$", "image", "none", 6, false, false)
            }) {
            push_back(ft);
            }
      }
