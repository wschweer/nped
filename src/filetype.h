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

#pragma once

#include <QObject>

//---------------------------------------------------------
//   FileType
//---------------------------------------------------------

struct FileType {
      Q_GADGET
      Q_PROPERTY(QString extensions MEMBER extensions)
      Q_PROPERTY(QString languageId MEMBER languageId)
      Q_PROPERTY(QString languageServer MEMBER languageServer)
      Q_PROPERTY(int tabSize MEMBER tabSize)
      Q_PROPERTY(bool header MEMBER header)
      Q_PROPERTY(bool createTabs MEMBER createTabs)

   public:
      QString extensions;
      QString languageId; // language id to select the right language server
      QString languageServer;
      int tabSize{6};    // tab expansion

      bool header{false};     // special handling of header files
      bool createTabs{false}; // spaces are converted to tabs when writing the file
                              // on reading tabs are converted to spaces

      bool operator==(const FileType& other) const = default;

      FileType() {}
      FileType(const QString& a, const QString& b, const QString& c,  int d, bool f, bool g)
         : extensions(a), languageId(b), languageServer(c), tabSize(d), header(f), createTabs(g) {}

      };

static const FileType defaultFileType = FileType(QString(), QString(), QString("none"), 6, false, false);

//---------------------------------------------------------
//   FileTypes
//---------------------------------------------------------

class FileTypes : public QList<FileType> {
   public:
      using QList<FileType>::QList;
      void fromJson(const QJsonArray& array);
      QJsonArray toJson() const;
      void reset();
      };

Q_DECLARE_METATYPE(FileType)
Q_DECLARE_METATYPE(FileTypes)
