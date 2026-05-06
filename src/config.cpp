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
#include <QFontDatabase>
#include <QSettings>
#include <QScreen>
#include <QGuiApplication>
#include <QDir>
#include "editor.h"
#include "kontext.h"
#include "logger.h"
#include "filetype.h"
#include "configwebview.h"
#include "editwin.h"

#include <QScrollBar>
#include <QStackedWidget>
//---------------------------------------------------------
//   loadDefaults
//---------------------------------------------------------

void Editor::resetToDefaults() {
      _fileTypes.reset();
      _languageServersConfig.reset();
      _mcpServersConfig.clear();
      {
            McpServerConfig githubConfig;
            githubConfig.id      = "github";
            githubConfig.command = "npx";
            githubConfig.args    = "-y @modelcontextprotocol/server-github";
            githubConfig.enabled = true;
            _mcpServersConfig.append(githubConfig);
      }

      static const TextStyles tsLight = {
         TextStyle("normal", QColor("#ff000000"), QColor("#ffdcdcdc"), false, false),
         TextStyle("selected", QColor("#00000000"), QColor("#ffb9b9b9"), false, false),
         TextStyle("cursor", QColor("#ffffffff"), QColor("#ff323232"), false, false),
         TextStyle("flow", QColor("#ff0404fe"), QColor("#00000000"), true, false),
         TextStyle("type", QColor("#ff000096"), QColor("#00000000"), false, false),
         TextStyle("comment", QColor("#ff006400"), QColor("#00000000"), false, true),
         TextStyle("string", QColor("#ff960000"), QColor("#00000000"), false, false),
         TextStyle("search", QColor("#ff000000"), QColor("#ff000000"), false, false),
         TextStyle("searchHit", QColor("#ff000000"), QColor("#ffeef81b"), false, false),
         TextStyle("gutter", QColor("#ff000000"), QColor("#ffb2b2b2"), false, false),
         TextStyle("marked Line", QColor("#00000000"), QColor("#ffdde71f"), false, false),
         TextStyle("nontext BG", QColor("#00000000"), QColor("#ffe8e8e8"), false, false),
      };
      if (_textStylesLight != tsLight) {
            _textStylesLight = tsLight;
            emit textStylesLightChanged();
      }

      static const TextStyles tsDark = {
         TextStyle("normal", QColor("#ffd3d3d3"), QColor("#ff333333"), false, false),
         TextStyle("selected", QColor("#05ff1d1d"), QColor("#ff575757"), false, false),
         TextStyle("cursor", QColor("#ff000000"), QColor("#ffffffff"), false, false),
         TextStyle("flow", QColor("#ff91b0f0"), QColor("#00000000"), true, false),
         TextStyle("type", QColor("#ff9ac864"), QColor("#00000000"), false, false),
         TextStyle("comment", QColor("#ff64c864"), QColor("#00000000"), false, true),
         TextStyle("string", QColor("#ffc86464"), QColor("#00000000"), false, false),
         TextStyle("search", QColor("#ffffffff"), QColor("#ff814d4d"), false, false),
         TextStyle("searchHit", QColor("#fff11515"), QColor("#ff9696dc"), false, false),
         TextStyle("gutter", QColor("#003fdcdd"), QColor("#ff7c8e8d"), false, false),
         TextStyle("markedLine", QColor("#ff000000"), QColor("#6ca8b201"), false, false),
         TextStyle("nontextBG", QColor("#ff000000"), QColor("#ff262626"), false, false),
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
//   updateShortcut
//---------------------------------------------------------

void Editor::updateShortcut(const QString& id, const QString& sequence) {
      for (auto& [cmd, sc] : _shortcuts) {
            if (sc.id == id) {
                  if (sc.sequence != sequence) {
                        sc.sequence = sequence;
                        emit shortcutsChanged();
                  }
                  return;
            }
      }
}
//---------------------------------------------------------
//   showConfig
//    Show the config page stacked on top of the editor.
//    The config page is at stack index 2.
//---------------------------------------------------------

void Editor::showConfig() {
      if (!_configWebView) {
            _configWebView = new ConfigWebView(this, this);

            _configContainer          = new QWidget(this);
            QVBoxLayout* configLayout = new QVBoxLayout(_configContainer);
            configLayout->setContentsMargins(0, 0, 0, 0);
            configLayout->setSpacing(0);
            configLayout->addWidget(_configWebView, 1);

            _stack->addWidget(_configContainer);

            connect(_configWebView, &ConfigWebView::configSaved, this, [this] {
                  hideConfig();
                  initFont();
                  saveSettings();
                  emit configApplied(); //??
            });
            connect(_configWebView, &ConfigWebView::configCancelled, this, [this] { hideConfig(); });
            connect(_configWebView, &ConfigWebView::configResetRequested, this, [this] {
                  resetToDefaults();
                  // Re-open config to show reset values
                  _configWebView->openConfig();
            });
      }
      _configWebView->openConfig();
      _stack->setCurrentWidget(_configContainer);
      configButton->setChecked(true);
      vScroll->setVisible(false);
}
//---------------------------------------------------------
//   hideConfig
//    Return to the normal editor view.
//---------------------------------------------------------

void Editor::hideConfig() {
      _stack->setCurrentWidget(_editWidget);
      configButton->setChecked(false);
      _editWidget->setFocus();
      vScroll->setVisible(true);
}
//---------------------------------------------------------
//   toJson
//---------------------------------------------------------

static QJsonArray toJson(const TextStyles& ts) {
      QJsonArray array;
      for (const auto& s : ts) {
            QJsonObject o;
            o["name"]   = s.name;
            o["fg"]     = s.fg.name(QColor::HexArgb);
            o["bg"]     = s.bg.name(QColor::HexArgb);
            o["italic"] = s.italic;
            o["bold"]   = s.bold;
            array.append(o);
      }
      return array;
}
//---------------------------------------------------------
//   tsFromJson
//---------------------------------------------------------

static TextStyles tsFromJson(const QJsonArray& array) {
      TextStyles ts;
      for (int i = 0; i < array.size(); ++i) {
            QJsonObject o = array[i].toObject();
            TextStyle s;
            s.name   = o["name"].toString();
            s.fg     = QColor::fromString(o["fg"].toString());
            s.bg     = QColor::fromString(o["bg"].toString());
            s.italic = o["italic"].toBool();
            s.bold   = o["bold"].toBool();
            ts.append(s);
      }
      return ts;
}
//---------------------------------------------------------
//   saveSettings
//---------------------------------------------------------

void Editor::saveSettings() {
      QJsonObject configs;

      Debug("save settings");
      configs["darkMode"]        = darkMode();
      configs["textStylesLight"] = toJson(_textStylesLight);
      configs["textStylesDark"]  = toJson(_textStylesDark);

      auto s                     = toJson(_models).dump();
      auto doc                   = QJsonDocument::fromJson(s.c_str());
      configs["models"]          = doc.array();
      configs["fileTypes"]       = fileTypes().toJson();
      configs["languageServers"] = languageServersConfig().toJson();
      auto mcp_s                 = mcpServerConfigsToJson(_mcpServersConfig).dump();
      configs["mcpServers"]      = QJsonDocument::fromJson(mcp_s.c_str()).array();

      QJsonArray ar;
      for (const auto& agentRole : _agentRoles) {
            QJsonObject r;
            r["name"]       = agentRole.name;
            r["manifest"]   = agentRole.manifest;
            r["rw"]         = agentRole.rw;
            r["mcpServers"] = QJsonArray::fromStringList(agentRole.mcpServers);
            ar.append(r);
      }
      configs["agentRoles"] = ar;

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

      QJsonArray cprompts;
      for (const auto& cp : _cannedPrompts) {
            QJsonObject o;
            o["name"]        = cp.name;
            o["description"] = cp.description;
            o["prompt"]      = cp.prompt;
            cprompts.append(o);
      }
      configs["cannedPrompts"] = cprompts;

      QJsonArray projects;
      int i = 0;
      for (const auto& p : _projects) {
            QJsonObject project;
            project["path"] = p;
            projects.append(project);
            ++i;
            if (i == 5) // save only last 5 project names
                  break;
      }
      configs["projects"] = projects;

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
      if (config.contains("projects")) {
            QJsonArray sc = config["projects"].toArray();
            for (int i = 0; i < sc.size(); ++i) {
                  QJsonObject obj = sc[i].toObject();
                  QString path    = obj["path"].toString();
                  _projects.push_back(path);
            }
      }

      if (config.contains("cannedPrompts")) {
            _cannedPrompts.clear();
            QJsonArray sc = config["cannedPrompts"].toArray();
            for (int i = 0; i < sc.size(); ++i) {
                  QJsonObject obj = sc[i].toObject();
                  CannedPrompt cp;
                  cp.name        = obj["name"].toString();
                  cp.description = obj["description"].toString();
                  cp.prompt      = obj["prompt"].toString();
                  _cannedPrompts.push_back(cp);
            }
      }

      if (config.contains("agentRoles")) {
            _agentRoles.clear();
            QJsonArray sc = config["agentRoles"].toArray();
            for (int i = 0; i < sc.size(); ++i) {
                  QJsonObject obj = sc[i].toObject();
                  AgentRole ar;
                  ar.name     = obj["name"].toString();
                  ar.manifest = obj["manifest"].toString();
                  ar.rw       = obj["rw"].toBool();

                  QJsonArray mcpArray = obj["mcpServers"].toArray();
                  for (int j = 0; j < mcpArray.size(); ++j)
                        ar.mcpServers.append(mcpArray[j].toString());

                  _agentRoles.push_back(ar);
            }
      }

      if (config.contains("darkMode"))
            set_darkMode(config["darkMode"].toBool());
      if (config.contains("models")) { // hack until we converted all to lohmann json
            auto ma       = config["models"].toArray();
            QByteArray ba = QJsonDocument(ma).toJson();
            json json     = json::parse(ba.data());
            _models       = fromJson(json);
      }
      if (config.contains("fileTypes"))
            _fileTypes.fromJson(config["fileTypes"].toArray());
      if (config.contains("languageServers"))
            _languageServersConfig.fromJson(config["languageServers"].toArray());
      if (config.contains("mcpServers")) {
            auto mcp_arr = config["mcpServers"].toArray();
            _mcpServersConfig =
                mcpServerConfigsFromJson(json::parse(QJsonDocument(mcp_arr).toJson().toStdString()));
      }
      if (config.contains("textStylesLight")) {
            _textStylesLight = tsFromJson(config["textStylesLight"].toArray());
            emit textStylesLightChanged();
      }
      //      if (config.contains("fontSize")) {
      //            double n  = config["fontSize"].toDouble();
      //            _fontSize = std::clamp(n, 5.0, 40.0); // sanitize value
      //            }
      if (config.contains("fontFamily"))
            set_fontFamily(config["fontFamily"].toString());
      if (config.contains("textStylesDark")) {
            _textStylesDark = tsFromJson(config["textStylesDark"].toArray());
            emit textStylesDarkChanged();
      }

      if (_agentRoles.isEmpty()) {
            QStringList allMcpServers;
            for (const auto& mcp : _mcpServersConfig)
                  allMcpServers.append(mcp.id);
            AgentRoles defaultRoles = {
               { "C++Coder",    "You are a high-performace c++ coding engine.\nUse modern C++23.",  true,
                allMcpServers                                                                                          },
               {"Architect", "You are an experienced C++ developer acting as a system architect.", false,
                allMcpServers                                                                                          },
               {    "Agent",                                  "You are a friendly helpful agent.",  true, allMcpServers}
            };
            _agentRoles = defaultRoles;
      }
      if (_cannedPrompts.isEmpty()) {
            CannedPrompts defaultPrompts = {
               {"Refactor", "Refactor the selected code",
                "Refactor the following code to improve readability and performance."            },
               { "Explain",  "Explain the selected code", "Explain how the following code works."}
            };
            _cannedPrompts = defaultPrompts;
      }
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
