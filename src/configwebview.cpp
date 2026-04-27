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

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QFile>
#include <QTimer>
#include <QUrlQuery>
#include <QFontDatabase>
#include <QWebChannel>

#include "configwebview.h"
#include "mcp.h"
#include <QMetaProperty>
#include <QMetaSequence>

#include "editor.h"
#include "logger.h"
#include "filetype.h"
#include "ls.h"

//---------------------------------------------------------
//   ConfigApi
//---------------------------------------------------------

class ConfigApi : public QObject
      {
      Q_OBJECT
      ConfigWebView* _view;

    public:
      explicit ConfigApi(ConfigWebView* view, QObject* parent = nullptr) : QObject(parent), _view(view) {}
    public slots:
      void saveConfig(const QString& jsonData) {
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromPercentEncoding(jsonData.toUtf8()));
            QJsonObject obj   = doc.object();

            for (auto it = obj.begin(); it != obj.end(); ++it)
                  _view->setProperty(it.key(), it.value().toVariant());
            emit _view->configSaved();
            }
      void cancelConfig() { emit _view->configCancelled(); }
      void resetConfig() { emit _view->configResetRequested(); }
      //---------------------------------------------------------
      //   listAdd
      //---------------------------------------------------------

      void listAdd(const QString& listName) {
            if (listName == "fileTypes") {
                  FileTypes ft = _view->editor()->fileTypes();
                  ft.append(FileType(".*\\.new$", "new", "none", 4, false, false));
                  _view->editor()->setFileTypes(ft);
                  _view->openConfig();
                  }
            else if (listName == "models") {
                  Models m = _view->editor()->models();
                  Model newModel;
                  newModel.name = "New Model";
                  newModel.api  = "openai";
                  m.append(newModel);
                  _view->editor()->setModels(m);
                  _view->openConfig();
                  }
            else if (listName == "agentRoles") {
                  AgentRoles ar = _view->editor()->agentRoles();
                  AgentRole newRole;
                  newRole.name     = "New Role";
                  newRole.manifest = "You are a helpful assistant.";
                  newRole.rw       = true;
                  ar.append(newRole);
                  _view->editor()->setAgentRoles(ar);
                  _view->openConfig();
                  }
            else if (listName == "languageServersConfig") {
                  auto lsc = _view->editor()->languageServersConfig();
                  LanguageServerConfig newLsc;
                  newLsc.name    = "new_ls";
                  newLsc.command = "ls_cmd";
                  newLsc.args    = "";
                  lsc.append(newLsc);
                  _view->editor()->setLanguageServersConfig(lsc);
                  _view->openConfig();
                  }
            else if (listName == "mcpServersConfig") {
                  McpServerConfigs mcp_list = _view->editor()->mcpServersConfig();
                  McpServerConfig newMcp;
                  newMcp.id = "new_mcp";
                  mcp_list.append(newMcp);
                  _view->editor()->setMcpServersConfig(mcp_list);
                  _view->openConfig();
                  }
            else {
                  Critical("unknown list <{}>", listName);
                  }
            }
      //---------------------------------------------------------
      //   listRemove
      //---------------------------------------------------------

      void listRemove(const QString& listName, int index) {
            if (listName == "fileTypes") {
                  FileTypes ft = _view->editor()->fileTypes();
                  if (index >= 0 && index < ft.size()) {
                        ft.removeAt(index);
                        _view->editor()->setFileTypes(ft);
                        _view->openConfig();
                        }
                  }
            else if (listName == "models") {
                  Models m = _view->editor()->models();
                  if (index >= 0 && index < m.size()) {
                        m.removeAt(index);
                        _view->editor()->setModels(m);
                        _view->openConfig();
                        }
                  }
            else if (listName == "agentRoles") {
                  AgentRoles ar = _view->editor()->agentRoles();
                  if (index >= 0 && index < ar.size()) {
                        ar.removeAt(index);
                        _view->editor()->setAgentRoles(ar);
                        _view->openConfig();
                        }
                  }
            else if (listName == "languageServersConfig") {
                  auto lsc = _view->editor()->languageServersConfig();
                  if (index >= 0 && index < lsc.size()) {
                        lsc.removeAt(index);
                        _view->editor()->setLanguageServersConfig(lsc);
                        _view->openConfig();
                        }
                  }
            else if (listName == "mcpServersConfig") {
                  McpServerConfigs mcp = _view->editor()->mcpServersConfig();
                  if (index >= 0 && index < mcp.size()) {
                        mcp.removeAt(index);
                        _view->editor()->setMcpServersConfig(mcp);
                        _view->openConfig();
                        }
                  }
            }
      };

//---------------------------------------------------------
//   ConfigWebView
//---------------------------------------------------------

ConfigWebView::ConfigWebView(Editor* editor, QWidget* parent) : MarkdownWebView(editor, parent) {
      // 1. Setup WebChannel
      QWebChannel* channel = new QWebChannel(page());
      ConfigApi* api       = new ConfigApi(this, channel);

      // 2. Register C++ api
      channel->registerObject(QStringLiteral("configApi"), api);
      page()->setWebChannel(channel);

      settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);

      // Set background to match theme
      bool dark = editor->darkMode();
      page()->setBackgroundColor(dark ? QColor("#222222") : QColor("#f5f5f5"));
      }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

QVariant ConfigWebView::getProperty(const QString& name) const {
      return editor()->property(name.toUtf8().constData());
      }

//---------------------------------------------------------
//   setVariant
//---------------------------------------------------------

void setVariant(QVariant item, const QString& propName, QVariant value) {
      int pidx = item.metaType().metaObject()->indexOfProperty(propName.toUtf8().constData());
      if (pidx >= 0) {
            QMetaProperty p   = item.metaType().metaObject()->property(pidx);
            QVariant valToSet = value;

            // Versuche, den Wert in den Zieltyp der Property zu konvertieren
            if (valToSet.convert(p.metaType()))
                  p.writeOnGadget(item.data(), valToSet);
            else
                  Critical("Konnte Property {} nicht in Typ {} konvertieren", p.name(), p.metaType().name());
            }
      else
            Critical("Gadget property not found");
      }

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

void ConfigWebView::setProperty(const QString& name, const QVariant& value) {
      if (name.contains('[')) {
            int b1  = name.indexOf('[');
            int b2  = name.indexOf(']');
            int dot = name.indexOf('.');
            if (b1 != -1 && b2 != -1 && dot != -1) {
                  QString listName = name.left(b1);
                  int index        = name.mid(b1 + 1, b2 - b1 - 1).toInt();
                  QString propName = name.mid(dot + 1);

                  if (listName == "fileTypes") {
                        auto list = editor()->fileTypes();
                        if (index >= 0 && index < list.size()) {
                              QVariant item = QVariant::fromValue(list[index]);
                              setVariant(item, propName, value);
                              list[index] = item.value<FileType>();
                              editor()->setFileTypes(list);
                              return;
                              }
                        }
                  else if (listName == "models") {
                        auto list = editor()->models();
                        if (index >= 0 && index < list.size()) {
                              QVariant item = QVariant::fromValue(list[index]);
                              setVariant(item, propName, value);
                              list[index] = item.value<Model>();
                              editor()->setModels(list);
                              return;
                              }
                        }
                  else if (listName == "agentRoles") {
                        auto list = editor()->agentRoles();
                        if (index >= 0 && index < list.size()) {
                              QVariant item = QVariant::fromValue(list[index]);
                              setVariant(item, propName, value);
                              list[index] = item.value<AgentRole>();
                              editor()->setAgentRoles(list);
                              return;
                              }
                        }
                  else if (listName == "textStylesDark") {
                        auto list = editor()->textStylesDark();
                        if (index >= 0 && index < list.size()) {
                              QVariant item = QVariant::fromValue(list[index]);
                              setVariant(item, propName, value);
                              list[index] = item.value<TextStyle>();
                              editor()->setTextStylesDark(list);
                              return;
                              }
                        }
                  else if (listName == "textStylesLight") {
                        auto list = editor()->textStylesLight();
                        if (index >= 0 && index < list.size()) {
                              QVariant item = QVariant::fromValue(list[index]);
                              setVariant(item, propName, value);
                              list[index] = item.value<TextStyle>();
                              editor()->setTextStylesLight(list);
                              return;
                              }
                        }
                  else if (listName == "mcpServersConfig") {
                        auto list = editor()->mcpServersConfig();
                        if (index >= 0 && index < list.size()) {
                              QVariant item = QVariant::fromValue(list[index]);
                              setVariant(item, propName, value);
                              list[index] = item.value<McpServerConfig>();
                              editor()->setMcpServersConfig(list);
                              return;
                              }
                        }
                  else if (listName == "languageServersConfig") {
                        auto list = editor()->languageServersConfig();
                        if (index >= 0 && index < list.size()) {
                              QVariant item = QVariant::fromValue(list[index]);
                              setVariant(item, propName, value);
                              list[index] = item.value<LanguageServerConfig>();
                              editor()->setLanguageServersConfig(list);
                              return;
                              }
                        }
                  else if (listName == "shortcuts") {
                        auto list = editor()->shortcuts();
                        if (index >= 0 && index < list.size()) {
                              QVariant item = QVariant::fromValue(list[index]);
                              setVariant(item, propName, value);
                              list[index] = item.value<ShortcutConfig>();
                              editor()->setShortcuts(list);
                              return;
                              }
                        }
                  else
                        Critical("unknown list <{}>", listName);
                  }
            }
      editor()->setProperty(name.toUtf8().constData(), value);
      }

//---------------------------------------------------------
//   openConfig  –  load page & inject data
//---------------------------------------------------------

void ConfigWebView::openConfig() {
      QFile file(":/res/config.json");
      if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Could not open config.json";
            return;
            }
      QByteArray data   = file.readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      QJsonObject obj   = doc.object();

      QString title       = obj["titel"].toString();
      QJsonArray sections = obj["sections"].toArray();

      bool dark           = editor()->darkMode();
      QString bgColor     = dark ? "#222222" : "#f5f5f5";
      QString fgColor     = dark ? "#eeeeee" : "#111111";
      QString borderColor = dark ? "#444" : "#ccc";
      QString activeBg    = dark ? "#555" : "#ddd";
      QString listBg      = dark ? "#333" : "#fff";

      QString html =
          QString("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                  "<link href=\"qrc:/res/css/bootstrap.min.css\" rel=\"stylesheet\">"
                  "<link rel=\"stylesheet\" "
                  "href=\"https://cdn.jsdelivr.net/npm/@simonwep/pickr/dist/themes/classic.min.css\"/>"
                  "<script src=\"https://cdn.jsdelivr.net/npm/@simonwep/pickr/dist/pickr.min.js\"></script>"
                  "<script src=\"qrc:///qtwebchannel/qwebchannel.js\"></script>"
                  "<style>"
                  "body { background-color: %1; color: %2; height: 100vh; display: flex; flex-direction: "
                  "column; overflow: hidden; margin: 0; font-family: sans-serif; }"
                  ".header { padding: 10px 20px; font-size: 24px; border-bottom: 1px solid %3; } "
                  ".main-content { display: flex; flex: 1; overflow: hidden; } "
                  ".sidebar { width: 250px; border-right: 1px solid %3; overflow-y: auto; } "
                  ".content { flex: 1; padding: 20px; overflow-y: auto; } "
                  ".footer { padding: 10px 20px; border-top: 1px solid %3; display: flex; justify-content: "
                  "flex-end; gap: 10px; } "
                  ".nav-link { color: %2; cursor: pointer; padding: 10px 15px; text-decoration: none; "
                  "display: block; } "
                  ".nav-link.active { font-weight: bold; background-color: %4; } "
                  ".form-control, .form-select { background-color: %1; color: %2; border: 1px solid %3; } "
                  ".list-group-item { background-color: %6; color: %2; border: 1px solid %3; } "
                  ".list-group-item.active { background-color: %4; border-color: %3; } "
                  "</style>"
                  "</head><body>"
                  "<div class=\"header\">%5</div>"
                  "<div class=\"main-content\">"
                  "<div class=\"sidebar\">"
                  "<ul class=\"nav flex-column\">")
              .arg(bgColor, fgColor, borderColor, activeBg, title, listBg);

      for (int i = 0; i < sections.size(); ++i) {
            QString name         = sections[i].toObject()["title"].toString();
            QString activeClass  = (i == 0) ? " active" : "";
            html                += QString("<li class=\"nav-item\"><a class=\"nav-link%1\" id=\"nav-%2\" "
                                           "onclick=\"showSection(%2)\">%3</a></li>")
                                       .arg(activeClass)
                                       .arg(i)
                                       .arg(name);
            }

      html += "</ul></div><div class=\"content\">";

      for (int i = 0; i < sections.size(); ++i) {
            QJsonObject secObj = sections[i].toObject();
            QString name       = secObj["title"].toString();
            QString type       = secObj["type"].toString();
            QString dNone      = (i == 0) ? "" : " d-none";

            html += QString("<div id=\"section-%1\" class=\"section-panel%2\"><h2>%3</h2>")
                        .arg(i)
                        .arg(dNone)
                        .arg(name);

            if (type == "properties") {
                  html           += "<form id=\"form-" + QString::number(i) + "\">";
                  QJsonArray seq  = secObj["sequence"].toArray();
                  for (QJsonValue val : seq) {
                        QString propKey     = val.toString();
                        QJsonObject propObj = secObj[propKey].toObject();
                        QString propType    = propObj["type"].toString();
                        QString propLabel   = propObj["label"].toString();
                        QString propName    = propObj["name"].toString();

                        QVariant currentValue = getProperty(propName);

                        html += "<div class=\"form-group mb-3\">";
                        html += "<label>" + propLabel + "</label>";

                        if (propType == "bool") {
                              QString checked = currentValue.toBool() ? "checked" : "";
                              html += QString("<input type=\"checkbox\" class=\"form-check-input ms-2\" "
                                              "name=\"%1\" %2>")
                                          .arg(propName)
                                          .arg(checked);
                              }
                        else if (propType == "font") {
                              html += QString("<select class=\"form-select\" name=\"%1\">").arg(propName);
                              QStringList families = QFontDatabase::families();
                              for (const QString& f : families) {
                                    QString selected = (f == currentValue.toString()) ? "selected" : "";
                                    html +=
                                        QString("<option value=\"%1\" %2>%1</option>").arg(f).arg(selected);
                                    }
                              html += "</select>";
                              }
                        html += "</div>";
                        }
                  html += "</form>";
                  }
            else if (type == "list") {
                  QString listName        = secObj["listName"].toString();
                  QString listItemTitle   = secObj["listItemTitle"].toString();
                  QString titleProp       = listItemTitle.split(".").last();
                  QJsonArray listSequence = secObj["listSequence"].toArray();

                  QVariant listVar  = getProperty(listName);
                  QVariantList list = listVar.toList();

                  html += "<div class=\"d-flex\" style=\"height: calc(100vh - 120px);\">";
                  html += "<div class=\"list-group w-25\" style=\"overflow-y: auto;\">";

                  QString detailsHtml = "<div class=\"w-75 p-3\" style=\"overflow-y: auto;\">";
                  int itemIdx         = 0;

                  for (const QVariant& item : list) {
                        const QMetaObject* meta = item.metaType().metaObject();
                        if (!meta)
                              continue;

                        QString titleValue = "Item " + QString::number(itemIdx);
                        for (int k = 0; k < meta->propertyCount(); ++k) {
                              if (QString(meta->property(k).name()) == titleProp) {
                                    titleValue = meta->property(k).readOnGadget(item.constData()).toString();
                                    break;
                                    }
                              }

                        QString activeClass = (itemIdx == 0) ? " active" : "";
                        html +=
                            QString(
                                "<a href=\"#\" class=\"list-group-item list-group-item-action%1 "
                                "nav-list-%2\" onclick=\"showListItem(%2, %3)\" id=\"list-nav-%2-%3\">%4</a>")
                                .arg(activeClass)
                                .arg(i)
                                .arg(itemIdx)
                                .arg(titleValue);

                        QString dNone = (itemIdx == 0) ? "" : " d-none";
                        detailsHtml += QString("<div id=\"list-item-%1-%2\" class=\"list-item-panel-%1%3\">")
                                           .arg(i)
                                           .arg(itemIdx)
                                           .arg(dNone);

                        for (QJsonValue val : listSequence) {
                              QString propKey     = val.toString();
                              QJsonObject propObj = secObj[propKey].toObject();
                              QString propType    = propObj["type"].toString();
                              QString propLabel   = propObj["label"].toString();
                              bool propReadOnly   = propObj["readOnly"].toBool();

                              QVariant currentValue;
                              for (int k = 0; k < meta->propertyCount(); ++k) {
                                    if (QString(meta->property(k).name()) == propKey) {
                                          currentValue = meta->property(k).readOnGadget(item.constData());
                                          break;
                                          }
                                    }

                              detailsHtml += "<div class=\"form-group mb-3\">";
                              detailsHtml += "<label>" + propLabel + "</label>";

                              QString readonlyAttr = propReadOnly ? "readonly" : "";
                              QString inputName =
                                  QString("%1[%2].%3").arg(listName).arg(itemIdx).arg(propKey);

                              if (propType == "string") {
                                    detailsHtml += QString("<input type=\"text\" class=\"form-control mt-1\" "
                                                           "name=\"%1\" value=\"%2\" %3>")
                                                       .arg(inputName)
                                                       .arg(currentValue.toString().toHtmlEscaped())
                                                       .arg(readonlyAttr);
                                    }
                              else if (propType == "bool") {
                                    QString checked = currentValue.toBool() ? "checked" : "";
                                    detailsHtml +=
                                        QString("<input type=\"checkbox\" class=\"form-check-input ms-2\" "
                                                "name=\"%1\" %2 %3>")
                                            .arg(inputName)
                                            .arg(checked)
                                            .arg(readonlyAttr);
                                    }
                              else if (propType == "color") {
                                    QString colorVal = currentValue.toString();
                                    if (colorVal.isEmpty() || colorVal == "invalid")
                                          colorVal = "rgba(0, 0, 0, 1)";
                                    detailsHtml += QString("<input type=\"hidden\" name=\"%1\" "
                                                           "id=\"hidden-%1\" value=\"%2\">")
                                                       .arg(inputName)
                                                       .arg(colorVal);
                                    detailsHtml +=
                                        QString("<div class=\"color-picker\" data-input=\"hidden-%1\" "
                                                "data-value=\"%2\"></div>")
                                            .arg(inputName)
                                            .arg(colorVal);
                                    }
                              else if (propType == "enum") {
                                    detailsHtml +=
                                        QString("<select class=\"form-select mt-1\" name=\"%1\" %2>")
                                            .arg(inputName)
                                            .arg(readonlyAttr);
                                    QJsonArray options = propObj["options"].toArray();
                                    for (QJsonValue opt : options) {
                                          QString optStr = opt.toString();
                                          QString selected =
                                              (optStr == currentValue.toString()) ? "selected" : "";
                                          detailsHtml += QString("<option value=\"%1\" %2>%1</option>")
                                                             .arg(optStr)
                                                             .arg(selected);
                                          }
                                    detailsHtml += "</select>";
                                    }
                              else if (propType == "textarea") {
                                    detailsHtml += QString("<textarea class=\"form-control mt-1\" "
                                                           "name=\"%1\" rows=\"5\" %2>%3</textarea>")
                                                       .arg(inputName)
                                                       .arg(readonlyAttr)
                                                       .arg(currentValue.toString().toHtmlEscaped());
                                    }
                              else if (propType == "int") {
                                    int minVal = propObj["min"].toVariant().toInt();
                                    int maxVal = propObj["max"].toVariant().toInt();
                                    detailsHtml +=
                                        QString("<input type=\"number\" class=\"form-control mt-1\" "
                                                "name=\"%1\" value=\"%2\" min=\"%3\" max=\"%4\" %5>")
                                            .arg(inputName)
                                            .arg(currentValue.toInt())
                                            .arg(minVal)
                                            .arg(maxVal)
                                            .arg(readonlyAttr);
                                    }
                              detailsHtml += "</div>";
                              }
                        detailsHtml += "</div>";
                        itemIdx++;
                        }

                  bool listReadOnly  = secObj["readOnly"].toBool();
                  html              += "</div>"; // end list-group
                  detailsHtml       += "</div>"; // end details pane
                  html              += detailsHtml;
                  if (!listReadOnly) {
                        html += QString("<div class=\"d-flex justify-content-end gap-2\" style=\"position: "
                                        "absolute; bottom: 80px; right: 20px;\">"
                                        "<button onclick=\"if(backend) backend.listAdd('%1')\" class=\"btn "
                                        "btn-success\" style=\"width: 100px;\">Add</button>"
                                        "<button onclick=\"removeCurrentListItem('%1', %2)\" class=\"btn "
                                        "btn-danger\" style=\"width: 100px;\">Remove</button>"
                                        "</div>")
                                    .arg(listName)
                                    .arg(i);
                        }
                  html += "</div>"; // end d-flex
                  }
            else {
                  html += "<p>Configuration for " + name + " goes here.</p>";
                  }
            html += "</div>";
            }

      html += "</div></div>"; // end content, main-content

      html +=
          "<div class=\"footer\">"
          "<button onclick=\"if(backend) backend.resetConfig()\" class=\"btn btn-warning\">Reset</button>"
          "<button onclick=\"if(backend) backend.cancelConfig()\" class=\"btn btn-secondary\">Cancel</button>"
          "<button onclick=\"saveConfig()\" class=\"btn btn-primary\">Save</button>"
          "</div>";

      html +=
          R"(<script>
      var backend = null;
      new QWebChannel(qt.webChannelTransport, function (channel) {
            backend = channel.objects.configApi;
      });

      function showSection(idx) {
            document.querySelectorAll('.section-panel').forEach(p => p.classList.add('d-none'));
            document.querySelectorAll('.nav-link').forEach(l => l.classList.remove('active'));
            document.getElementById('section-' + idx).classList.remove('d-none');
            document.getElementById('nav-' + idx).classList.add('active');
      }

      function showListItem(sectionIdx, itemIdx) {
            document.querySelectorAll('.list-item-panel-' + sectionIdx).forEach(p => p.classList.add('d-none'));
            document.querySelectorAll('.nav-list-' + sectionIdx).forEach(l => l.classList.remove('active'));
            document.getElementById('list-item-' + sectionIdx + '-' + itemIdx).classList.remove('d-none');
            document.getElementById('list-nav-' + sectionIdx + '-' + itemIdx).classList.add('active');
      }

      function saveConfig() {
            let data = {};
            document.querySelectorAll('input, select, textarea').forEach(el => {
                  if (el.name) {
                        if (el.type === 'checkbox') {
                              data[el.name] = el.checked;
                        }
                        else {
                              data[el.name] = el.value;
                        }
                  }
            });
            let jsonString = JSON.stringify(data);
            if(backend) backend.saveConfig(encodeURIComponent(jsonString));
      }

      function removeCurrentListItem(listName, sectionIdx) {
            let activeEl = document.querySelector('.nav-list-' + sectionIdx + '.active');
            if (activeEl) {
                  let idParts = activeEl.id.split('-');
                  let itemIdx = idParts[idParts.length - 1];
                  if(backend) backend.listRemove(listName, parseInt(itemIdx));
            }
      }

      // Initialize Pickr
      document.querySelectorAll('.color-picker').forEach(el => {
            const hiddenInput = document.getElementById(el.getAttribute('data-input'));
            const pickr = Pickr.create({
                  el: el,
                  theme: 'classic',
                  default: el.getAttribute('data-value'),
                  components: {
                        preview: true,
                        opacity: true,
                        hue: true,
                        interaction: { hex: true, rgba: true, input: true, save: true }
                  }
            });
            pickr.on('save', (color) => {
                  hiddenInput.value = color.toRGBA().toString();
                  pickr.hide();
            });
      });
      </script>
      </body></html>)";

      setHtml(html);
      }

#include "configwebview.moc"
