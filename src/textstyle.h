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

#include <QWidget>
#include <QString>
#include <QColor>

//---------------------------------------------------------
//   TextStyle
//---------------------------------------------------------

struct TextStyle {
      Q_GADGET
      Q_PROPERTY(QString name MEMBER name)
      Q_PROPERTY(QColor fg MEMBER fg)
      Q_PROPERTY(QColor bg MEMBER bg)
      Q_PROPERTY(bool bold MEMBER bold)
      Q_PROPERTY(bool italic MEMBER italic)

    public:
      QString name;
      QColor fg;
      QColor bg;
      bool bold;
      bool italic;
      bool operator==(const TextStyle& other) const = default;
      enum Style { Normal, Selection, Cursor, Flow, Type, Comment, String, Search, SearchHit };
      };

Q_DECLARE_METATYPE(TextStyle)
