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
#include "editor.h"
#include "kontext.h"
#include "logger.h"
// Hilfs-Operatoren für QDataStream
QDataStream& operator<<(QDataStream& out, const ShortcutConfig& v) {
      // Cmd ist ein enum class, daher als int casten
      out << static_cast<int>(v.cmd) << v.id << v.description << v.sequence;
      return out;
      }

QDataStream& operator>>(QDataStream& in, ShortcutConfig& v) {
      int cmdInt;
      in >> cmdInt >> v.id >> v.description >> v.sequence;
      v.cmd = static_cast<Cmd>(cmdInt);
      return in;
      }

QDataStream& operator<<(QDataStream& out, const FileType& v) {
      out << v.extensions << v.languageId << v.languageServer << v.tabSize << v.parse << v.header << v.createTabs << v.pymacros;
      return out;
      }

QDataStream& operator>>(QDataStream& in, FileType& v) {
      in >> v.extensions >> v.languageId >> v.languageServer >> v.tabSize >> v.parse >> v.header >> v.createTabs;
      return in;
      }

QDataStream& operator<<(QDataStream& out, const LanguageServerConfig& v) {
      out << v.name << v.command << v.args;
      return out;
      }

QDataStream& operator>>(QDataStream& in, LanguageServerConfig& v) {
      in >> v.name >> v.command >> v.args;
      return in;
      }

//---------------------------------------------------------
//   loadDefaults
//---------------------------------------------------------

void Editor::loadDefaults() {
      _shortcuts = {
               {                 Cmd::CMD_QUIT,                       "CMD_QUIT",                 "Quit",        "Ctrl + K, Ctrl + Q; Shift+F1"},
               {            Cmd::CMD_SAVE_QUIT,                  "CMD_SAVE_QUIT",            "Save+Quit",                                  "F1"},
               {                 Cmd::CMD_SAVE,                       "CMD_SAVE",                 "Save",                 "Ctrl + K,  Ctrl + S"},
               {           Cmd::CMD_CHAR_RIGHT,                 "CMD_CHAR_RIGHT",                "Right",                     "Right; Ctrl + D"},
               {            Cmd::CMD_CHAR_LEFT,                  "CMD_CHAR_LEFT",                 "Left",                     "Left;  Ctrl + S"},
               {              Cmd::CMD_LINE_UP,                    "CMD_LINE_UP",                   "Up",                      "Up;   Ctrl + E"},
               {            Cmd::CMD_LINE_DOWN,                  "CMD_LINE_DOWN",                 "Down",                  "Down;     Ctrl + X"},
               {           Cmd::CMD_LINE_START,                 "CMD_LINE_START",           "Line Start", "Ctrl + Q,    Ctrl + S; Shift + Left"},
               {             Cmd::CMD_LINE_END,                   "CMD_LINE_END",             "Line End",  "Ctrl + Q,  Ctrl + D; Shift + Right"},
               {             Cmd::CMD_LINE_TOP,                   "CMD_LINE_TOP",             "Line Top",                 "Ctrl + Q,  Ctrl + E"},
               {          Cmd::CMD_LINE_BOTTOM,                "CMD_LINE_BOTTOM",          "Line Bottom",                 "Ctrl + Q,  Ctrl + X"},
               {              Cmd::CMD_PAGE_UP,                    "CMD_PAGE_UP",              "Page Up",                     "Ctrl + R;  PgUp"},
               {            Cmd::CMD_PAGE_DOWN,                  "CMD_PAGE_DOWN",            "Page Down",                   "Ctrl + C;  PgDown"},
               {           Cmd::CMD_FILE_BEGIN,                 "CMD_FILE_BEGIN",                "Start",    "Ctrl + Q,  Ctrl + R; Ctrl + PgUp"},
               {             Cmd::CMD_FILE_END,                   "CMD_FILE_END",                  "End",  "Ctrl + Q,  Ctrl + C; Ctrl + PgDown"},
               {            Cmd::CMD_WORD_LEFT,                  "CMD_WORD_LEFT",            "Word Left",               "Ctrl + A; Ctrl + Left"},
               {           Cmd::CMD_WORD_RIGHT,                 "CMD_WORD_RIGHT",           "Word Right",              "Ctrl + F; Ctrl + Right"},
               {                Cmd::CMD_ENTER,                      "CMD_ENTER",                "Enter",                              "Escape"},
               {         Cmd::CMD_KONTEXT_COPY,               "CMD_KONTEXT_COPY",         "Copy Kontext",             "Ctrl + K,  Ctrl + K; F4"},
               {         Cmd::CMD_KONTEXT_PREV,               "CMD_KONTEXT_PREV",         "Kontext Left",     "Ctrl + K,  Ctrl + J; Shift + F3"},
               {         Cmd::CMD_KONTEXT_NEXT,               "CMD_KONTEXT_NEXT",        "Kontext Right",             "Ctrl + K,  Ctrl + L; F3"},
               {        Cmd::CMD_SHOW_BRACKETS,              "CMD_SHOW_BRACKETS",        "Show Brackets",                 "Ctrl + K,  Ctrl + I"},
               {           Cmd::CMD_SHOW_LEVEL,                 "CMD_SHOW_LEVEL",           "Show Level",                 "Ctrl + K,  Ctrl + M"},
               {           Cmd::CMD_KONTEXT_UP,            "Cmd::CMD_KONTEXT_UP",           "Kontext Up",                           "Ctrl + Up"},
               {         Cmd::CMD_KONTEXT_DOWN,               "CMD_KONTEXT_DOWN",         "Kontext Down",                         "Ctrl + Down"},
               {          Cmd::CMD_DELETE_LINE,                "CMD_DELETE_LINE",          "Delete Line",                            "Ctrl + Y"},
               {    Cmd::CMD_DELETE_LINE_RIGHT,          "CMD_DELETE_LINE_RIGHT",    "Delete Line Right",                 "Ctrl + Q,  Ctrl + Y"},
               {                 Cmd::CMD_UNDO,                       "CMD_UNDO",                 "Undo",                            "Ctrl + Z"},
               {                 Cmd::CMD_REDO,                       "CMD_REDO",                 "Redo",                    "Shift + Ctrl + Z"},
               {               Cmd::CMD_RUBOUT,                "Cmd::CMD_RUBOUT",               "Rubout",                   "Delete; Backspace"},
               {          Cmd::CMD_CHAR_DELETE,                "CMD_CHAR_DELETE",               "Delete",                            "Ctrl + G"},
               {          Cmd::CMD_INSERT_LINE,                "CMD_INSERT_LINE",          "Insert Line",                            "Ctrl + N"},
               {                  Cmd::CMD_TAB,                        "CMD_TAB",            "Tabulator",                                 "Tab"},
               {           Cmd::CMD_SELECT_ROW,                 "CMD_SELECT_ROW",           "Row Select",                                  "F5"},
               {           Cmd::CMD_SELECT_COL,                 "CMD_SELECT_COL",           "Col Select",                                  "F6"},
               {          Cmd::CMD_SEARCH_NEXT,                "CMD_SEARCH_NEXT",          "Search Next",                                  "F7"},
               {               Cmd::CMD_RENAME,                "Cmd::CMD_RENAME",        "Global Rename",                  "Ctrl + O, Ctrl + R"},
               {                 Cmd::CMD_PICK,                  "Cmd::CMD_PICK",     "Copy Line (Pick)",                                  "F8"},
               {                  Cmd::CMD_PUT,                   "Cmd::CMD_PUT",     "Paste Line (Put)",                                  "F9"},
               {          Cmd::CMD_SEARCH_PREV,                "CMD_SEARCH_PREV",          "Search Prev",                          "SHIFT + F7"},
               {          Cmd::CMD_DELETE_WORD,                "CMD_DELETE_WORD",          "Delete Word",                            "Ctrl + T"},
               {           Cmd::CMD_ENTER_WORD,            "Cmd::CMD_ENTER_WORD",           "Enter Word",                 "Ctrl + O,  Ctrl + W"},
               {            Cmd::CMD_GOTO_BACK,             "Cmd::CMD_GOTO_BACK",         "History Back",                          "Ctrl + F12"},
               {            Cmd::CMD_SHOW_INFO,             "Cmd::CMD_SHOW_INFO",        "Show AI Panel",                            "Ctrl + H"},
               {               Cmd::CMD_FORMAT,                "Cmd::CMD_FORMAT",               "Format",                 "Ctrl + O,  Ctrl + F"},
               {       Cmd::CMD_VIEW_FUNCTIONS,        "Cmd::CMD_VIEW_FUNCTIONS",          "Toggle View",                            "Ctrl + V"},
               {            Cmd::CMD_VIEW_BUGS,             "Cmd::CMD_VIEW_BUGS",   "View LS Annotation",                            "Ctrl + B"},
               { Cmd::CMD_GOTO_TYPE_DEFINITION,  "Cmd::CMD_GOTO_TYPE_DEFINITION", "Goto Type Definition",                                 "F10"},
               {  Cmd::CMD_GOTO_IMPLEMENTATION,   "Cmd::CMD_GOTO_IMPLEMENTATION",  "Goto Implementation",                                 "F11"},
               {      Cmd::CMD_GOTO_DEFINITION,       "Cmd::CMD_GOTO_DEFINITION",      "Goto Definition",                                 "F12"},
               {        Cmd::CMD_EXPAND_MACROS,         "Cmd::CMD_EXPAND_MACROS",        "Expand Macros",                  "Ctrl + O, Ctrl + E"},
               {          Cmd::CMD_COMPLETIONS,           "Cmd::CMD_COMPLETIONS",     "Show Completions",             "Ctrl + Tab; Shift + Tab"},
               {             Cmd::CMD_FOLD_ALL,              "Cmd::CMD_FOLD_ALL",             "Fold All",                            "Ctrl + M"},
               {           Cmd::CMD_UNFOLD_ALL,            "Cmd::CMD_UNFOLD_ALL",           "Unfold All",                    "Ctrl + Shift + M"},
               {          Cmd::CMD_FOLD_TOGGLE,           "Cmd::CMD_FOLD_TOGGLE",          "Fold Toggle",                            "Ctrl + <"},
               {      Cmd::CMD_FUNCTION_HEADER,       "Cmd::CMD_FUNCTION_HEADER",        "Create Header",                  "Ctrl + O, Ctrl + H"},
               {           Cmd::CMD_GIT_TOGGLE,            "Cmd::CMD_GIT_TOGGLE",       "Show Git Panel",                  "Ctrl + O, Ctrl + G"},
               {       Cmd::CMD_ENTER_ADD_FILE,        "Cmd::CMD_ENTER_ADD_FILE",             "Add File",                                  "F3"},
               {         Cmd::CMD_ENTER_SEARCH,          "Cmd::CMD_ENTER_SEARCH",       "Search/Replace",                                  "F7"},
               {Cmd::CMD_ENTER_CREATE_FUNCTION, "Cmd::CMD_ENTER_CREATE_FUNCTION",      "Create Function",                              "Ctrl+F"},
               {      Cmd::CMD_ENTER_GOTO_LINE,       "Cmd::CMD_ENTER_GOTO_LINE",            "Goto Line",                              "Ctrl+G"},
               {           Cmd::CMD_SCREENSHOT,            "Cmd::CMD_SCREENSHOT",          "Screen Shot",        "Ctrl + O, Ctrl + O, Ctrl + P"},
               {            Cmd::CMD_LINK_BACK,             "Cmd::CMD_LINK_BACK",            "Link Back",                         "Ctrl + PgUp"},
               {         Cmd::CMD_LINK_FORWARD,          "Cmd::CMD_LINK_FORWARD",         "Link Forward",                       "Ctrl + PgDown"},
            };

      _fileTypes = {
         FileType(".*\\.cpp$", "cpp", "clangd", 6, true, false, false, false),
         FileType(".*\\.c$", "c", "clangd", 6, true, false, false, false),
         FileType(".*\\.html$", "html", "vscode-html", 4, false, false, false, false),
         FileType(".*\\.h$", "cpp", "clangd", 6, true, true, false, true),
         FileType(".*\\.py$", "python", "pylsp", 6, false, false, false, false),
         FileType(".*\\.qml$", "qml", "qmlls", 4, false, false, false, false),
         FileType(".*\\.md$", "markdown", "none", 6, false, false, false, false),
         FileType("Makefile$", "makefile", "none", 6, false, false, true, false),
         FileType(".*\\.jpg$; .*\\.jpeg$; .*\\.png$; .*\\.gif$;.*\\.svg$;.*\\.webp$", "image", "none", 6, false, false, false, false)};

      _languageServersConfig = {
               {     "clangd",                              "clangd", "--query-driver --log=error --completion-style=detailed --compile-commands-dir=build --background-index"},
               {"vscode-html",          "vscode-html-languageserver",                                                                                                "--stdio"},
               {      "pylsp",                               "pylsp",                                                                                                       ""},
               {      "qmlls", "/home/ws/Qt/6.11.0/gcc_64/bin/qmlls",                                                                                                       ""}
            };
      emit shortcutsChanged();
      emit fileTypesChanged();
      emit languageServersConfigChanged();
      }

//---------------------------------------------------------
//   setShortcuts
//---------------------------------------------------------

void Editor::setShortcuts(const QList<ShortcutConfig>& s) {
      if (_shortcuts == s)
            return;
      _shortcuts = s;
      emit shortcutsChanged();
      }

//---------------------------------------------------------
//   setFileTypes
//---------------------------------------------------------

void Editor::setFileTypes(const QList<FileType>& f) {
      if (_fileTypes == f)
            return;
      _fileTypes = f;
      emit fileTypesChanged();
      }

//---------------------------------------------------------
//   setLanguageServers
//---------------------------------------------------------

void Editor::setLanguageServersConfig(const QList<LanguageServerConfig>& l) {
      if (_languageServersConfig == l)
            return;
      _languageServersConfig = l;
      emit languageServersConfigChanged();
      }

//---------------------------------------------------------
//   resetToDefaults
//---------------------------------------------------------

void Editor::resetToDefaults() {
      loadDefaults();
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
      QSettings settings("nped", "config");

      // Speichern komplexer Listen als JSON String oder QVariantList
      // Hier vereinfacht via QVariantList convertierung
      QVariantList scList;
      for (const auto& s : _shortcuts)
            scList << QVariant::fromValue(s);
      settings.setValue("shortcuts", scList);

      QVariantList ftList;
      for (const auto& f : _fileTypes)
            ftList << QVariant::fromValue(f);
      settings.setValue("fileTypes", ftList);

      QVariantList lsList;
      for (const auto& l : _languageServersConfig)
            lsList << QVariant::fromValue(l);
      settings.setValue("languageServers", lsList);
      settings.setValue("darkMode", darkMode());
      settings.setValue("fgColor", fgColor());
      settings.setValue("bgColor", bgColor());
      }

//---------------------------------------------------------
//   loadSettings
//---------------------------------------------------------

void Editor::loadSettings() {
      QSettings settings("nped", "config");
      if (!settings.contains("shortcuts"))
            return; // Kein Config vorhanden

      auto loadList = [&]<typename T>(const QString& key, QList<T>& target) {
            if (!settings.contains(key))
                  return;
            QVariantList list = settings.value(key).toList();
            target.clear();
            for (const auto& v : list)
                  if (v.canConvert<T>())
                        target.append(v.value<T>());
            };

      loadList("shortcuts", _shortcuts);
      loadList("fileTypes", _fileTypes);
      loadList("languageServers", _languageServersConfig);
      setDarkMode(settings.value("darkMode", darkMode()).toBool());
      setFgColor(settings.value("fgColor", fgColor()).value<QColor>());
      setBgColor(settings.value("bgColor", bgColor()).value<QColor>());
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
      _quickWidget->rootContext()->setContextProperty("config", editor);
      _quickWidget->rootContext()->setContextProperty("agent", editor->getAgent());

      // Lade die QML Datei aus dem Modul "Nped.Config"
      // Hinweis: Durch die CMake Policy QTP0001 ist der Pfad standardisiert:
      _quickWidget->setSource(QUrl("qrc:/qt/qml/Nped/Config/qml/ConfigDialog.qml"));
      }

void ConfigDialogWrapper::accept() {
      // Hier könnten Daten gespeichert werden
      close();
      }

void ConfigDialogWrapper::reject() {
      // Hier könnten Änderungen verworfen werden
      close();
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
