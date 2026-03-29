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

#include "textstyle.h"

//---------------------------------------------------------
//   serialize TextStyles
//---------------------------------------------------------

QJsonArray TextStyles::toJson() const {
      QJsonArray array;
      for (const auto& s : *this) {
            QJsonObject o;
            o["name"] = s.name;
            o["fg"] = s.fg.name(QColor::HexArgb);
            o["bg"] = s.bg.name(QColor::HexArgb);
            o["italic"] = s.italic;
            o["bold"] = s.bold;
            array.append(o);
            }
      return array;
      }

void TextStyles::fromJson(const QJsonArray& array) {
      clear();
      for (int i = 0; i < array.size(); ++i) {
            QJsonObject o = array[i].toObject();
            TextStyle s;
            s.name = o["name"].toString();
            s.fg   = QColor(o["fg"].toString());
            s.bg   = QColor(o["bg"].toString());
            s.italic = o["italic"].toBool();
            s.bold = o["bold"].toBool();
            append(s);
            }
      }
