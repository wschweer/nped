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

#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVBoxLayout>
#include <QQmlContext>
#include <QDebug>
#include <QFontDatabase>
#include <QSettings>
#include <QScreen>
#include <QGuiApplication>
#include <QDir>
#include "editor.h"
#include "kontext.h"
#include "logger.h"
#include "filetype.h"

//---------------------------------------------------------
//   loadDefaults
//---------------------------------------------------------

void Editor::resetToDefaults() {
      _fileTypes.reset();
      _languageServersConfig.reset();

      static const TextStyles tsLight = {
         TextStyle("normal",        QColor("#ff000000"), QColor("#ffdcdcdc"), false, false),
         TextStyle("selected",      QColor("#00000000"), QColor("#ffb9b9b9"), false, false),
         TextStyle("cursor",        QColor("#ffffffff"), QColor("#ff323232"), false, false),
         TextStyle("flow",          QColor("#ff0404fe"), QColor("#00000000"), true, false),
         TextStyle("type",          QColor("#ff000096"), QColor("#00000000"), false, false),
         TextStyle("comment",       QColor("#ff006400"), QColor("#00000000"), false, true),
         TextStyle("string",        QColor("#ff960000"), QColor("#00000000"), false, false),
         TextStyle("search",        QColor("#ff000000"), QColor("#ff000000"), false, false),
         TextStyle("searchHit",     QColor("#ff000000"), QColor("#ffeef81b"), false, false),
         TextStyle("gutter",        QColor("#ff000000"), QColor("#ffb2b2b2"), false, false),
         TextStyle("marked Line",   QColor("#00000000"), QColor("#ffdde71f"), false, false),
         TextStyle("nontext BG",    QColor("#00000000"), QColor("#ffe8e8e8"), false, false),
            };
      if (_textStylesLight != tsLight) {
            _textStylesLight = tsLight;
            emit textStylesLightChanged();
            }

      static const TextStyles tsDark = {
         TextStyle("normal",        QColor("#ffd3d3d3"), QColor("#ff333333"), false, false),
         TextStyle("selected",      QColor("#05ff1d1d"), QColor("#ff575757"), false, false),
         TextStyle("cursor",        QColor("#ff000000"), QColor("#ffffffff"), false, false),
         TextStyle("flow",          QColor("#ff91b0f0"), QColor("#00000000"), true, false),
         TextStyle("type",          QColor("#ff9ac864"), QColor("#00000000"), false, false),
         TextStyle("comment",       QColor("#ff64c864"), QColor("#00000000"), false, true),
         TextStyle("string",        QColor("#ffc86464"), QColor("#00000000"), false, false),
         TextStyle("search",        QColor("#ffffffff"), QColor("#ff814d4d"), false, false),
         TextStyle("searchHit",     QColor("#fff11515"), QColor("#ff9696dc"), false, false),
         TextStyle("gutter",        QColor("#003fdcdd"), QColor("#ff7c8e8d"), false, false),
         TextStyle("markedLine",    QColor("#ff000000"), QColor("#6ca8b201"), false, false),
         TextStyle("nontextBG",     QColor("#ff000000"), QColor("#ff262626"), false, false),
            };
      if (_textStylesDark != tsDark) {
            _textStylesDark = tsDark;
            textStylesDarkChanged();
            }
      _darkMode = false;
      emit shortcutsChanged();
      emit fileTypesChanged();
      emit languageServersConfigChanged();
      }

//---------------------------------------------------------
//   apply
//---------------------------------------------------------

void Editor::apply() {
      saveSettings();
      emit configApplied();
      }

//---------------------------------------------------------
//   saveSettings
//---------------------------------------------------------

void Editor::saveSettings() {
      QJsonObject configs;

      configs["darkMode"]        = darkMode();
      configs["textStylesLight"] = _textStylesLight.toJson();
      configs["textStylesDark"]  = _textStylesDark.toJson();
      auto s                     = _models.toJson().dump();
      auto doc                   = QJsonDocument::fromJson(s.c_str());
      configs["models"]          = doc.array();
      configs["fileTypes"]       = _fileTypes.toJson();
      configs["languageServers"] = _languageServersConfig.toJson();

      QJsonArray array;
      for (const auto& [id, m] : _shortcuts) {
            if (m.buildin == m.sequence)
                  continue;
            QJsonObject obj;
            obj["id"]       = m.id;
            obj["sequence"] = m.sequence;
            array.append(obj);
            }

      configs["shortcuts"]  = array;
      configs["fontFamily"] = fontFamily();
      configs["fontSize"]   = fontSize();

      QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
      QDir().mkpath(path);
      path += "/nped.json";
      QFile file(path);
      if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(configs).toJson());
            file.close();
            }
      }

//---------------------------------------------------------
//   loadSettings
//---------------------------------------------------------

void Editor::loadSettings() {
      QString path  = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
      path         += "/nped.json";
      QFile file(path);
      if (!file.open(QIODevice::ReadOnly)) {
            Debug("config file <{}> not found", path);
            return;
            }
      QByteArray s       = file.readAll();
      QJsonObject config = QJsonDocument::fromJson(s).object();

      QJsonArray sc = config["shortcuts"].toArray();
      for (int i = 0; i < sc.size(); ++i) {
            QJsonObject obj  = sc[i].toObject();
            QString id       = obj["id"].toString();
            QString sequence = obj["sequence"].toString();
            for (auto& [cmd, sc] : _shortcuts) {
                  if (sc.id == id) {
                        sc.sequence = sequence;
                        break;
                        }
                  }
            }
      if (config.contains("darkMode"))
            setDarkMode(config["darkMode"].toBool());
      if (config.contains("models")) {     // hack until we converted all to lohmann json
            auto ma = config["models"].toArray();
            QByteArray ba = QJsonDocument(ma).toJson();
            json json =  json::parse(ba.data());
            _models.fromJson(json);
            }
      if (config.contains("fileTypes"))
            _fileTypes.fromJson(config["fileTypes"].toArray());
      if (config.contains("languageServers"))
            _languageServersConfig.fromJson(config["languageServers"].toArray());
      if (config.contains("textStylesLight")) {
            _textStylesLight.fromJson(config["textStylesLight"].toArray());
            emit textStylesLightChanged();
            }
      if (config.contains("fontSize")) {
            double n  = config["fontSize"].toDouble();
            _fontSize = std::clamp(n, 5.0, 40.0); // sanitize value
            }
      if (config.contains("fontFamily"))
            setFontFamily(config["fontFamily"].toString());
      if (config.contains("textStylesDark")) {
            _textStylesDark.fromJson(config["textStylesDark"].toArray());
            emit textStylesDarkChanged();
            }
      }

//---------------------------------------------------------
//   ConfigDialogWrapper
//---------------------------------------------------------

ConfigDialogWrapper::ConfigDialogWrapper(Editor* editor, QWidget* parent) : QDialog(parent) {
      setModal(true);
      setSizeGripEnabled(true);
      _quickWidget = new QQuickWidget(this);
      _quickWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

      // Layout erstellen, damit das QQuickWidget den ganzen Dialog füllt
      auto layout = new QVBoxLayout(this);
      layout->setContentsMargins(0, 0, 0, 0);
      layout->addWidget(_quickWidget);

      // Wichtig: Resize-Modus, damit sich QML dem Dialog anpasst
      _quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

      // Zugriff auf diesen Dialog für QML ermöglichen (für close/accept/reject)
      _quickWidget->rootContext()->setContextProperty("dialog", this);
      _quickWidget->rootContext()->setContextProperty("nped", editor);

      // Lade die QML Datei aus dem Modul "Nped.Config"
      // Hinweis: Durch die CMake Policy QTP0001 ist der Pfad standardisiert:
      _quickWidget->setSource(QUrl("qrc:/qt/qml/Nped/Config/qml/ConfigDialog.qml"));
      }

//---------------------------------------------------------
//   monospacedFonts
//---------------------------------------------------------

QStringList Editor::monospacedFonts() const {
      const QStringList allFamilies = QFontDatabase::families();
      QStringList monoFamilies;

      for (const QString& family : allFamilies) {
            // Prüft, ob die Schriftart eine feste Breite hat
            if (QFontDatabase::isFixedPitch(family))
                  monoFamilies.append(family);
            }

      return monoFamilies;
      }
