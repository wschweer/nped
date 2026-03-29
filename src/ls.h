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

class LSclient;

//---------------------------------------------------------
//   LanguageServerConfig
//---------------------------------------------------------

struct LanguageServerConfig {
      Q_GADGET
      Q_PROPERTY(QString name MEMBER name)
      Q_PROPERTY(QString command MEMBER command)
      Q_PROPERTY(QString args MEMBER args)

    public:
      QString name;
      QString command;
      QString args;
      bool operator==(const LanguageServerConfig& other) const = default;
      };

//---------------------------------------------------------
//   LanguageServersConfig
//---------------------------------------------------------

class LanguageServersConfig : public QList<LanguageServerConfig> {
   public:
      using QList<LanguageServerConfig>::QList;
      void fromJson(const QJsonArray& array);
      QJsonArray toJson() const;
      void reset();
      };

//---------------------------------------------------------
//   LanguageServer
//---------------------------------------------------------

struct LanguageServer {
      QString name;
      LSclient* client{nullptr};
      };

//---------------------------------------------------------
//   LanguageServerList
//---------------------------------------------------------

struct LanguageServerList : public std::vector<LanguageServer> {
      };

Q_DECLARE_METATYPE(LanguageServerConfig)
