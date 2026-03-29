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
         TextStyle("normal", QColor(0, 0, 0), QColor(0xe8, 0xe8, 0xe8), false, false),
         TextStyle("selected", QColor(), QColor(0xb9, 0xb9, 0xb9), false, false),
         TextStyle("cursor", QColor(255, 255, 255), QColor(50, 50, 50), false, false),
         TextStyle("flow", QColor(0, 0, 150), QColor(), true, false),
         TextStyle("type", QColor(0, 0, 150), QColor(), false, false),
         TextStyle("comment", QColor(0, 100, 0), QColor(), false, true),
         TextStyle("string", QColor(150, 0, 0), QColor(), false, false),
         TextStyle("search", QColor(255, 255, 255), QColor(120, 120, 120), false, false),
         TextStyle("searchHit", QColor(0, 0, 0), QColor(0xc9, 0xc9, 0xc9), false, false),
         TextStyle("gutter", QColor(0, 0, 0, 0), QColor(100, 100, 100, 255), false, false),
         TextStyle("marked Line", QColor(0, 0, 0, 0), QColor(100, 100, 100, 255), false, false),
         TextStyle("nontext BG", QColor(), QColor(0xd0, 0xd0, 0xd0), false, false),
            };
      if (_textStylesLight != tsLight) {
            _textStylesLight = tsLight;
            emit textStylesLightChanged();
            }

      static const TextStyles tsDark = {
         TextStyle("normal", QColor(255, 255, 255), QColor(), false, false),
         TextStyle("selected", QColor(255, 255, 255), QColor(150, 150, 150), false, false),
         TextStyle("cursor", QColor(0, 0, 0), QColor(), false, false),
         TextStyle("flow", QColor(100, 150, 255), QColor(), true, false),
         TextStyle("type", QColor(200, 200, 100), QColor(), false, false),
         TextStyle("comment", QColor(100, 200, 100), QColor(), false, true),
         TextStyle("string", QColor(200, 100, 100), QColor(), false, false),
         TextStyle("search", QColor(0, 0, 0), QColor(150, 150, 150), false, false),
         TextStyle("searchHit", QColor(0, 0, 0), QColor(150, 150, 220), false, false),
         TextStyle("gutter", QColor(0, 0, 0, 0), QColor(100, 100, 100, 255), false, false),
         TextStyle("markedLine", QColor(), QColor(), false, false),
         TextStyle("nontextBG", QColor(), QColor(), false, false),
            };
      if (_textStylesDark != tsDark) {
            _textStylesDark = tsDark;
            textStylesDarkChanged();
            }

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

      configs["textStylesLight"] = _textStylesLight.toJson();
      configs["textStylesDark"]  = _textStylesDark.toJson();
      configs["models"]          = _models.toJson();
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
      if (config.contains("models"))
            _models.fromJson(config["models"].toArray());
      if (config.contains("fileTypes"))
            _fileTypes.fromJson(config["fileTypes"].toArray());
      if (config.contains("languageServers"))
            _languageServersConfig.fromJson(config["languageServers"].toArray());
      if (config.contains("textStylesLight")) {
            _textStylesLight.fromJson(config["textStylesLight"].toArray());
            emit textStylesLightChanged();
            }
      if (config.contains("fontSize")) {
            double n     = config["fontSize"].toDouble();
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

ConfigDialogWrapper::ConfigDialogWrapper(Editor* editor, QWidget* parent) : QWidget(parent), _quickWidget(new QQuickWidget(this)) {
      // Layout erstellen, damit das QQuickWidget den ganzen Dialog füllt
      auto layout = new QVBoxLayout(this);
      layout->setContentsMargins(0, 0, 0, 0);
      layout->setSizeConstraint(QLayout::SetFixedSize);
      layout->addWidget(_quickWidget);

      // Wichtig: Resize-Modus, damit sich QML dem Dialog anpasst
      // enum ResizeMode { SizeViewToRootObject, SizeRootObjectToView };
      _quickWidget->setResizeMode(QQuickWidget::SizeViewToRootObject);

      // Zugriff auf diesen Dialog für QML ermöglichen (für close/accept/reject)
      _quickWidget->rootContext()->setContextProperty("dialog", this);
      _quickWidget->rootContext()->setContextProperty("nped", editor);
      _quickWidget->rootContext()->setContextProperty("Editor", editor);
      _quickWidget->rootContext()->setContextProperty("agent", editor->getAgent());

      // Lade die QML Datei aus dem Modul "Nped.Config"
      // Hinweis: Durch die CMake Policy QTP0001 ist der Pfad standardisiert:
      _quickWidget->setSource(QUrl("qrc:/qt/qml/Nped/Config/qml/ConfigDialog.qml"));
      }

//---------------------------------------------------------
//   resizeEvent
//---------------------------------------------------------

void ConfigDialogWrapper::resizeEvent(QResizeEvent* event) {
      QWidget::resizeEvent(event);
      if (parentWidget()) {
            QPoint center = parentWidget()->rect().center();
            QSize mySize  = size();
            move(center.x() - mySize.width() / 2, center.y() - mySize.height() / 2);
            }
      _quickWidget->setFocus();
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
