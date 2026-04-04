#pragma once
#include <QString>
#include <vector>
#include <QList>
#include <QJsonArray>
#include <QMetaType>

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

Q_DECLARE_METATYPE(LanguageServerConfig)

//---------------------------------------------------------
//   LanguageServer
//---------------------------------------------------------

struct LanguageServer {
      LanguageServer() = default;
      LanguageServer(const QString& n, LSclient* c) : name(n), client(c) {}
      
      bool operator==(const LanguageServer& other) const { return name == other.name; }

      QString name;
      LSclient* client{nullptr};
      };

Q_DECLARE_METATYPE(LanguageServer)

//---------------------------------------------------------
//   LanguageServerList
//---------------------------------------------------------

struct LanguageServerList : public std::vector<LanguageServer> {
      };
