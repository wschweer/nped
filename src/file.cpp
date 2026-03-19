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

#include "logger.h"
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QRegularExpression>
#include <QTextStream>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <QMessageBox>

#include <vector>
#include <algorithm> // Für std::find_if
#include <optional>  // Für std::optional

#include "editor.h"
#include "agent.h"
#include "editwin.h"
#include "file.h"
#include "kontext.h"
#include "lsclient.h"
#include "undo.h"

extern Codec readCodec;
extern Codec writeCodec;

#define AST false // debug AST parsing for indentation level

using PropertyItem  = std::map<std::string, QString>;
using PropertyItems = std::vector<PropertyItem>;

//---------------------------------------------------------
//   evalPP
//---------------------------------------------------------

QString Editor::evalPP(const QString& pdata) {
      PropertyItems propertyItems;
      QStringList sl = pdata.split("\n");
      //      std::string s;
      for (const auto& l : sl) {
            QStringList tokenList = l.simplified().split(' ');
            PropertyItem item;
            if (tokenList[0] == "ON") {
                  QString js = l.simplified().mid(2);
                  json jprop;
                  try {
                        jprop = json::parse(js.toStdString());
                        }
                  catch (json::parse_error& e) {
                        Critical("json::parse failed: {}", e.what());
                        msg("json::parse failed: {}", e.what());
                        return "";
                        }
                  catch (...) {
                        msg("json::parse failed");
                        return "";
                        }
                  item["pr"] = "true"; // create Property()
                  for (const auto& [key, value] : jprop.items()) {
                        Debug("parse json <{}> <{}>", key, value.dump());
                        if (key == "name") {
                              QString s    = QString::fromStdString(value.get<std::string>());
                              item["name"] = s;
                              s[0]         = s[0].toUpper();
                              item["Name"] = s;
                              }
                        else {
                              QString s;
                              if (value.is_string())
                                    s = QString::fromStdString(value.get<std::string>());
                              else
                                    s = QString::fromStdString(value.dump());
                              // s = s.replace("\"", "\\\"");
                              item[key] = s;
                              }
                        }
                  }
            else {
                  for (int i = 0; i < tokenList.size(); ++i) {
                        //##P2  double raster     1.0 Raster \"\" .1 100.0 1 \"mm\"
                        switch (i) {
                              case 0:
                                    item["op"] = tokenList[i];
                                    if (item["op"] == "P0")
                                          item["persistent"] = "1";
                                    else if (item["op"] == "P1")
                                          ;
                                    else if (item["op"] == "P1RO")
                                          item["ro"] = "1";
                                    else if (item["op"] == "P2")
                                          item["pr"] = "true"; // create Property()
                                    else if (item["op"] == "P2RO") {
                                          item["ro"] = "1";
                                          item["pr"] = "true"; // create Property()
                                          }
                                    break;
                              case 1: item["type"] = tokenList[i]; break;
                              case 2:
                                    item["name"]    = tokenList[i];
                                    tokenList[i][0] = tokenList[i][0].toUpper();
                                    item["Name"]    = tokenList[i];
                                    break;
                              case 3: item["default"] = tokenList[i]; break;
                              case 4: item["label"] = tokenList[i]; break;
                              case 5: item["script"] = tokenList[i]; break;
                              case 6: item["min"] = tokenList[i]; break;
                              case 7: item["max"] = tokenList[i]; break;
                              case 8: item["precision"] = tokenList[i]; break;
                              case 9: item["units"] = tokenList[i]; break;
                              }
                        }
                  }
            propertyItems.push_back(item);
            }

      std::string result;
      result += "//##<\n";

      //***************************************************
      //    create properties
      //***************************************************

      for (const auto& item : propertyItems) {
            result += std::format("      Q_PROPERTY({0} {1} READ {1} ", item.at("type"), item.at("name"));
            if (item.contains("ro"))
                  result += std::format("NOTIFY {}Changed)\n", item.at("name"));
            else
                  result += std::format("WRITE set{} NOTIFY {}Changed)\n", item.at("Name"), item.at("name"));
            }

      //***************************************************
      //    create variables
      //***************************************************

      result += "\n   protected:\n";
      for (auto& item : propertyItems) {
            auto name = item["name"];
            if (item.contains("pr") && item["pr"] == "true") {
                  std::string jsonArgs;
                  if (item.contains("min")) {
                        if (!jsonArgs.empty())
                              jsonArgs += ", ";
                        jsonArgs += format("\\\"min\\\":{}", item["min"]);
                        }
                  if (item.contains("max")) {
                        if (!jsonArgs.empty())
                              jsonArgs += ", ";
                        jsonArgs += format("\\\"max\\\":{}", item["max"]);
                        }
                  if (item.contains("precision")) {
                        if (!jsonArgs.empty())
                              jsonArgs += ", ";
                        jsonArgs += format("\\\"precision\\\":{}", item["precision"]);
                        }
                  if (item.contains("units")) {
                        if (!jsonArgs.empty())
                              jsonArgs += ", ";
                        jsonArgs += format("\\\"units\\\":{}", item["units"]);
                        }
                  if (item.contains("label")) {
                        if (!jsonArgs.empty())
                              jsonArgs += ", ";
                        jsonArgs += format("\\\"label\\\":{}", item["label"]);
                        }
                  if (item.contains("script")) {
                        if (!jsonArgs.empty())
                              jsonArgs += ", ";
                        jsonArgs += format("\\\"script\\\":{}", item["script"]);
                        }
                  if (item.contains("nosave")) {
                        if (!jsonArgs.empty())
                              jsonArgs += ", ";
                        jsonArgs += format("\\\"nosave\\\":{}", item["nosave"]);
                        }
                  if (item.contains("noshow")) {
                        if (!jsonArgs.empty())
                              jsonArgs += ", ";
                        jsonArgs += format("\\\"noshow\\\":{}", item["noshow"]);
                        }
                  if (item.contains("scriptable")) {
                        if (!jsonArgs.empty())
                              jsonArgs += ", ";
                        jsonArgs += format("\\\"scriptable\\\":{}", item["scriptable"]);
                        }
                  //                  jsonArgs = QString::fromStdString(jsonArgs).replace("\"", "\\\"").toStdString();
                  result += format("      Property* {0}Property = PropertyMap::push_back(\"{0}\", ", name);
                  result += format("new Property(this, \"{}\", [this] {{ {}Changed(); }}, {}", name, name, item["default"]);
                  if (jsonArgs.empty())
                        result += "));\n";
                  else
                        result += format(", \"{{ {} }}\"));\n", jsonArgs);
                  }
            else {
                  result += format("      {} _{}", item.at("type"), name);
                  if (item.contains("default"))
                        result += format("={}", item["default"]);
                  result += ";\n";
                  }
            }

      //***************************************************
      //    create signals
      //***************************************************

      result += "\n   signals:\n";
      for (auto& item : propertyItems)
            result += std::format("      void {}Changed();\n", item["name"]);

      // create persistent storage of properties
      bool hasPersistentProperties = false;
      for (auto& item : propertyItems) {
            if (item.contains("persistent")) {
                  hasPersistentProperties = true;
                  break;
                  }
            }

      if (hasPersistentProperties) {
            result += "\n   protected:\n";
            result += "      void saveProperties() {\n";
            result += "            QSettings settings;\n";
            for (auto& item : propertyItems)
                  if (item.contains("persistent"))
                        result += std::format("            settings.setValue(\"Properties/{0}\", {0}());\n", item["name"]);
            result += "            };\n";
            result += "      void restoreProperties() {\n";
            result += "            QSettings settings;\n";
            for (auto& item : propertyItems) {
                  if (item.contains("persistent")) {
                        result          += std::format("            set{}(settings.value(\"Properties/{}\").", item["Name"], item["name"]);
                        const auto type  = item.at("type");
                        if (type == "QString")
                              result += "toString());\n";
                        else if (type == "int")
                              result += "toInt());\n";
                        else if (type == "double")
                              result += "toDouble());\n";
                        else if (type == "bool")
                              result += "toBool());\n";
                        else if (type == "QColor")
                              result += "value<QColor>());\n";
                        }
                  }
            result += "            };\n";
            }

      //***************************************************
      //    create setters
      //***************************************************

      result += "\n   public:\n";
      for (auto& item : propertyItems) {
            if (item.contains("ro"))
                  continue;
            if (item.contains("pr") && item["pr"] == "true") {
                  result += std::format("      void set{}({} v) {{ {}Property->setValue(QVariant::fromValue<{}>(v)); }}\n", item["Name"],
                                        item.at("type"), item["name"], item.at("type"));
                  }
            else {
                  result += std::format("      void set{0}({1} v) {{ if (v != {2}()) {{ _{2} = v; emit {2}Changed(); }} }}\n", item["Name"],
                                        item.at("type"), item["name"]);
                  }
            }

      //***************************************************
      //    create getters
      //***************************************************

      for (auto& item : propertyItems) {
            if (item.contains("noGetter"))
                  continue;
            if (item.contains("pr") && item["pr"] == "true") {
                  // QString proto() const { return get<QString>(protoProperty->value()); }
                  //                  result += std::format("      {} {}() const {{ return get<{}>({}Property->value()); }}\n", item.at("type"), item["name"],
                  result += std::format("      {} {}() const {{ return {}Property->value().value<{}>(); }}\n", item.at("type"),
                                        item["name"], item["name"], item.at("type"));
                  }
            else
                  result += std::format("      {} {}() const {{ return _{};}}\n", item.at("type"), item["name"], item["name"]);
            }

      result += "\n   public:\n";
      result += "//##>\n";

      return QString::fromStdString(result);
      ;
      }

//---------------------------------------------------------
//   Lines
//---------------------------------------------------------

Lines::Lines(const QStringList& sl) {
      for (const auto& s : sl)
            push_back(s);
      }

Lines::Lines(const QVector<Line> l) : QVector<Line>(l) {
      }

//---------------------------------------------------------
//   join
//---------------------------------------------------------

QString Lines::join(QChar c) const {
      QString s;
      for (const auto& ss : *this) {
            s += ss.qstring();
            s += c;
            }
      return s;
      }

//---------------------------------------------------------
//   toStringList
//---------------------------------------------------------

QStringList Lines::toStringList() {
      QStringList l;
      for (const auto& ss : *this)
            l << ss.qstring();
      return l;
      }

//---------------------------------------------------------
//   onFileChangedOnDisk
//---------------------------------------------------------

void File::onFileChangedOnDisk(const QString& path) {
      Debug("===========");
      // TODO: implement
      }

//---------------------------------------------------------
//   File
//---------------------------------------------------------

File::File(Editor* e, const QFileInfo& fi) : _fi(fi), editor(e) {
      f.setFileName(_fi.absoluteFilePath());
      _readOnly = !_fi.isWritable();
      _viewMode = ViewMode::File;
      _undo     = new UndoStack(this);
      connect(_undo, &UndoStack::dirtyChanged, [this] { emit modifiedChanged(); });

      // lookup fileType:

      QString filename = _fi.fileName();
      for (const auto& ft : fileTypes) {
            for (const auto& pattern : ft.extensions) {
                  QRegularExpression wildcard(pattern);
                  auto match = wildcard.match(filename);
                  if (match.hasMatch()) {
                        fileType = &ft;
                        break;
                        }
                  }
            if (fileType != &defaultFileType)
                  break;
            }
      QString ls = fileType->languageServer;
      setLSclient(e->getLSclient(ls));
      }

File::~File() {
      if (client)
            client->didCloseNotification(this);
      if (!created && editor && editor->getFileWatcher())
            editor->getFileWatcher()->removePath(_fi.absoluteFilePath());
      f.close();
      }

//---------------------------------------------------------
//   setViewMode
//---------------------------------------------------------

void File::setViewMode(ViewMode m, const Pos&) {
      _viewMode = m;
      switch (_viewMode) {
            case ViewMode::Functions: updateKollaps(); break;
            case ViewMode::File:
            case ViewMode::SearchResults:
            case ViewMode::GitVersion:
            case ViewMode::Bugs: break;
            case ViewMode::WebView: break;
            }
      }

//---------------------------------------------------------
//   readOnly
//---------------------------------------------------------

bool File::readOnly() const {
//      if (editor && editor->getAgent() && editor->getAgent()->isWorking())
//            return true;
      return _readOnly || (_viewMode != ViewMode::File);
      }

//---------------------------------------------------------
//   modified
//---------------------------------------------------------

bool File::modified() const {
      return _undo->dirty();
      }

//---------------------------------------------------------
//   load
//---------------------------------------------------------

bool File::load() {
      if (_readOnly) {
            if (!f.open(QIODevice::ReadOnly)) {
                  // file is new
                  _readOnly = false;
                  _fileText.push_back(Line());
                  lcOpen();
                  return false;
                  }
            }
      else {
            if (!f.open(QIODevice::ReadWrite)) {
                  _fileText.push_back(Line());
                  return false;
                  }
            // lock file
            struct flock fl;
            fl.l_type   = F_WRLCK;
            fl.l_whence = SEEK_SET;
            fl.l_start  = 0;
            fl.l_pid    = getpid();
            int fd      = f.handle();
            if (fcntl(fd, F_SETLK, &fl) == -1) {
                  if (errno == EACCES || errno == EAGAIN) {
                        // Debug("File is locked");
                        editor->msg("File is locked");
                        _readOnly = true;
                        }
                  else {
                        Debug("F_SETLK: Ioctl error fd {}: {}", fd, strerror(errno));
                        return false;
                        }
                  }
            }

      QTextStream os(&f);
      QString s = os.readAll();
      _fileText = Lines(s);
      for (auto& l : _fileText) {
            if (l.contains('\t')) {
                  int col = 0;
                  QString r;
                  for (const auto& c : l) {
                        if (c == '\t') {
                              r += ' ';
                              ++col;
                              while (col % tab()) {
                                    r += ' ';
                                    ++col;
                                    }
                              }
                        else {
                              r += c;
                              ++col;
                              }
                        }
                  l = r;
                  }
            }
      if (_fileText.empty())
            _fileText.push_back(Line());

      markExpansion();
      makePretty();
      created = false;
      lcOpen(); // notify language server
      mode = f.permissions();
      if (editor && editor->getFileWatcher())
          editor->getFileWatcher()->addPath(_fi.absoluteFilePath());
      return true;
      }

//---------------------------------------------------------
//   lcOpen
//---------------------------------------------------------

void File::lcOpen() {
      if (!client)
            return;
      if (client->initialized()) {
            client->didOpenNotification(this);
            updateAST();
            }
      else {
            connect(client, &LSclient::initializedChanged, [this] {
                  if (client->initialized()) {
                        client->didOpenNotification(this);
                        updateAST();
                        }
                  });
            }
      }

//---------------------------------------------------------
//   updateAST
//---------------------------------------------------------

void File::updateAST() {
      if (!client || !client->initialized())
            return;
      if (client->astProvider())
            client->astRequest(this);
      }

//---------------------------------------------------------
//   save
//    return true on success
//---------------------------------------------------------

bool File::save() {
      if (!modified())
            return true;
      expandMacros();

      // remove trailing spaces
      int lines = _fileText.size();
      for (int i = 0; i < lines; ++i) {
            auto& s = _fileText[i];
            //
            // remove trailing space
            //
            int n = s.size();
            while (n && s[n - 1].isSpace())
                  n--;
            s = s.left(n);
            }

      // remove trailing empty lines
      int n = 0;
      for (int i = _fileText.size() - 1; i > 0; --i) {
            if (!_fileText[i].empty())
                  break;
            ++n;
            }
      n -= 1; // keep one empty line
      if (n > 0) {
            Debug("erase {} empty lines at file end, file lines {}", n, _fileText.size());
            _fileText.erase(_fileText.end() - n, _fileText.end());
            }

      if (_fileText.isEmpty() && created) { // do not create an empty file
            Debug("not saved");
            return true;
            }

      QDir dir(_fi.path());
      QFile tempFile;
      for (int i = 0; i < 1000; ++i) {
            QString tmpName = QString("PED%1").arg(i);
            if (!dir.exists(tmpName)) {
                  tempFile.setFileName(dir.absolutePath() + "/" + tmpName);
                  if (!tempFile.open(QIODevice::ReadWrite)) {
                        QString s = QString("Open Temp File\n") + tempFile.fileName() + QString("\nfailed: ") + QString(strerror(errno));
                        Critical("Open Temp File: {} failed", s);
                        return false;
                        }
                  break;
                  }
            if (i == 999) {
                  Critical("cannot create tmp file");
                  return false;
                  }
            }
      QTextStream os(&tempFile);
      if (writeCodec == Codec::UTF8)
            os.setEncoding(QStringConverter::Utf8);
      else if (writeCodec == Codec::ISO_LATIN)
            os.setEncoding(QStringConverter::Latin1);

      lines = _fileText.size();
      for (int i = 0; i < lines; ++i) {
            auto& string = _fileText[i];

            if (fileType->createTabs) {
                  //                  int col = 0;
                  int t = 0;
                  QString nl;
                  for (const QChar& c : string.qstring()) {
                        if (c == ' ') {
                              ++t;
                              //                              ++col;
                              if ((t % tab()) == 0) {
                                    if (t > 1)
                                          os << '\t';
                                    else
                                          os << ' ';
                                    t = 0;
                                    }
                              }
                        else {
                              while (t) {
                                    os << ' ';
                                    t--;
                                    }
                              os << c;
                              //                              ++col;
                              t = 0;
                              }
                        }
                  }
            else {
                  if (!string.empty())
                        os << string.qstring();
                  }
            if (i < lines - 1)
                  os << '\n';
            }
      if (tempFile.error()) {
            QString s = QString("Write Temp File\n") + tempFile.fileName() + QString("failed: ") + tempFile.errorString();
            Critical("Ped: Write Temp: {}", s);
            return false;
            }
      //
      // remove old backup file if exists
      //
      QString backupName = _fi.absolutePath() + "/" + QString(".") + _fi.fileName() + QString(",");
      dir.remove(backupName);

      //
      // rename old file into backup
      //
      if (!dir.rename(path(), backupName))
            Critical("**rename failed: {} -> {}", path(), backupName);

      //
      // rename temp name into file name
      //
      if (!tempFile.rename(path()))
            Critical("rename failed: {}", tempFile.errorString());

      _undo->setClean();
      if (!QFile::setPermissions(path(), mode))
            Critical("set permissions {} on <{}> failed\n", int(mode), path());
      if (client)
            client->documentSaved(this);
      return true;
      }

//---------------------------------------------------------
//   setKollaps
//---------------------------------------------------------

void File::setKollaps(const Lines& map) {
      _kollaps = map;
      if (_viewMode == ViewMode::Functions)
            editor->editWidget()->update();
      }

//---------------------------------------------------------
//   kollaps
//---------------------------------------------------------

void File::updateKollaps() {
      Lines map;
      dumpLine(map, 0, 0, astTopNode);
      setKollaps(map);
      }

//---------------------------------------------------------
//   dumpLine
//---------------------------------------------------------

void File::dumpLine(Lines& lines, int level, int clevel, const ASTNode& node) const {
      bool selected;
      if (fileType->header)
            selected = node.p1.row != node.p2.row && node.role == "declaration";
      else
            selected =
                (node.role == "declaration" && (node.kind == "Function" || node.kind == "CXXMethod" || node.kind == "CXXConstructor"));
      if (selected) {
            int y = node.p1.row;
            if (y < fileText().size()) {
                  const QString& s = fileText().at(node.p1.row).qstring();
                  if (lines.empty() || lines.back().qstring() != s)
                        lines.push_back(s, node.p1);
                  }
            }
      for (const auto& c : node.children)
            dumpLine(lines, ++level, clevel, c);
      }

//---------------------------------------------------------
//   setAST
//---------------------------------------------------------

void File::setAST(const ASTNode& node) {
      astTopNode = node;
      Lines map;
      dumpLine(map, 0, 0, astTopNode);
      setKollaps(map);
      }

//---------------------------------------------------------
//   setBugs
//---------------------------------------------------------

void File::setBugs(const Lines& map) {
      _bugs = map;
      if (_viewMode == ViewMode::Bugs)
            editor->editWidget()->update();
      }

//---------------------------------------------------------
//   setSearchResults
//---------------------------------------------------------

void File::setSearchResults(const Lines& map) {
      _searchResults = map;
      if (_viewMode == ViewMode::SearchResults)
            editor->editWidget()->update();
      }

//---------------------------------------------------------
//   setLabel
//---------------------------------------------------------

void File::setLabel(int y, QChar c, QColor color) {
      if (y < 0 || y >= _fileText.size())
            y = _fileText.size() - 1;
      _fileText[y].setLabel(c, color);
      editor->editWidget()->update();
      }

//---------------------------------------------------------
//   clearLabel
//---------------------------------------------------------

void File::clearLabel() {
      for (auto& l : _fileText)
            if (l.fold() == FoldMark::No)
                  l.setLabel(QChar(' '));
      editor->editWidget()->update();
      }

//---------------------------------------------------------
//   inside
//---------------------------------------------------------

static bool operator>=(const Pos& p1, const Pos& p2) {
      return (p1.row > p2.row) || ((p1.row == p2.row) && (p1.col >= p2.col));
      }

static bool operator<(const Pos& p1, const Pos& p2) {
      return (p1.row < p2.row) || ((p1.row == p2.row) && (p1.col < p2.col));
      }

static bool inside(const Pos& p, const ASTNode& node) {
      if (node.p1.row == 0 && node.p2.row == 0)
            return true;
      return (p >= node.p1) && (p < node.p2);
      }

//---------------------------------------------------------
//   levelcount
//---------------------------------------------------------

bool File::levelcount(const Pos& p, int* level, const ASTNode& node) const {
      if (!inside(p, node))
            return false;

      int idx = 0;
      for (const auto& c : node.children) {
            //Debug("==========={} {}", *level, c.kind);
            if (inside(p, c)) {
                  //Debug("    inside kind <{}> role <{}>", c.kind, c.role);

                  // 1. Prüfen, ob wir das Level erhöhen müssen
                  //                  bool isElseIf = (c.kind == "If" && node.kind == "If" && c.role == "else");
                  bool isElseIf = (c.kind == "If" && node.kind == "If" && idx == 2);

                  // Wir erhöhen das Level bei Kontrollstrukturen,
                  // außer es handelt sich um ein 'else if'.
                  if ((c.kind == "If" && !isElseIf) || c.kind == "For" || c.kind == "Function" || c.kind == "CXXRecord" ||
                      c.kind == "CXXMethod" || c.kind == "While" || c.kind == "Switch" || c.kind == "Do" || c.kind == "Case" ||
                      c.kind == "Default") {
                        //Debug("++level");
                        (*level)++;
                        }

                  // 2. Rekursion: Tiefer in den Baum gehen
                  levelcount(p, level, c);
                  return true;
                  }
            ++idx;
            }
      return true;
      }

//---------------------------------------------------------
//   clevel
//---------------------------------------------------------

int File::indent(const Pos& cursor) const {
      int level = 0;
      if (parse()) {
            levelcount(cursor, &level, astTopNode);
            return level * tab();
            }
      int row = cursor.row;
      if (row == 0 || row >= rows())
            return 0;
      //      const auto& text = fileText(row);
      int column = 0;
      for (const auto& c : line(row)) {
            if (c != ' ')
                  break;
            ++column;
            }
      return column;
      }

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

QString Lines::removeText(const Pos& pos, int nn) {
      int n = nn;
      QString rs;
      int x = pos.col;
      int y = pos.row;

      if (y >= size())
            return QString();
      Line* s = &(*this)[y];
      while (n--) {
            if (x >= s->size()) {
                  rs += '\n';
                  if (y + 1 < size()) {
                        *s += (*this)[y + 1];
                        remove(y + 1);
                        }
                  }
            else {
                  rs += (*s)[x];
                  s->removeAt(x);
                  }
            }
      if (nn != rs.size())
            Critical("bad match {} != {}", nn, rs.size());
      return rs;
      }

//---------------------------------------------------------
//    insertText
//---------------------------------------------------------

void Lines::insertText(const Pos& pos, const QString& text) {
      if (text.isEmpty())
            return;
      int x = pos.col;
      int y = pos.row;
      while (y >= size())
            push_back(Line());
      for (const auto& c : text) {
            if (c == '\n') {
                  // split line in left and right part, remove right part
                  // and prepend it to a new line

                  QString nextLine = (*this)[y].mid(x);  // copy right part
                  (*this)[y]       = (*this)[y].left(x); // remove right part

                  ++y;
                  x = 0;

                  insert(y, Line(nextLine));
                  }
            else {
                  (*this)[y].insert(x++, c);
                  }
            }
      }

//---------------------------------------------------------
//   posValid
//---------------------------------------------------------

bool File::posValid(const Pos& pos) const {
      if (pos.col < 0 || pos.row < 0)
            return false;
      if (pos.col > columns(pos.row))
            return false;
      if (pos.row >= rows())
            return false;
      return true;
      }

//--------------------------------------------------------------
//   patch
//    This is the single entry point to actually change text.
//    Patches are applied to the unfold text version as
//    the language server sees it.
//    Here we also synchronize the language server.
//--------------------------------------------------------------

void File::patch(Patches& items) {
      if (readOnly()) {
            Critical("read only");
            return;
            }
      bool nothingToRemove = true;
      for (auto& pi : items) {
            if (pi.toRemove < 0) {
                  Critical("bad patch toRemove == {}", pi.toRemove);
                  return;
                  }
            if (!pi.empty()) {
                  nothingToRemove = false;
                  if (!posValid(pi.startPos)) {
                        Critical("invalid position col {} line {}", pi.startPos.col, pi.startPos.row);
                        return;
                        }
                  }
            }
      if (nothingToRemove) {
            Debug("empty patch");
            return;
            }

      ++_version;
      if (client)
            client->didChangeNotification(this, items);
      for (auto& pi : items) {
            QString removedText;
            //            Debug("patch {} {} <{}>", pi.position.y(), pi.toRemove,
            //            pi.insertText);
            if (pi.toRemove)
                  removedText = _fileText.removeText(pi.startPos, pi.toRemove);
            if (!pi.insertText.isEmpty())
                  _fileText.insertText(pi.startPos, pi.insertText);
            // flip values:
            pi.toRemove   = pi.insertText.size();
            pi.insertText = removedText;
            }
      emit fileChanged();
      makePretty();
      editor->update();
      }

//---------------------------------------------------------
//   toOffset
//    calculate the text byte offset from line/column
//---------------------------------------------------------

int File::toOffset(const Pos& p) {
      return distance(Pos(), p);
      }

//-----------------------------------------------------------------------------
//   distance
//    p2 - p1 denotes a Range
//    A range in a text document expressed as (zero-based) start and end
//    positions.
//    A range is comparable to a selection. Therefore, the end position
//    is exclusive. If you want to specify a range that contains a line
//    including the line ending character(s) then use an end position
//    denoting the start of the next line.
//    Returns the distance (p2-p1) as number of QChars().
//    Notice: unicode surrogates are not handled
//-----------------------------------------------------------------------------

int File::distance(const Pos& p1, const Pos& p2) const {
      Pos c(p1);

      // sanity checks:
      int n = _fileText.size();
      if (n == 0) {
            Critical("file is empty");
            return 0;
            }
      if (p1.row < 0 || p1.row >= n) {
            Critical("{}: bad line {} max {}", path(), p1.row, n);
            return 0;
            }
      if (p2.row < 0 || p2.row > n) {
            Critical("{}: bad line {} max {}", path(), p2.row, n);
            return 0;
            }
      const Line& l1 = _fileText[p1.row];

      // the x position can point behind the end of the string
      // which will add the (implicit) newline to the char count

      if (p1.col < 0 || p1.col > l1.size()) {
            Critical("bad position");
            return 0;
            }
      // const Line& l2 = _fileText[p2.y()];
      int n2 = columns(p2.row);
      if (p2.col < 0 || p2.col > n2) {
            Critical("bad position");
            return 0;
            }
      if (p1.row == p2.row) // only one line
            return p2.col - p1.col;
      n = 0;
      for (;;) {
            if (c.row == p2.row) { // on last line
                  n += p2.col;
                  break;
                  }
            if (c.row == p1.row) // on first line
                  n += columns(c.row) - c.col;
            else
                  n += columns(c.row); // a complete line
            c.col = 0;
            ++c.row;
            n++; // count implicit newline
            }
      return n;
      }

//---------------------------------------------------------
//   advance
//---------------------------------------------------------

Pos File::advance(const Pos& p, int dist) const {
      if (dist == 0)
            return p;
      int x = p.col;
      int y = p.row;

      if (y >= _fileText.size())
            return p;

      QString s = _fileText[y].qstring();
      while (dist--) {
            if (x >= columns(y)) {
                  ++y;
                  x = 0;
                  if (y >= _fileText.size())
                        break;
                  }
            else
                  ++x;
            }
      return QPoint(x, y);
      }

//---------------------------------------------------------
//   lookupExpansion
//---------------------------------------------------------

static int lookupExpansion(const Lines& text, int idx) {
      int removeLen = 0;
      int lines     = text.size();
      if (idx >= lines)
            return 0;
      Line string = text[idx];
      if (string.qstring().trimmed() != startReplaceSignature)
            return 0;
      ++idx;
      removeLen += string.size() + 1;
      for (; idx < lines; ++idx) {
            string     = text[idx];
            removeLen += string.size() + 1;
            if (string.qstring().trimmed() == endReplaceSignature)
                  break;
            }
      return removeLen;
      }

//---------------------------------------------------------
//   startsWithMacro
//---------------------------------------------------------

static bool startsWithMacro(const Line& ss) {
      auto s = ss.qstring().trimmed();
      return s.startsWith(macroProperty0) || s.startsWith(macroProperty1) || s.startsWith(macroProperty2) || s.startsWith(macroProperty3) ||
             s.startsWith(macroProperty4);
      }

//---------------------------------------------------------
//   markExpansion
//---------------------------------------------------------

void File::markExpansion() {
      int lines        = _fileText.size();
      bool inExpansion = false;
      for (int i = 0; i < lines; ++i) {
            Line& string = _fileText[i];
            QString ts   = string.qstring().trimmed();
            if (inExpansion) {
                  string.setFold(FoldMark::Fold, QColor("green"));
                  string.setLabel('|');
                  if (ts.startsWith(endReplaceSignature))
                        inExpansion = false;
                  }
            else if (ts.startsWith(startReplaceSignature)) {
                  string.setFold(FoldMark::Begin, QColor("green"));
                  inExpansion = true;
                  setFoldFlag(i, true);
                  }
            else
                  string.setFold(FoldMark::No);
            }
      }

//---------------------------------------------------------
//   expandMacros
//---------------------------------------------------------

void File::expandMacros() {
      if (!pyMacros())
            return;
      int lines = _fileText.size();
      for (int i = 0; i < lines; ++i) {
            Line string = _fileText[i];
            if (startsWithMacro(string)) {
                  QString pdata = string.qstring().trimmed().mid(4);
                  for (++i; i < lines; ++i) {
                        string = _fileText[i];
                        if (!startsWithMacro(string))
                              break;
                        if (!pdata.isEmpty())
                              pdata += "\n";
                        pdata += string.qstring().trimmed().mid(4);
                        }
                  QString ss = editor->evalPP(pdata);
                  // look if there is already an expansion
                  //                  ++i;
                  int removeLen = lookupExpansion(_fileText, i);
                  auto pos      = editor->kontext()->cursor();
                  undo()->push(new Patch(this, Pos(0, i), removeLen, ss, pos, pos));
                  lines = _fileText.size();
                  }
            }
      markExpansion();
      }

//---------------------------------------------------------
//   nextLineIsEmpty
//    return true if next line is empty or at end of file
//---------------------------------------------------------

bool Lines::nextLineIsEmpty(int i) const {
      return (i >= (size() - 1)) || at(i + 1).empty();
      }

//---------------------------------------------------------
//   prevLineIsEmpty
//    return true if previous line is empty or at begin of file
//---------------------------------------------------------

bool Lines::prevLineIsEmpty(int i) const {
      return (i == 0) || at(i - 1).empty();
      }

//---------------------------------------------------------
//   isString
//---------------------------------------------------------

bool Line::isString() const {
      bool val = false;
      for (const auto& m : marks()) {
            // col1 points to first character in comment
            // Debug: special case: there is code before the comment
            //            if (m.type == Marker::String)
            //                  Debug("String {} {} <{}>", m.col1, m.col2, qstring());
            if (m.type == Marker::String && m.col1 == 0 && m.col2 == size()) {
                  val = true;
                  break;
                  }
            }
      return val;
      }

//---------------------------------------------------------
//   postprocessFormat
//    Unfortunately clang-format does not support the
//    "Ratliff" or "Banner" bracket indentation style,
//    so we have to fix it here in a small hack.
//---------------------------------------------------------

void File::postprocessFormat() {
      //      int lines   = _fileText.size();
      auto cursor = editor->kontext()->cursor();
      for (int i = 0; i < _fileText.size(); ++i) {
            const Line& l = _fileText[i];
            // dont reformat anything in a string
            if (!l.isString()) {
                  auto s = l.qstring().trimmed();
                  if (!s.isEmpty() && (s[0] == '}' || s[0] == '{'))
                        undo()->push(new Patch(this, {0, i}, 0, "      ", cursor, cursor));
                  }
            //
            // add an empty line after every top level closing "}" or "};"
            //
            if ((l.qstring() == "      }" || l.qstring() == "      };") && !_fileText.nextLineIsEmpty(i)) {
                  undo()->push(new Patch(this, {0, i + 1}, 0, "\n", cursor, cursor));
                  ++i;
                  }
            //
            // make sure there is an empty line before every comment block
            // starting with "//------"
            //
            if (l.startsWith("//-----") && !_fileText.prevLineIsEmpty(i) && i && !_fileText[i - 1].qstring().trimmed().startsWith("//")) {
                  undo()->push(new Patch(this, {0, i}, 0, "\n", cursor, cursor));
                  ++i;
                  }
            }
      markExpansion();
      }

//---------------------------------------------------------
//   fileText
//---------------------------------------------------------

const Line& File::fileText(int row) const {
      static const Line emptyLine;
      if (_fileText.empty() || row >= _fileText.size())
            return emptyLine;
      return _fileText[row];
      }

//---------------------------------------------------------
//   line
//---------------------------------------------------------

const Line& File::line(int row) const {
      const Lines* lines = nullptr;
      switch (_viewMode) {
            default: break;
            case ViewMode::File: lines = &_fileText; break;
            case ViewMode::Functions: lines = &_kollaps; break;
            case ViewMode::GitVersion: lines = &_gitVersion; break;
            case ViewMode::Bugs: lines = &_bugs; break;
            }
            static const Line emptyLine;
      if (!lines || lines->empty() || row >= lines->size())
            return emptyLine;
      return lines->at(row);
      }

//---------------------------------------------------------
//   rows
//---------------------------------------------------------

int File::rows() const {
      int n = 0;
      switch (_viewMode) {
            default:
            case ViewMode::File: n = _fileText.size(); break;
            case ViewMode::Functions: n = _kollaps.size(); break;
            case ViewMode::GitVersion: n = _gitVersion.size(); break;
            case ViewMode::Bugs: n = _bugs.size(); break;
            }
      //      if (readOnly() && n > 0)
      //            n -= 1;
      return n;
      }

//---------------------------------------------------------
//   maxLineLength
//---------------------------------------------------------

int File::maxLineLength() const {
      int maxLen = 0;
      const Lines* lines = nullptr;
      switch (_viewMode) {
            default:
            case ViewMode::File: lines = &_fileText; break;
            case ViewMode::Functions: lines = &_kollaps; break;
            case ViewMode::GitVersion: lines = &_gitVersion; break;
            case ViewMode::Bugs: lines = &_bugs; break;
            }
      if (lines) {
            for (const auto& line : *lines) {
                  if (line.size() > maxLen) {
                        maxLen = line.size();
                        }
                  }
            }
      return maxLen;
      }

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Marks::add(const Mark& newMark) {
      if (newMark.col1 >= newMark.col2) {
            Critical("bad mark: {} >= {}", newMark.col1, newMark.col2);
            return;
            }
      // 2. Finde den Start- und End-Iterator für den zu ersetzenden Block.

      // it_start: Der *erste* Bereich, der *zumindest teilweise* vom neuen Bereich
      //           betroffen ist (d.h. dessen Ende *hinter* dem Start des neuen Bereichs liegt).
      auto it_start = std::find_if(begin(), end(), [&](const Mark& r) { return r.col2 > newMark.col1; });

      // it_end: Der *erste* Bereich, der *nicht mehr* vom neuen Bereich betroffen ist
      //         (d.h. dessen Start *hinter oder gleich* dem Ende des neuen Bereichs liegt).
      auto it_end = std::find_if(it_start, end(), [&](const Mark& r) { return r.col1 >= newMark.col2; });

      // Wenn kein Start gefunden wurde oder der neue Bereich vor allem liegt,
      // sollte dies bei vollständiger Abdeckung nicht passieren.
      if (it_start == end()) {
            // Dieser Fall sollte durch die Invariante "deckt immer alles ab"
            // eigtl. nicht eintreten, es sei denn, der Vektor ist leer.
            Debug("it_start == end()");
            return;
            }

      // 3. Reste der Randbereiche speichern (Splitting)

      // Daten des ersten betroffenen Bereichs für einen möglichen linken Rest
      Mark start_range = *it_start;
      // Daten des letzten betroffenen Bereichs für einen möglichen rechten Rest
      Mark end_range = *(it_end - 1); // Der Bereich *vor* it_end

      std::optional<Mark> left_part;
      std::optional<Mark> right_part;

      // Müssen wir den ersten Bereich aufteilen? (Split-Start)
      if (start_range.col1 < newMark.col1)
            left_part = {start_range.col1, newMark.col1, start_range.type};

      // Müssen wir den letzten Bereich aufteilen? (Split-End)
      if (end_range.col2 > newMark.col2)
            right_part = {newMark.col2, end_range.col2, end_range.type};

      // 4. Überschreiben: Betroffene Bereiche löschen
      // insert_pos zeigt nach dem Löschen auf die Position, an der 'it_end' war.
      auto insert_pos = erase(it_start, it_end);

      // 5. Neue Bereiche einfügen (Reste und den neuen Bereich)
      // Wichtig: 'insert' fügt *vor* dem Iterator ein und gibt einen Iterator
      // auf das eingefügte Element zurück.

      // Rechten Teil einfügen (falls vorhanden)
      if (right_part)
            insert_pos = insert(insert_pos, *right_part);

      // Neuen Bereich einfügen
      auto it_new = insert(insert_pos, newMark);

      // Linken Teil einfügen (falls vorhanden)
      if (left_part)
            it_new = insert(it_new, *left_part);

      // 6. Zusammenfassen (Merging)
      // Überprüfe den eingefügten Block (der jetzt aus 1-3 Teilen besteht)
      // mit seinen direkten Nachbarn.

      // Merge nach rechts
      auto it = it_new; // Startet bei left_part (falls vorhanden) oder newMark
      while (it != end() && (it + 1) != end()) {
            if (it->type == (it + 1)->type) {
                  it->col2 = (it + 1)->col2; // Erweitere den linken Bereich
                  erase(it + 1);             // Lösche den rechten Bereich
                  // Der Iterator 'it' bleibt gültig und wird im nächsten Loop erneut geprüft
                  }
            else {
                  it++; // Kein Merge, weiter zum nächsten
                  }
            }
      }

//---------------------------------------------------------
//   addMark
//---------------------------------------------------------

void File::addMark(int row, int idx1, int idx2, Marker m) {
      if (row >= _fileText.size())
            return;
      Line& line = _fileText[row];
      line.addMark(idx1, idx2, m);
      }

//---------------------------------------------------------
//   clearSearchMarks
//---------------------------------------------------------

bool File::clearSearchMarks() {
      int rv = false;
      for (auto& line : _fileText)
            if (line.clearSearchMarks())
                  rv = true;
      return rv;
      }

//---------------------------------------------------------
//   clearSearchMarks
//---------------------------------------------------------

bool Line::clearSearchMarks() {
      bool rv = false;
      for (;;) {
            bool hasChanged = false;
            for (const auto& m : _marks) {
                  if (m.type == Marker::Search || m.type == Marker::SearchHit) {
                        removeMark(m);
                        hasChanged = true; // iterate again
                        rv         = true;
                        break;
                        }
                  }
            if (!hasChanged)
                  break;
            }
      return rv;
      }

//---------------------------------------------------------
//   clearPrettyMarks
//---------------------------------------------------------

bool Line::clearPrettyMarks() {
      bool rv = false;
      for (auto& m : _marks) {
            if (m.type != Marker::Search && m.type != Marker::Normal) {
                  m.type = Marker::Normal;
                  rv     = true;
                  break;
                  }
            }
      return rv;
      }

//---------------------------------------------------------
//   removeMark
//---------------------------------------------------------

void Line::removeMark(const Mark& m) {
      if (_marks.size() <= 1)
            return;
      for (size_t i = 0; i < _marks.size(); ++i) {
            Mark& mark = _marks[i];
            if (mark == m)
                  mark.type = Marker::Normal;
            }
      }

//---------------------------------------------------------
//   folded
//    check if row is part of a foldable area and return
//    true if its folded (unvisible)
//---------------------------------------------------------

bool File::folded(int row) const {
      if (row >= rows())
            return false;
      const Line& l = line(row);
      FoldMark mark = l.fold();
      if (mark != FoldMark::Fold)
            return false;
      // go back to begin of folded area to find out if
      // area is folded by checking the arrow mark
      while (row--) {
            const Line& l = line(row);
            FoldMark mark = l.fold();
            if (mark == FoldMark::Begin)
                  return l.label() == QChar(0x25b6);
            }
      return false;
      }

//---------------------------------------------------------
//   unfold
//---------------------------------------------------------

void File::unfold(int row) {
      if (row >= rows())
            return;
      const Line& l = line(row);
      FoldMark mark = l.fold();
      if (mark != FoldMark::Fold)
            return;
      while (row--) {
            const Line& l = line(row);
            FoldMark mark = l.fold();
            if (mark == FoldMark::Begin) {
                  setFoldFlag(row, false);
                  break;
                  }
            }
      }

//---------------------------------------------------------
//   setFoldFlag
//---------------------------------------------------------

void File::setFoldFlag(int row, bool folded) {
      setLabel(row, folded ? QChar(0x25b6) : QChar(0x25bc));
      }

//---------------------------------------------------------
//   toggleFold
//---------------------------------------------------------

void File::toggleFold(int row) {
      const Line& l = fileText(row);
      if (l.fold() == FoldMark::Begin)
            setFoldFlag(row, l.label() != QChar(0x25b6));
      }

//---------------------------------------------------------
//   isFoldable
//---------------------------------------------------------

bool File::isFoldable(int row) const {
      const Line& l = fileText(row);
      return l.fold() != FoldMark::No;
      }

//---------------------------------------------------------
//   setLabel
//---------------------------------------------------------

void Line::setLabel(QChar c, QColor color) {
      _label.text  = c;
      _label.color = color;
      }

//---------------------------------------------------------
//   foldAll
//---------------------------------------------------------

void File::foldAll(bool v) {
      Debug("{}", v);
      for (int row = 0; row < _fileText.size(); ++row) {
            const Line& l = fileText(row);
            if (l.fold() == FoldMark::Begin)
                  setFoldFlag(row, v);
            }
      }

//---------------------------------------------------------
//   setFold
//---------------------------------------------------------

void Line::setFold(FoldMark m, QColor color) {
      _fold        = m;
      _label.color = color;
      }

//---------------------------------------------------------
//   nextRow
//    return next visible row
//    return -1 if there is no next row
//---------------------------------------------------------

int File::nextRow(int row) const {
      if (row >= rows() - 1)
            return -1;
      ++row;
      while (row < (rows() - 1) && folded(row))
            ++row;
      if (folded(row))
            return -1;
      return row;
      }

//---------------------------------------------------------
//   previousRow
//    return previous visible row
//    return -1 if there is no previous row
//---------------------------------------------------------

int File::previousRow(int row) const {
      if (row <= 0)
            return -1;
      --row;
      while (row > 0 && folded(row))
            --row;
      if (folded(row))
            return -1;
      return row;
      }

//---------------------------------------------------------
//   searchReplace
//---------------------------------------------------------

bool File::searchReplace(const QString& search, const QString& replaceText) {
      Debug("<{}> -- <{}>", search, replaceText);

      if (search.isEmpty())
            return false; // Nichts zu suchen

      QStringList searchLines = search.split('\n');
      int numSearchLines      = searchLines.size();
      int searchLen           = search.length();

      QStringList l = _fileText.toStringList();

      // Wir sammeln alle Treffer in einer Liste, um sie später rückwärts zu ersetzen
      QList<Pos> matches;

      if (numSearchLines == 1) {
            // --- Fall 1: Einzeilige Suche ---
            for (int i = 0; i < l.size(); ++i) {
                  int col = 0;
                  // Finde alle Vorkommen in der aktuellen Zeile
                  while ((col = l[i].indexOf(search, col)) != -1) {
                        matches.append({col, i});
                        col += searchLen; // Weiter suchen nach dem aktuellen Treffer
                        }
                  }
            }
      else {
            // --- Fall 2: Mehrzeilige Suche ---
            for (int i = 0; i <= l.size() - numSearchLines; ++i) {
                  const QString& firstSearchLine = searchLines.first();

                  // 1. Endet die aktuelle Zeile mit dem Anfang des Suchstrings?
                  if (!l[i].endsWith(firstSearchLine))
                        continue;

                  bool isMatch = true;

                  // 2. Stimmen die kompletten mittleren Zeilen exakt überein?
                  for (int k = 1; k < numSearchLines - 1; ++k) {
                        if (l[i + k] != searchLines[k]) {
                              isMatch = false;
                              break;
                              }
                        }

                  if (!isMatch)
                        continue;

                  // 3. Beginnt die abschließende Zeile mit dem Ende des Suchstrings?
                  if (!l[i + numSearchLines - 1].startsWith(searchLines.last()))
                        continue;

                  // Treffer gefunden! Spalte berechnen (Länge der Zeile minus Länge des Such-Anfangs)
                  int col = l[i].length() - firstSearchLine.length();
                  matches.append({col, i});

                  // Index voranschreiten lassen, um nicht innerhalb des gefundenen Blocks weiterzusuchen
                  i += numSearchLines - 2;
                  }
            }

      if (matches.isEmpty()) {
            Debug("no matches");
            return false; // Kein Treffer gefunden
            }

      // --- Das eigentliche Ersetzen ---
      // Wir iterieren RÜCKWÄRTS durch die Treffer. Das verhindert, dass sich
      // Zeilen/Spalten-Indizes der noch unberührten Treffer verschieben, wenn
      // "replaceText" eine andere Länge hat als "search".
      for (int j = matches.size() - 1; j >= 0; --j) {
            Debug("patch {}.{}: len {}", matches[j].row, matches[j].col, searchLen);
            undo()->push(new Patch(this, matches[j], searchLen, replaceText, Cursor(), Cursor()));
            }
      return true;
      }
