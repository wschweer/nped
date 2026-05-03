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

#include <QApplication>
#include <QTreeView>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QMainWindow>
#include <QBoxLayout>
#include <QToolBar>
#include <QClipboard>
#include <QCompleter>
#include <QDir>
#include <QFile>
#include <QComboBox>
#include <QLabel>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QKeyEvent>
#include <QStringList>
#include <QListWidget>
#include <QProgressBar>
#include <QMetaType>
#include <QDataStream>
#include <QPainter>

#include <functional>
#include <thread>
#include <vector>

#include "editor.h"
#include "editwin.h"
#include "file.h"
#include "kontext.h"
#include "lsclient.h"
#include "logger.h"
#include "undo.h"
#include "agent.h"
#include "webview.h"
#include "completion.h"
#include "textstyle.h"
#include "session.h"
// #include "screenshot.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

//---------------------------------------------------------
//   lessThan
//---------------------------------------------------------

bool ProjectFileProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
      if (!left.isValid() || !right.isValid())
            return false;

      QFileSystemModel* fsModel = qobject_cast<QFileSystemModel*>(sourceModel());

      if (!fsModel)
            return false;

      // Verzeichnisse immer vor Dateien
      bool leftIsDir  = fsModel->isDir(left);
      bool rightIsDir = fsModel->isDir(right);

      if (leftIsDir && !rightIsDir)
            return true;

      if (!leftIsDir && rightIsDir)
            return false;

      if (leftIsDir && rightIsDir) {
            // Beide sind Verzeichnisse -> alphabetisch nach Name
            return QString::compare(left.data(QFileSystemModel::FileNameRole).toString(),
                                    right.data(QFileSystemModel::FileNameRole).toString(),
                                    Qt::CaseInsensitive) < 0;
            }

      // Beide sind Dateien -> erst nach Extension, dann nach Name
      QString leftName  = left.data(QFileSystemModel::FileNameRole).toString();
      QString rightName = right.data(QFileSystemModel::FileNameRole).toString();

      int leftDotPos   = leftName.lastIndexOf(".");
      int rightDotPos  = rightName.lastIndexOf(".");
      QString leftExt  = (leftDotPos != -1) ? leftName.mid(leftDotPos) : QString();
      QString rightExt = (rightDotPos != -1) ? rightName.mid(rightDotPos) : QString();

      // Dateien ohne Extension zuerst

      if (leftExt.isEmpty() && !rightExt.isEmpty())
            return true;

      if (!leftExt.isEmpty() && rightExt.isEmpty())
            return false;

      int extCmp = QString::compare(leftExt, rightExt, Qt::CaseInsensitive);

      if (extCmp != 0)
            return extCmp < 0;

      // Gleiche Extension -> nach Basisname sortieren

      QString leftBase  = (leftDotPos != -1) ? leftName.left(leftDotPos) : leftName;
      QString rightBase = (rightDotPos != -1) ? rightName.left(rightDotPos) : rightName;
      return QString::compare(leftBase, rightBase, Qt::CaseInsensitive) < 0;
      }

//---------------------------------------------------------
//   tintPixmap
//---------------------------------------------------------

static QPixmap tintPixmap(const QPixmap& src, const QColor& color) {
      QPixmap pixmap = src;
      QPainter painter(&pixmap);
      painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
      painter.fillRect(pixmap.rect(), color);
      painter.end();
      return pixmap;
      }

//---------------------------------------------------------
//   createStatefulIcon
//---------------------------------------------------------

QIcon Editor::createStatefulIcon(const QString& svgPath, const QColor& normalColor, const QColor& hoverColor,
                                 const QColor& checkedColor) {
      QIcon icon;
      QPixmap basePixmap = QIcon(svgPath).pixmap(24, 24);

      icon.addPixmap(tintPixmap(basePixmap, normalColor), QIcon::Normal, QIcon::Off);
      if (hoverColor.isValid())
            icon.addPixmap(tintPixmap(basePixmap, hoverColor), QIcon::Active, QIcon::Off);
      if (checkedColor.isValid())
            icon.addPixmap(tintPixmap(basePixmap, checkedColor), QIcon::Normal, QIcon::On);

      icon.addPixmap(tintPixmap(basePixmap, Qt::gray), QIcon::Disabled, QIcon::Off);

      return icon;
      }

extern bool persistent;

//---------------------------------------------------------
//   _shortcuts
//---------------------------------------------------------

std::map<Cmd, ShortcutConfig> Editor::_shortcuts = {
         {                 Cmd::CMD_QUIT,                    {"CMD_QUIT", "Quit", "Ctrl + K, Ctrl + Q; Shift+F1"}},
         {            Cmd::CMD_SAVE_QUIT,                                    {"CMD_SAVE_QUIT", "Save+Quit", "F1"}},
         {                 Cmd::CMD_SAVE,                             {"CMD_SAVE", "Save", "Ctrl + K,  Ctrl + S"}},
         {           Cmd::CMD_CHAR_RIGHT,                          {"CMD_CHAR_RIGHT", "Right", "Right; Ctrl + D"}},
         {            Cmd::CMD_CHAR_LEFT,                            {"CMD_CHAR_LEFT", "Left", "Left;  Ctrl + S"}},
         {              Cmd::CMD_LINE_UP,                                 {"CMD_LINE_UP", "Up", "Up;   Ctrl + E"}},
         {            Cmd::CMD_LINE_DOWN,                         {"CMD_LINE_DOWN", "Down", "Down;     Ctrl + X"}},
         {           Cmd::CMD_LINE_START, {"CMD_LINE_START", "Line Start", "Ctrl + Q,    Ctrl + S; Shift + Left"}},
         {             Cmd::CMD_LINE_END,      {"CMD_LINE_END", "Line End", "Ctrl + Q,  Ctrl + D; Shift + Right"}},
         {             Cmd::CMD_LINE_TOP,                     {"CMD_LINE_TOP", "Line Top", "Ctrl + Q,  Ctrl + E"}},
         {          Cmd::CMD_LINE_BOTTOM,               {"CMD_LINE_BOTTOM", "Line Bottom", "Ctrl + Q,  Ctrl + X"}},
         {              Cmd::CMD_PAGE_UP,                           {"CMD_PAGE_UP", "Page Up", "Ctrl + R;  PgUp"}},
         {            Cmd::CMD_PAGE_DOWN,                     {"CMD_PAGE_DOWN", "Page Down", "Ctrl + C;  PgDown"}},
         {           Cmd::CMD_FILE_BEGIN,         {"CMD_FILE_BEGIN", "Start", "Ctrl + Q,  Ctrl + R; Ctrl + PgUp"}},
         {             Cmd::CMD_FILE_END,           {"CMD_FILE_END", "End", "Ctrl + Q,  Ctrl + C; Ctrl + PgDown"}},
         {            Cmd::CMD_WORD_LEFT,                 {"CMD_WORD_LEFT", "Word Left", "Ctrl + A; Ctrl + Left"}},
         {           Cmd::CMD_WORD_RIGHT,              {"CMD_WORD_RIGHT", "Word Right", "Ctrl + F; Ctrl + Right"}},
         {                Cmd::CMD_ENTER,                                        {"CMD_ENTER", "Enter", "Escape"}},
         {         Cmd::CMD_KONTEXT_COPY,         {"CMD_KONTEXT_COPY", "Copy Kontext", "Ctrl + K,  Ctrl + K; F4"}},
         {         Cmd::CMD_KONTEXT_PREV, {"CMD_KONTEXT_PREV", "Kontext Left", "Ctrl + K,  Ctrl + J; Shift + F3"}},
         {         Cmd::CMD_KONTEXT_NEXT,        {"CMD_KONTEXT_NEXT", "Kontext Right", "Ctrl + K,  Ctrl + L; F3"}},
         {        Cmd::CMD_SHOW_BRACKETS,           {"CMD_SHOW_BRACKETS", "Show Brackets", "Ctrl + K,  Ctrl + I"}},
         {           Cmd::CMD_SHOW_LEVEL,                 {"CMD_SHOW_LEVEL", "Show Level", "Ctrl + K,  Ctrl + M"}},
         {           Cmd::CMD_KONTEXT_UP,                           {"CMD_KONTEXT_UP", "Kontext Up", "Ctrl + Up"}},
         {         Cmd::CMD_KONTEXT_DOWN,                     {"CMD_KONTEXT_DOWN", "Kontext Down", "Ctrl + Down"}},
         {    Cmd::CMD_DELETE_LINE_RIGHT,   {"CMD_DELETE_LINE_RIGHT", "Delete Line Right", "Ctrl + Q,  Ctrl + Y"}},
         {                 Cmd::CMD_UNDO,                                        {"CMD_UNDO", "Undo", "Ctrl + Z"}},
         {                 Cmd::CMD_REDO,                                {"CMD_REDO", "Redo", "Shift + Ctrl + Z"}},

         {               Cmd::CMD_RUBOUT,                           {"CMD_RUBOUT", "Rubout", "Delete; Backspace"}},
         {          Cmd::CMD_CHAR_DELETE,                               {"CMD_CHAR_DELETE", "Delete", "Ctrl + G"}},
         {          Cmd::CMD_INSERT_LINE,                          {"CMD_INSERT_LINE", "Insert Line", "Ctrl + N"}},
         {                  Cmd::CMD_TAB,                                         {"CMD_TAB", "Tabulator", "Tab"}},

         {           Cmd::CMD_SELECT_ROW,                                  {"CMD_SELECT_ROW", "Row Select", "F5"}},
         {           Cmd::CMD_SELECT_COL,                                  {"CMD_SELECT_COL", "Col Select", "F6"}},
         {          Cmd::CMD_SELECT_CHAR,                      {"CMD_SELECT_CHAR", "Row/Col Select", "Ctrl + F5"}},
         {                 Cmd::CMD_PICK,                                       {"CMD_PICK", "Copy (Pick)", "F8"}},
         {                  Cmd::CMD_PUT,                                        {"CMD_PUT", "Paste (Put)", "F9"}},
         {          Cmd::CMD_DELETE_LINE,                                  {"CMD_DELETE_LINE", "Cut", "Ctrl + Y"}},

         {          Cmd::CMD_FLIP_CURSOR,      {"CMD_FLIP_CURSOR", "Flip Selection Cursor", "Ctrl + Q, Ctrl + F"}},
         {          Cmd::CMD_SEARCH_NEXT,                                {"CMD_SEARCH_NEXT", "Search Next", "F7"}},
         {               Cmd::CMD_RENAME,                   {"CMD_RENAME", "Global Rename", "Ctrl + O, Ctrl + R"}},
         {          Cmd::CMD_SEARCH_PREV,                        {"CMD_SEARCH_PREV", "Search Prev", "SHIFT + F7"}},
         {          Cmd::CMD_DELETE_WORD,                          {"CMD_DELETE_WORD", "Delete Word", "Ctrl + T"}},
         {           Cmd::CMD_ENTER_WORD,                 {"CMD_ENTER_WORD", "Enter Word", "Ctrl + O,  Ctrl + W"}},
         {            Cmd::CMD_GOTO_BACK,                         {"CMD_GOTO_BACK", "History Back", "Ctrl + F12"}},
         {               Cmd::CMD_FORMAT,                         {"CMD_FORMAT", "Format", "Ctrl + O,  Ctrl + F"}},
         {       Cmd::CMD_VIEW_FUNCTIONS,                       {"CMD_VIEW_FUNCTIONS", "Toggle View", "Ctrl + V"}},
         {          Cmd::CMD_ANNOTATIONS,                     {"CMD_VIEW_BUGS", "View LS Annotation", "Ctrl + B"}},
         { Cmd::CMD_GOTO_TYPE_DEFINITION,             {"CMD_GOTO_TYPE_DEFINITION", "Goto Type Definition", "F10"}},
         {  Cmd::CMD_GOTO_IMPLEMENTATION,               {"CMD_GOTO_IMPLEMENTATION", "Goto Implementation", "F11"}},
         {      Cmd::CMD_GOTO_DEFINITION,                       {"CMD_GOTO_DEFINITION", "Goto Definition", "F12"}},
         {        Cmd::CMD_EXPAND_MACROS,            {"CMD_EXPAND_MACROS", "Expand Macros", "Ctrl + O, Ctrl + E"}},
         {          Cmd::CMD_COMPLETIONS,      {"CMD_COMPLETIONS", "Show Completions", "Ctrl + Tab; Shift + Tab"}},
         {             Cmd::CMD_FOLD_ALL,                                {"CMD_FOLD_ALL", "Fold All", "Ctrl + M"}},
         {           Cmd::CMD_UNFOLD_ALL,                    {"CMD_UNFOLD_ALL", "Unfold All", "Ctrl + Shift + M"}},
         {          Cmd::CMD_FOLD_TOGGLE,                          {"CMD_FOLD_TOGGLE", "Fold Toggle", "Ctrl + <"}},
         {      Cmd::CMD_FUNCTION_HEADER,          {"CMD_FUNCTION_HEADER", "Create Header", "Ctrl + O, Ctrl + H"}},

         {           Cmd::CMD_TOGGLE_GIT,            {"CMD_TOGGLE_GIT", "Toggle Git Panel", "Ctrl + O, Ctrl + G"}},
         {       Cmd::CMD_TOGGLE_PROJECT,    {"CMD_TOGGLE_PROJECT", "Toggle Project Panel", "Ctrl + O, Ctrl + P"}},
         {            Cmd::CMD_TOGGLE_AI,                        {"CMD_TOGGLE_AI", "Toggle AI Panel", "Ctrl + I"}},
         {        Cmd::CMD_TOGGLE_CONFIG,                {"CMD_TOGGLE_CONFIG", "Toggle Config Panel", "Ctrl + C"}},

         {       Cmd::CMD_ENTER_ADD_FILE,                                {"CMD_ENTER_ADD_FILE", "Add File", "F3"}},
         {         Cmd::CMD_ENTER_SEARCH,                            {"CMD_ENTER_SEARCH", "Search/Replace", "F7"}},
         {Cmd::CMD_ENTER_CREATE_FUNCTION,              {"CMD_ENTER_CREATE_FUNCTION", "Create Function", "Ctrl+F"}},
         {      Cmd::CMD_ENTER_GOTO_LINE,                          {"CMD_ENTER_GOTO_LINE", "Goto Line", "Ctrl+G"}},
         {           Cmd::CMD_SCREENSHOT,       {"CMD_SCREENSHOT", "Screen Shot", "Ctrl + O, Ctrl + O, Ctrl + P"}},
         {            Cmd::CMD_LINK_BACK,                           {"CMD_LINK_BACK", "Link Back", "Ctrl + PgUp"}},
         {         Cmd::CMD_LINK_FORWARD,                   {"CMD_LINK_FORWARD", "Link Forward", "Ctrl + PgDown"}},
      };

//---------------------------------------------------------
//   screenshot
//---------------------------------------------------------

void Editor::screenshot() {
      QImage image(size(), QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter painter(&image);
      render(&painter);
      painter.end();

      int n = 0;
      QString path;
      while (true) {
            path = QString("screenshot-%1.jpg").arg(n, 2, 10, QChar('0'));
            if (!QFile::exists(path))
                  break;
            n++;
            }
      if (image.save(path, "JPG"))
            msg("Screenshot saved to {}", path.toStdString());
      else
            msg("Screenshot save failed");
      emit screenshotReady(image);
      }

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void KeyLogger::clear() {
      keys[0] = QKeyCombination::fromCombined(0);
      keys[1] = QKeyCombination::fromCombined(0);
      keys[2] = QKeyCombination::fromCombined(0);
      keys[3] = QKeyCombination::fromCombined(0);
      n       = 0;
      }

//---------------------------------------------------------
//   eventFilter
//---------------------------------------------------------

bool KeyLogger::eventFilter(QObject* obj, QEvent* event) {
      if (event->type() != QEvent::KeyPress)
            return QObject::eventFilter(obj, event);
      QKeyEvent* e = static_cast<QKeyEvent*>(event);

      // ignore modifier only events:
      if (!(e->key() & ~(Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
            return QObject::eventFilter(obj, event);

      if (n == 4)
            clear();
      keys[n++]    = e->keyCombination();
      bool partial = false;
      for (auto& a : *actions) {
            for (const auto& sc : a.seq) {
                  int i = 0;
                  for (; i < 4; ++i)
                        if (keys[i] != sc[i])
                              break;
                  if (i == 4) {
                        partial = false;
                        emit triggered(&a);
                        clear();
                        emit keyLabelChanged("");
                        return true;
                        }
                  else if ((i >= n) && (sc[i] != QKeyCombination::fromCombined(0)))
                        partial = true;
                  }
            }
      if (!partial)
            clear();
      emit keyLabelChanged(
          QKeySequence(keys[0], keys[1], keys[2], keys[3]).toString(QKeySequence::NativeText));
      return QObject::eventFilter(obj, event);
      }

//---------------------------------------------------------
//   getSC
//---------------------------------------------------------

const ShortcutConfig& Editor::getSC(Cmd cmd) {
      return _shortcuts[cmd];
      }

//---------------------------------------------------------
//   shortcuts
//---------------------------------------------------------

QList<ShortcutConfig> Editor::shortcuts() const {
      QList<ShortcutConfig> l;
      for (const auto& [key, val] : _shortcuts)

            l.push_back(val);
      return l;
      }

//---------------------------------------------------------
//   updateStyle
//---------------------------------------------------------

void Editor::updateStyle() {
      QString styleFile = darkMode() ? ":/src/dark.qss" : ":/src/light.qss";
      QFile file(styleFile);
      if (file.open(QFile::ReadOnly)) {
            QString style = QLatin1String(file.readAll());
            qApp->setStyleSheet(style);
            }
      }

//---------------------------------------------------------
//   Editor
//---------------------------------------------------------

Editor::Editor(int argc, char** argv) : QMainWindow(nullptr) {
      if (!initProject())
            Critical("init project failed");
      resetToDefaults();
      loadSettings();

      bool found = false;
      for (const auto& project : _projects) {
            Debug("{} {}", _projectRoot, project);
            if (_projectRoot == project) {
                  found = true;
                  break;
                  }
            }
      if (!found) {
            _projects.push_front(_projectRoot);
            saveSettings();
            }

      // Associate a KeySequence with a function which performs an editor action.
      // The function is surrounded by an startCmd() and an endCmd() call.

      _pedActions = {
         Action(getSC(Cmd::CMD_QUIT), [this] { quitCmd(); }),
         Action(getSC(Cmd::CMD_SAVE_QUIT), [this] { saveQuitCmd(); }),

         Action(getSC(Cmd::CMD_CHAR_RIGHT), [this] { kontext()->moveCursorRel(1, 0); }),
         Action(getSC(Cmd::CMD_CHAR_LEFT), [this] { kontext()->moveCursorRel(-1, 0); }),
         Action(getSC(Cmd::CMD_LINE_UP),
                [this] {
                      if (completionsPopup->isVisible()) {
                            completionsPopup->up();
                            return;
                            }
                      kontext()->moveCursorRel(0, -1);
                      }),
         Action(getSC(Cmd::CMD_LINE_DOWN),
                [this] {
                      if (completionsPopup->isVisible()) {
                            completionsPopup->down();
                            return;
                            }
                      kontext()->moveCursorRel(0, 1);
                      }),
         Action(getSC(Cmd::CMD_LINE_START), [this] { kontext()->moveCursorAbs(0, -1); }),
         Action(getSC(Cmd::CMD_LINE_END),
                [this] { kontext()->moveCursorAbs(kontext()->currentLine().size(), -1); }),
         Action(getSC(Cmd::CMD_LINE_TOP), [this] { kontext()->moveCursorTopLine(); }),
         Action(getSC(Cmd::CMD_LINE_BOTTOM), [this] { kontext()->moveCursorBottomLine(); }),
         Action(getSC(Cmd::CMD_PAGE_UP),
                [this] {
                      int n = (_editWidget->rows() * 3) / 4;
                      kontext()->moveCursorRel(0, -n, MoveType::Page);
                      }),
         Action(getSC(Cmd::CMD_PAGE_DOWN),
                [this] {
                      int n = (_editWidget->rows() * 3) / 4;
                      kontext()->moveCursorRel(0, n, MoveType::Page);
                      }),
         Action(getSC(Cmd::CMD_FILE_BEGIN), [this] { kontext()->moveCursorAbs(-1, 0); }),
         Action(getSC(Cmd::CMD_FILE_END), [this] { kontext()->moveCursorAbs(-1, kontext()->rows() - 1); }),
         Action(getSC(Cmd::CMD_WORD_LEFT), [this] { kontext()->movePrevWord(); }),
         Action(getSC(Cmd::CMD_WORD_RIGHT), [this] { kontext()->moveNextWord(); }),
         Action(getSC(Cmd::CMD_ENTER), [this] { enterCmd(); }),

         Action(getSC(Cmd::CMD_SAVE), [this] { kontext()->file()->save(); }),
         Action(getSC(Cmd::CMD_KONTEXT_COPY), [this] { copyKontext(); }),
         Action(getSC(Cmd::CMD_KONTEXT_PREV), [this] { prevKontext(); }),
         Action(getSC(Cmd::CMD_KONTEXT_NEXT), [this] { nextKontext(); }),
         Action(getSC(Cmd::CMD_SHOW_BRACKETS), [this] { cBrackets(); }),
         Action(getSC(Cmd::CMD_SHOW_LEVEL), [this] { cLevel(); }),

         Action(getSC(Cmd::CMD_KONTEXT_UP), [this] { kontext()->moveCursorRel(0, -1, MoveType::Roll); }),
         Action(getSC(Cmd::CMD_KONTEXT_DOWN), [this] { kontext()->moveCursorRel(0, 1, MoveType::Roll); }),
         Action(getSC(Cmd::CMD_DELETE_LINE), [this] { deleteLine(); }),
         Action(getSC(Cmd::CMD_DELETE_LINE_RIGHT), [this] { deleteRestOfLine(); }),
         Action(getSC(Cmd::CMD_UNDO), [this] { kontext()->file()->undo()->undo(); }),
         Action(getSC(Cmd::CMD_REDO), [this] { kontext()->file()->undo()->redo(); }),
         Action(getSC(Cmd::CMD_RUBOUT), [this] { rubout(); }),
         Action(getSC(Cmd::CMD_CHAR_DELETE), [this] { deleteChar(); }),
         Action(getSC(Cmd::CMD_INSERT_LINE),
                [this] {
                      kontext()->moveCursorAbs(kontext()->currentLine().size(), -1);
                      input("\n");
                      }),

         Action(getSC(Cmd::CMD_TAB), [this] { insertTab(); }),
         Action(getSC(Cmd::CMD_PICK), [this] { pick(); }),
         Action(getSC(Cmd::CMD_PUT), [this] { put(); }),

         Action(getSC(Cmd::CMD_SELECT_ROW), [this] { rowSelect(); }),
         Action(getSC(Cmd::CMD_SELECT_COL), [this] { colSelect(); }),
         Action(getSC(Cmd::CMD_SELECT_CHAR), [this] { charSelect(); }),
         Action(getSC(Cmd::CMD_FLIP_CURSOR), [this] { kontext()->flipSelectionCursor(); }),

         Action(getSC(Cmd::CMD_FORMAT), [this] { formatting(); }),
         Action(getSC(Cmd::CMD_VIEW_FUNCTIONS), [this] { toggleViewMode(); }),
         Action(getSC(Cmd::CMD_ANNOTATIONS),
                [this] {
                      if (kontext()->viewMode() == ViewMode::Annotations)
                            setViewMode(ViewMode::File);
                      else
                            setViewMode(ViewMode::Annotations);
                      }),
         Action(getSC(Cmd::CMD_SEARCH_NEXT), [this] { searchNext(); }),
         Action(getSC(Cmd::CMD_SEARCH_PREV), [this] { searchPrev(); }),
         Action(getSC(Cmd::CMD_DELETE_WORD), [this] { deleteNextWord(); }),
         Action(getSC(Cmd::CMD_ENTER_WORD), [this] { getNextWord(); }),
         Action(getSC(Cmd::CMD_GOTO_TYPE_DEFINITION), [this] { gotoTypeDefinition(); }),
         Action(getSC(Cmd::CMD_GOTO_IMPLEMENTATION), [this] { gotoImplementation(); }),
         Action(getSC(Cmd::CMD_GOTO_DEFINITION), [this] { gotoDefinition(); }),
         Action(getSC(Cmd::CMD_GOTO_BACK), [this] { backKontext(); }),
         Action(getSC(Cmd::CMD_EXPAND_MACROS), [] {}), // dummy
         Action(getSC(Cmd::CMD_COMPLETIONS), [this] { requestCompletions(); }),
         Action(getSC(Cmd::CMD_FUNCTION_HEADER), [this] { kontext()->createFunctionHeader(); }),
         Action(getSC(Cmd::CMD_TOGGLE_AI), [this] { hover(); }),
         Action(getSC(Cmd::CMD_TOGGLE_GIT), [this] { _gitButton->setChecked(!_gitButton->isChecked()); }),
         Action(getSC(Cmd::CMD_TOGGLE_PROJECT),
                [this] { _projectButton->setChecked(!_projectButton->isChecked()); }),
         Action(getSC(Cmd::CMD_TOGGLE_CONFIG),
                [this] { configButton->setChecked(!configButton->isChecked()); }),
         Action(getSC(Cmd::CMD_FOLD_ALL), [this] { foldAll(); }),
         Action(getSC(Cmd::CMD_UNFOLD_ALL), [this] { unfoldAll(); }),
         Action(getSC(Cmd::CMD_FOLD_TOGGLE), [this] { foldToggle(); }),
         //             Action(getSC(Cmd::CMD_SEARCH_LIST, [this] { setViewMode(ViewMode::SearchResults); }),
         Action(getSC(Cmd::CMD_RENAME), [this] { rename(); }),
         Action(getSC(Cmd::CMD_SCREENSHOT), [this] { screenshot(); }),
            };

      KeyLogger* kl = new KeyLogger(&_pedActions, this);
      connect(kl, &KeyLogger::triggered, [this](Action* a) {
            startCmd();
            a->func();
            endCmd();
            keyLabel()->setText("");
            });
      connect(kl, &KeyLogger::keyLabelChanged, [this](QString s) { _keyLabel->setText(s); });

      box = new QWidget;
      setCentralWidget(box);

      QVBoxLayout* vbox = new QVBoxLayout();
      box->setLayout(vbox);
      vbox->setContentsMargins(0, 0, 0, 0);
      vbox->setSpacing(0);

      splitter = new QSplitter(Qt::Horizontal, box);
      splitter->setOpaqueResize(true);

      auto w            = new QWidget();
      QGridLayout* grid = new QGridLayout(w);
      grid->setSpacing(0);
      grid->setContentsMargins(0, 0, 0, 0);

      QWidget* hboxW    = new QWidget();
      QHBoxLayout* hbox = new QHBoxLayout(hboxW);
      tabBar            = new TabBar();
      tabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      hbox->addWidget(tabBar, 10, Qt::AlignLeft);

      //*****************************************
      //    ProjectButton
      //*****************************************

      _projectButton = new QToolButton();
      _projectButton->setText("P");
      _projectButton->setCheckable(true);
      _projectButton->setToolTip("toggle Project panel");
      hbox->addWidget(_projectButton, 0, Qt::AlignRight);
      connect(_projectButton, &QToolButton::toggled, [this] {
            bool visible = _projectButton->isChecked();
            int pIndex   = splitter->indexOf(_projectPanel);
            if (!visible && this->isVisible())
                  projectWidth = splitter->sizes()[pIndex];
            _projectPanel->setVisible(visible);
            if (visible)
                  updateProjectPanel();
            if (!this->isVisible())
                  return;

            int newWidth = width();
            if (visible) {
                  QList<int> sizes = splitter->sizes();
                  sizes[pIndex]    = projectWidth;
                  splitter->setSizes(sizes);
                  newWidth += projectWidth + splitter->handleWidth();
                  }
            else {
                  newWidth += -(projectWidth + splitter->handleWidth());
                  }
            resize(newWidth, height());
            });

      //*****************************************
      //    AiButton
      //*****************************************

      aiButton = new QToolButton();
      aiButton->setText("Ai");
      aiButton->setCheckable(true);
      //      aiButton->setChecked(false);
      aiButton->setToolTip("toggle AI panel");
      hbox->addWidget(aiButton, 0, Qt::AlignRight);
      connect(aiButton, &QToolButton::toggled, [this] {
            bool visible       = aiButton->isChecked();
            bool wasGitVisible = false;
            if (visible) {
                  wasGitVisible = _gitButton->isChecked();
                  const QSignalBlocker blocker(_gitButton);
                  _gitButton->setChecked(false);
                  _sidePanelStack->setCurrentWidget(agent());
                  }
            int sideIndex = splitter->indexOf(_sidePanelStack);
            if (!visible && !_gitButton->isChecked() && this->isVisible())
                  agentWidth = splitter->sizes()[sideIndex];

            bool showStack = visible || _gitButton->isChecked();
            _sidePanelStack->setVisible(showStack);

            if (!this->isVisible())
                  return;

            int newWidth = width();
            if (visible) {
                  QList<int> sizes = splitter->sizes();
                  sizes[sideIndex] = agentWidth;
                  splitter->setSizes(sizes);
                  if (wasGitVisible)
                        newWidth += agentWidth - gitWidth;
                  else
                        newWidth += agentWidth + splitter->handleWidth();
                  }
            else
                  newWidth += -(agentWidth + splitter->handleWidth());
            resize(newWidth, height());
            });

      _stack      = new QStackedWidget(this);
      hScroll     = new QScrollBar(Qt::Horizontal);
      vScroll     = new QScrollBar(Qt::Vertical);
      _editWidget = new EditWidget(nullptr, this);

      _stack->addWidget(_editWidget);
      //      _stack->addWidget(mdContainer);
      updateStyle();
      _editWidget->setFocus();
      connect(_editWidget, &EditWidget::markerClicked, [this](int row) {
            kontext()->file()->toggleFold(row);
            update();
            });

      grid->addWidget(hboxW, 0, 0, 1, 2);
      grid->addWidget(_stack, 1, 0);
      grid->addWidget(vScroll, 1, 1);
      grid->addWidget(hScroll, 2, 0);

      grid->setRowStretch(1, 500);
      grid->setColumnStretch(0, 500);
      _projectPanel = new QWidget(box);
      _projectPanel->setObjectName("projectPanel");
      _projectPanel->setMinimumWidth(projectPanelMinimumWidth);
      _projectPanel->setVisible(false);
      QVBoxLayout* projectLayout = new QVBoxLayout(_projectPanel);
      projectLayout->setContentsMargins(0, 0, 0, 0);
      projectLayout->setSpacing(0);

      _projectComboBox = new QComboBox(_projectPanel);
      projectLayout->addWidget(_projectComboBox);
      connect(_projectComboBox, &QComboBox::activated, this,
              [this](int index) { switchProject(_projectComboBox->itemText(index)); });
      _projectTreeView = new QTreeView(_projectPanel);
      _projectModel    = new QFileSystemModel(this);
      _projectModel->setFilter(QDir::NoDotAndDotDot | QDir::AllEntries);

      _projectProxyModel = new ProjectFileProxyModel(this);
      _projectProxyModel->setSourceModel(_projectModel);
      _projectProxyModel->sort(0);

      _projectTreeView->setModel(_projectProxyModel);
      _projectTreeView->setHeaderHidden(true);
      for (int i = 1; i < _projectProxyModel->columnCount(); ++i)
            _projectTreeView->hideColumn(i);
      projectLayout->addWidget(_projectTreeView);
      connect(_projectTreeView, &QTreeView::doubleClicked, [this](const QModelIndex& index) {
            QString path = static_cast<QFileSystemModel*>(_projectProxyModel->sourceModel())
                               ->filePath(_projectProxyModel->mapToSource(index));
            QFileInfo fi(path);
            if (fi.isFile() && !path.isEmpty()) {
                  addFile(path);
                  setCurrentKontext(_kontextList.size() - 1);
                  }
            });
      splitter->addWidget(_projectPanel);
      int pIndex = splitter->indexOf(_projectPanel);
      splitter->setCollapsible(pIndex, false);
      splitter->addWidget(w);
      int wIndex = splitter->indexOf(w);
      splitter->setStretchFactor(wIndex, 100);

      _sidePanelStack = new QStackedWidget(box);
      _sidePanelStack->setVisible(false);
      splitter->addWidget(_sidePanelStack);
      int sideIndex = splitter->indexOf(_sidePanelStack);
      splitter->setStretchFactor(sideIndex, 0);
      splitter->setCollapsible(sideIndex, false);

      //*****************************************
      //    Git
      //*****************************************

      _gitButton = new QToolButton();
      _gitButton->setToolTip("toggle GIT panel");
      _gitButton->setCheckable(true);
      //      _gitButton->setChecked(false);
      hbox->addWidget(_gitButton, 0);

      connect(_gitButton, &QToolButton::toggled, [this] {
            updateGitHistory();
            bool visible      = _gitButton->isChecked();
            bool wasAIVisible = false;
            if (visible) {
                  wasAIVisible = aiButton->isChecked();
                  const QSignalBlocker blocker(aiButton);
                  aiButton->setChecked(false);
                  _sidePanelStack->setCurrentWidget(gitPanel());
                  }

            int sideIndex = splitter->indexOf(_sidePanelStack);
            if (!visible && !aiButton->isChecked() && this->isVisible())
                  gitWidth = splitter->sizes()[sideIndex];

            bool showStack = visible || aiButton->isChecked();
            _sidePanelStack->setVisible(showStack);

            if (!this->isVisible())
                  return;

            auto sz      = size();
            int newWidth = sz.width();
            if (visible) {
                  QList<int> sizes = splitter->sizes();
                  sizes[sideIndex] = gitWidth;
                  splitter->setSizes(sizes);
                  if (wasAIVisible)
                        newWidth += gitWidth - agentWidth;
                  else
                        newWidth += gitWidth + splitter->handleWidth();
                  }
            else
                  newWidth += -(gitWidth + splitter->handleWidth());
            resize(newWidth, sz.height());
            });

      //*****************************************
      //    Config
      //*****************************************

      configButton = new QToolButton();
      configButton->setObjectName("configButton");
      configButton->setCheckable(true);
      configButton->setToolTip("Configure...");
      connect(configButton, &QToolButton::clicked, [this](bool checked) {
            if (checked)
                  showConfig();
            else
                  hideConfig();
            });
      hbox->addWidget(configButton, 0, Qt::AlignRight);

      vbox->addWidget(splitter, 500);

      initEnterWidget();
      vbox->addWidget(enter, 0);

      //*****************************************
      //    StatusBar
      //*****************************************

      auto sbw = new QWidget();
      grid->addWidget(sbw, 3, 0, 1, 2);

      auto sb = new QHBoxLayout(sbw);

      branchLabel = new QLabel();
      branchLabel->setObjectName("branchLabel");
      branchLabel->setToolTip("git branch");
      branchLabel->setStatusTip("git branch");
      branchLabel->setVisible(_projectMode);
      if (_hasGit) {
            branchLabel->setText(_currentBranchName);
            if (_git.isClean())
                  branchLabel->setStyleSheet("background-color: lightgreen; color: black");
            else
                  branchLabel->setStyleSheet("background-color: lightyellow; color: black");
            }

      sb->addWidget(branchLabel, 0);

      urlLabel = new QLabel();
      urlLabel->setText("url");
      urlLabel->setToolTip("document path");
      sb->addWidget(urlLabel, 2);

      auto spacer = new QWidget();
      sb->addWidget(spacer, 2);

      progressBar = new QProgressBar();
      sb->addWidget(progressBar, 2);
      progressBar->setVisible(false);

      _keyLabel = new QLabel;
      _keyLabel->setToolTip("shortcut sequence");
      sb->addWidget(_keyLabel, 2);

      lineLabel = new QLabel;
      lineLabel->setToolTip("line");
      sb->addWidget(lineLabel, 0);

      colLabel = new QLabel;
      colLabel->setToolTip("column");
      sb->addWidget(colLabel, 0);

      connect(tabBar, &QTabBar::tabBarClicked, [this](int index) { setCurrentKontext(index); });
      connect(tabBar, &QTabBar::tabBarDoubleClicked, [this](int index) {
            if (_kontextList.size() > 1) {
                  tabBar->removeTab(index);
                  removeKontext(index);
                  }
            });
      connect(tabBar, &QTabBar::tabMoved, [this](int from, int to) {
            auto k = _kontextList[from];
            _kontextList.remove(from);
            _kontextList.insert(to, k);
            _currentKontext = to;
            });

      connect(hScroll, SIGNAL(valueChanged(int)), this, SLOT(hScrollTo(int)));
      connect(vScroll, SIGNAL(valueChanged(int)), this, SLOT(vScrollTo(int)));

      completionsPopup = new CompletionsPopup(nullptr);
      completionsPopup->hide();
      connect(completionsPopup, &CompletionsPopup::applyCompletion, [this](int idx) {
            startCmd();
            applyCompletion(idx);
            endCmd();
            });

      //*****************************************
      //    Initialize from .nped
      //*****************************************

      gitPanel();
      agent();
      loadStatus(argc, argv);

            {
            const QSignalBlocker blocker(_projectButton);
            _projectButton->setChecked(_projectVisible);
            _projectPanel->setVisible(_projectVisible);
            if (_projectVisible) {
                  QList<int> sizes                        = splitter->sizes();
                  sizes[splitter->indexOf(_projectPanel)] = projectWidth;
                  splitter->setSizes(sizes);
                  }
            }
            {
            const QSignalBlocker blocker(aiButton);
            aiButton->setChecked(_aiVisible);
            if (_aiVisible) {
                  _sidePanelStack->setVisible(true);
                  _sidePanelStack->setCurrentWidget(_agent);
                  QList<int> sizes                          = splitter->sizes();
                  sizes[splitter->indexOf(_sidePanelStack)] = agentWidth;
                  splitter->setSizes(sizes);
                  }
            }
            {
            const QSignalBlocker blocker(_gitButton);
            _gitButton->setChecked(_gitVisible);
            if (_gitVisible) {
                  _sidePanelStack->setVisible(true);
                  _sidePanelStack->setCurrentWidget(_gitPanel);
                  QList<int> sizes                          = splitter->sizes();
                  sizes[splitter->indexOf(_sidePanelStack)] = gitWidth;
                  splitter->setSizes(sizes);
                  }
            }

      cursorTimer = new QTimer(this);
      connect(cursorTimer, &QTimer::timeout, [this] { unsetCursor(); });
      lsUpdateTimer = new QTimer(this);
      lsUpdateTimer->setSingleShot(true);
      connect(lsUpdateTimer, &QTimer::timeout, [this] { kontext()->file()->updateOutline(); });

      _editWidget->setFocus();
      _editWidget->installEventFilter(kl);
      initFont();
      updateProjectTreeColors();
      updateGitHistory();
      updateProjectPanel();
      connect(this, &Editor::fontFamilyChanged, [this] { initFont(); });
      connect(this, &Editor::darkModeChanged, this, &Editor::updateProjectTreeColors);
      connect(this, &Editor::textStylesLightChanged, this, &Editor::updateProjectTreeColors);
      connect(this, &Editor::textStylesDarkChanged, this, &Editor::updateProjectTreeColors);
      connect(this, &Editor::scaleChanged, [this] { setFontSize(_scale * 14.0); });
      connect(this, &Editor::fontSizeChanged, [this] { initFont(); });
      }

Editor::~Editor() {
      for (auto& ls : languageServers) {
            if (ls.client) {
                  ls.client->stop();
                  delete ls.client;
                  }
            }
      }

//---------------------------------------------------------
//   hideCompletions
//---------------------------------------------------------

void Editor::hideCompletions() {
      completionsPopup->hide();
      }

//---------------------------------------------------------
//   agent
//---------------------------------------------------------

Agent* Editor::agent() {
      if (!_agent) {
            _agent = new Agent(this, box);
            _agent->setMinimumWidth(agentMinimumWidth);
            _agent->setVisible(false);
            _sidePanelStack->addWidget(_agent);
            if (!_projectMode)
                  _aiVisible = false;

            if (_aiVisible) {
                  _agent->setVisible(_aiVisible);
                  const QSignalBlocker blocker(aiButton);
                  aiButton->setChecked(_aiVisible);
                  }
            }
      return _agent;
      }

//---------------------------------------------------------
//   gitPanel
//---------------------------------------------------------

QWidget* Editor::gitPanel() {
      if (!_gitPanel) {
            _gitPanel = new QWidget(box);
            _gitPanel->setObjectName("gitPanel");
            _gitPanel->setMinimumWidth(gitPanelMinimumWidth);
            _gitPanel->setVisible(false);

            _sidePanelStack->addWidget(_gitPanel);

            QVBoxLayout* gitLayout = new QVBoxLayout(_gitPanel);
            gitLayout->setContentsMargins(0, 0, 0, 0);
            gitLayout->setSpacing(0);

            QToolBar* gitToolBar = new QToolBar(_gitPanel);
            gitLayout->addWidget(gitToolBar);

            QAction* diffAction = gitToolBar->addAction("Diff");
            diffAction->setCheckable(true);
            diffAction->setChecked(gitDiff);
            connect(diffAction, &QAction::toggled, [this](bool checked) {
                  gitDiff = checked;
                  if (kontext() && kontext()->file() && kontext()->file()->currentGitHistory() != 0) {
                        showGitVersion(kontext()->file()->currentGitHistory());
                        update();
                        }
                  });

            if (_gitVisible) {
                  _gitPanel->setVisible(_gitVisible);
                  const QSignalBlocker blocker(_gitButton);
                  _gitButton->setChecked(_gitVisible);
                  }
            gitListView = new QListView(_gitPanel);
            gitListView->setModel(&gitList);
            gitListView->setFont(qApp->font());
            gitLayout->addWidget(gitListView);

            updateProjectTreeColors();

            connect(gitListView, &QListView::clicked, [this](const QModelIndex& index) {
                  showGitVersion(index.row());
                  tabBar->modifiedChanged();
                  _editWidget->setFocus();
                  update();
                  });
            }
      return _gitPanel;
      }

//---------------------------------------------------------
//   mdWidget
//---------------------------------------------------------

MarkdownWebView* Editor::mdWidget() {
      if (!_mdWidget) {
            _mdWidget             = new MarkdownWebView(this, this);
            _mdContainer          = new QWidget(this);
            QVBoxLayout* mdLayout = new QVBoxLayout(_mdContainer);
            mdLayout->setContentsMargins(0, 0, 0, 0);
            mdLayout->setSpacing(0);

            QWidget* navBar = new QWidget(_mdContainer);

            QHBoxLayout* navLayout = new QHBoxLayout(navBar);
            navLayout->setContentsMargins(4, 4, 4, 4);
            navLayout->setSpacing(4);

            QToolButton* btnBack = new QToolButton(navBar);
            btnBack->setToolTip("Back");
            connect(btnBack, &QToolButton::clicked, _mdWidget, &QWebEngineView::back);

            QToolButton* btnForward = new QToolButton(navBar);
            btnForward->setToolTip("Forward");

            auto updateButtons = [btnBack, btnForward, this]() {
                  if (!_mdWidget || !_mdWidget->page())
                        return;
                  btnBack->setEnabled(_mdWidget->page()->history()->canGoBack());
                  btnForward->setEnabled(_mdWidget->page()->history()->canGoForward());
                  };
            connect(_mdWidget, &QWebEngineView::loadFinished, this, [updateButtons] { updateButtons(); });
            updateButtons();

            connect(btnForward, &QToolButton::clicked, _mdWidget, &QWebEngineView::forward);

            QToolButton* btnReload = new QToolButton(navBar);
            btnReload->setToolTip("Reload");
            connect(btnReload, &QToolButton::clicked, _mdWidget, &QWebEngineView::reload);

            QToolButton* btnHome = new QToolButton(navBar);
            btnHome->setToolTip("Home");
            connect(btnHome, &QToolButton::clicked, [this]() {
                  if (kontext() && kontext()->file()) {
                        if (kontext()->file()->languageId() == "markdown")
                              _mdWidget->setMarkdown(kontext()->file()->plainText(), kontext()->fileRow());
                        else
                              _mdWidget->setHtml(kontext()->file()->plainText());
                        }
                  });

            connect(this, &Editor::darkModeChanged,
                    [this, btnBack, btnForward, btnReload, btnHome](bool dark) {
                          auto style = textStyle(TextStyle::Normal);
                          QColor fg  = style.fg;
                          btnBack->setIcon(createStatefulIcon(":/images/back.svg", fg, fg, fg));
                          btnForward->setIcon(createStatefulIcon(":/images/forward.svg", fg, fg, fg));
                          btnReload->setIcon(createStatefulIcon(":/images/reload.svg", fg, fg, fg));
                          btnHome->setIcon(createStatefulIcon(":/images/home.svg", fg, fg, fg));
                          _mdWidget->setDarkMode(dark);
                          });
            connect(this, &Editor::textStylesLightChanged, [this, btnBack, btnForward, btnReload, btnHome]() {
                  auto style = textStyle(TextStyle::Normal);
                  QColor fg  = style.fg;
                  btnBack->setIcon(createStatefulIcon(":/images/back.svg", fg, fg, fg));
                  btnForward->setIcon(createStatefulIcon(":/images/forward.svg", fg, fg, fg));
                  btnReload->setIcon(createStatefulIcon(":/images/reload.svg", fg, fg, fg));
                  btnHome->setIcon(createStatefulIcon(":/images/home.svg", fg, fg, fg));
                  });
            connect(this, &Editor::textStylesDarkChanged, [this, btnBack, btnForward, btnReload, btnHome]() {
                  auto style = textStyle(TextStyle::Normal);
                  QColor fg  = style.fg;
                  btnBack->setIcon(createStatefulIcon(":/images/back.svg", fg, fg, fg));
                  btnForward->setIcon(createStatefulIcon(":/images/forward.svg", fg, fg, fg));
                  btnReload->setIcon(createStatefulIcon(":/images/reload.svg", fg, fg, fg));
                  btnHome->setIcon(createStatefulIcon(":/images/home.svg", fg, fg, fg));
                  });
            navLayout->addWidget(btnBack);
            navLayout->addWidget(btnForward);
            navLayout->addWidget(btnReload);
            navLayout->addWidget(btnHome);
            navLayout->addStretch();

            mdLayout->addWidget(navBar, 0);
            mdLayout->addWidget(_mdWidget, 1);

            _stack->addWidget(_mdContainer);

            // Initial style
            auto style = textStyle(TextStyle::Normal);
            QColor fg  = style.fg;
            btnBack->setIcon(createStatefulIcon(":/images/back.svg", fg, fg, fg));
            btnForward->setIcon(createStatefulIcon(":/images/forward.svg", fg, fg, fg));
            btnReload->setIcon(createStatefulIcon(":/images/reload.svg", fg, fg, fg));
            btnHome->setIcon(createStatefulIcon(":/images/home.svg", fg, fg, fg));
            }
      return _mdWidget;
      }

//---------------------------------------------------------
//   initEnterWidget
//---------------------------------------------------------

void Editor::initEnterWidget() {
      enter                   = new QWidget();
      QBoxLayout* enterLayout = new QBoxLayout(QBoxLayout::LeftToRight, enter);
      enterLayout->setContentsMargins(5, 5, 5, 5);
      enterLayout->setSpacing(0);

      QLabel* enterLabel = new QLabel("Enter:", enter);
      enterLayout->addWidget(enterLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
      enterLine = new Completer(enter);

      connect(this, &Editor::fontChanged, [enterLabel, this](QFont f) {
            enterLabel->setFont(f);
            enterLine->setFont(f);
            });

      enterLine->completer()->popup()->setMaximumWidth(250);
      enterLayout->addWidget(enterLine, 50, Qt::AlignVCenter);
      enter->hide();

      std::vector<Action> enterActions = {
         Action(getSC(Cmd::CMD_ENTER), [] {}),

         Action(getSC(Cmd::CMD_ENTER_ADD_FILE),
                [this] {
                      QString fn = enterLine->text();
                      if (!fn.startsWith("/")) {
                            QFileInfo fi(kontext()->file()->fi());
                            fn = fi.absolutePath() + "/" + fn;
                            }
                      addFile(fn);
                      setCurrentKontext(_kontextList.size() - 1);
                      update();
                      }),
         Action(getSC(Cmd::CMD_ENTER_SEARCH),
                [this] {
                      startCmd();
                      search(enterLine->text());
                      endCmd();
                      }),
         Action(getSC(Cmd::CMD_ENTER_CREATE_FUNCTION),
                [this] {
                      startCmd();
                      kontext()->createFunction(enterLine->text());
                      endCmd();
                      }),
         Action(getSC(Cmd::CMD_ENTER_GOTO_LINE),
                [this] {
                      kontext()->gotoLine(enterLine->text());
                      update();
                      }),

            };
      for (auto& a : enterActions) {
            for (const auto& ks : a.seq) {
                  QAction* action = new QAction(this);
                  action->setShortcut(ks);
                  action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
                  connect(action, &QAction::triggered, [this, a] {
                        a.func();
                        //                       enterLine->execute();
                        enterLine->addHistory(enterLine->text());
                        leaveEnter();
                        });
                  enterLine->addAction(action);
                  }
            }
      }

//---------------------------------------------------------
//   getLSclient
//---------------------------------------------------------

LSclient* Editor::getLSclient(const QString& serverName) {
      if (serverName == "none")
            return nullptr;
      for (const auto& ls : languageServers)
            if (ls.name == serverName)
                  return ls.client;

      auto* client = LSclient::createClient(this, serverName.toStdString());
      if (client) {
            languageServers.push_back(LanguageServer(serverName, client));
            //            Debug("new language server started: <{}>", serverName);
            }
      else {
            //            QString ss = serverName;
            //            Debug("could not start language server: <{}>", ss);
            }
      return client;
      }

//---------------------------------------------------------
//   tab
//---------------------------------------------------------

int Editor::tab() const {
      return kontext()->file()->tab();
      }

//---------------------------------------------------------
//   hideCursor
//---------------------------------------------------------

void Editor::hideCursor() {
      setCursor(Qt::BlankCursor);
      cursorTimer->start(1000);
      }

//---------------------------------------------------------
//   addFile
//---------------------------------------------------------

Kontext* Editor::addFile(const QString& path) {
      QFileInfo fi(path);
      if (fi.isDir()) // silently ignore directories
            return 0;

      //---------------------------------------------------
      // Checken, ob das File schon geladen ist:
      //    inode und device vergleichen
      //---------------------------------------------------

      File* file = nullptr;
      if (fi.exists() && fi.isFile()) {
            for (auto f : files) {
                  if (fi == f->fi()) {
                        file = f;
                        break;
                        }
                  }
            }
      if (!file)
            file = createNewFile(fi);
      Kontext* k = new Kontext(this, file);
      addKontext(k);
      return k;
      }

//---------------------------------------------------------
//   lookupKontext
//    like addFile() except a new kontext is _not_ created
//---------------------------------------------------------

Kontext* Editor::lookupKontext(const QString& path) {
      QFileInfo fi(path);
      if (fi.isDir()) // silently ignore directories
            return 0;

      //---------------------------------------------------
      // Checken, ob das File schon geladen ist:
      //    inode und device vergleichen
      //---------------------------------------------------

      File* file = nullptr;
      if (fi.exists() && fi.isFile()) {
            for (auto f : files) {
                  if (fi == f->fi()) {
                        file = f;
                        break;
                        }
                  }
            }
      if (file) {
            // every file should have at least one kontext
            for (auto k : _kontextList)
                  if (k->file() == file)
                        return k;
            Critical("Kontext not found");
            return nullptr;
            }
      file       = createNewFile(fi);
      Kontext* k = new Kontext(this, file);
      addKontext(k);
      return k;
      }

//---------------------------------------------------------
//   setCurrentKontext
//---------------------------------------------------------

void Editor::setCurrentKontext(size_t idx) {
      if (idx >= _kontextList.size()) {
            Critical("====bad current context index {} size {}", idx, _kontextList.size());
            return;
            }
      _currentKontext = idx;
      //      kontext()->setCursorAbs(kontext()->cursor());
      tabBar->setCurrentIndex(_currentKontext);
      urlLabel->setText(kontext()->file()->path());
      setViewMode(kontext()->viewMode());
      //      update();
      updateGitHistory();
      updateCursor();
      updateProjectPanel();
      }

void Editor::setCurrentKontext(Kontext* k) {
      size_t i = 0;
      for (i = 0; i < _kontextList.size(); ++i)
            if (_kontextList[i] == k)
                  break;
      setCurrentKontext(i);
      }

//---------------------------------------------------------
//   addKontext
//---------------------------------------------------------

void Editor::addKontext(Kontext* k, int index) {
      connectKontext(k);
      QFileInfo fi(k->file()->path());
      index = tabBar->insertTab(index, fi.fileName());
      tabBar->setTabData(index, QVariant::fromValue<Kontext*>(k));
      _kontextList.insert(index, k);
      tabBar->modifiedChanged();
      }

//---------------------------------------------------------
//   updateCursor
//---------------------------------------------------------

void Editor::updateCursor() {
      auto p = kontext()->cursor().filePos;
      lineLabel->setText(QString(" %1 ").arg(p.row + 1));
      colLabel->setText(QString(" %1 ").arg(p.col + 1));
      updateVScrollbar();
      updateHScrollbar();
      }

//---------------------------------------------------------
//   removeKontext
//---------------------------------------------------------

void Editor::removeKontext(int idx) {
      Kontext* kontextToErase = _kontextList[idx];
      _kontextList.erase(idx);

      if (_kontextList.empty()) {
            // Sollte nicht passieren – mindestens 1 Tab muss offen bleiben
            delete kontextToErase;
            return;
            }
      if (_currentKontext >= _kontextList.size())
            _currentKontext = _kontextList.size() - 1;

      if (kontextToErase->file()->dereference()) {
            kontextToErase->file()->save(); // save unconditionally, do not ask
            std::erase(files, kontextToErase->file());
            delete kontextToErase->file();
            }
      delete kontextToErase;
      }

//---------------------------------------------------------
//   updateVScrollbar
//---------------------------------------------------------

void Editor::updateVScrollbar() {
      int visible = editWidget()->visibleSize().height();
      int total   = kontext()->rows() - 2;
      int pos     = kontext()->fileRow() - kontext()->screenRow();
      vScroll->blockSignals(true);
      if (total < 0)
            total = 0;
      if (total < visible)
            visible = total;
      vScroll->setValue(pos);
      vScroll->setRange(0, total - 1);
      vScroll->blockSignals(false);
      }

//---------------------------------------------------------
//   updateHScrollbar
//---------------------------------------------------------

void Editor::updateHScrollbar() {
      int visible = editWidget()->visibleSize().width();
      int total   = kontext()->maxLineLength();
      int pos     = kontext()->fileCol() - kontext()->screenCol();
      hScroll->blockSignals(true);
      if (total < 0)
            total = 0;
      if (total <= visible) {
            hScroll->hide();
            }
      else {
            hScroll->show();
            hScroll->setRange(0, total - visible + 1);
            hScroll->setValue(pos);
            hScroll->setPageStep(visible);
            }
      hScroll->blockSignals(false);
      }

//---------------------------------------------------------
//   hScrollTo
//---------------------------------------------------------

void Editor::hScrollTo(int xpos) {
      startCmd();
      int col                       = std::max(0, kontext()->fileCol() - xpos);
      kontext()->cursor().screenPos = Pos(col, kontext()->screenRow());
      update();
      endCmd();
      }

//---------------------------------------------------------
//   vScrollTo
//---------------------------------------------------------

void Editor::vScrollTo(int ypos) {
      startCmd();
      kontext()->cursor().screenPos = Pos(kontext()->screenCol(), kontext()->fileRow() - ypos);
      int cursorY                   = kontext()->fileRow();
      int maxCursorY = std::min(ypos + editWidget()->visibleSize().height(), int(kontext()->rows()));
      //     Debug("cursorY {} maxCursorY {} ypos {}", cursorY, maxCursorY, ypos);
      if (cursorY < (ypos + 1))
            kontext()->cursor().filePos = Pos(kontext()->fileCol(), std::min(maxCursorY, ypos + 1));
      else if (cursorY >= maxCursorY)
            kontext()->moveCursorAbs(kontext()->fileCol(), maxCursorY - 1);
      update();
      endCmd();
      }

//---------------------------------------------------------
//   formatting
//---------------------------------------------------------

void Editor::formatting() {
      if (lclient())
            lclient()->formattingRequest(kontext());
      }

//---------------------------------------------------------
//   startCmd
//    start an editor command
//---------------------------------------------------------

void Editor::startCmd() {
      hideCursor();
      kontext()->file()->undo()->beginMacro();
      }

//---------------------------------------------------------
//   update
//---------------------------------------------------------

void Editor::update(QRect r) {
      _editWidget->update(r);
      }

//---------------------------------------------------------
//   update
//---------------------------------------------------------

void Editor::update() {
      if (_editWidget)
            _editWidget->update();
      }

//---------------------------------------------------------
//   endCmd
//    end an editor command
//---------------------------------------------------------

void Editor::endCmd() {
      updateVScrollbar();
      kontext()->file()->undo()->endMacro();
      }

//---------------------------------------------------------
//   quitCmd
//---------------------------------------------------------

void Editor::quitCmd() {
      saveStatus();
      if (_agent)
            agent()->session()->save();
      close();
      }

//---------------------------------------------------------
//   saveAll
//---------------------------------------------------------

void Editor::saveAll() {
      // weird code: f->save() might expand macros and therefore needs a valid undo() stack
      kontext()->file()->undo()->endMacro();
      for (auto f : files) {
            f->undo()->beginMacro();
            f->save();
            f->undo()->endMacro();
            }
      kontext()->file()->undo()->beginMacro();
      }

//---------------------------------------------------------
//   saveQuitCmd
//---------------------------------------------------------

void Editor::saveQuitCmd() {
      saveAll();
      quitCmd();
      }

//---------------------------------------------------------
//   updateProjectTreeColors
//---------------------------------------------------------

void Editor::updateProjectTreeColors() {
      auto style  = textStyle(TextStyle::Normal);
      QColor fg   = style.fg;
      QString css = QString("background-color: %1; color: %2;").arg(style.bg.name(), fg.name());
      if (_projectTreeView)
            _projectTreeView->setStyleSheet(css);
      if (_projectComboBox)
            _projectComboBox->setStyleSheet(css);
      if (gitListView)
            gitListView->setStyleSheet(css);
      if (_gitButton)
            _gitButton->setIcon(createStatefulIcon(":/images/Git-Icon-1788C.svg", fg, fg, fg));
      if (configButton)
            configButton->setIcon(createStatefulIcon(":/images/configure.svg", fg, fg, fg));
      }

//---------------------------------------------------------
//   saveStatus
//    save the local status in current directory/.nped.json
//---------------------------------------------------------

void Editor::saveStatus() {
      if (!persistent)
            return;
      json j;
      j["version"] = "1.0";
      j["width"]   = width();
      j["height"]  = height();

      //      j["darkMode"] = darkMode();

      QByteArray geom = saveGeometry();
      j["geometry"]   = geom.toHex().toStdString();

      QByteArray state = saveState();
      j["state"]       = state.toHex().toStdString();

      QByteArray splitterState = splitter->saveState();
      j["splitterState"]       = splitterState.toHex().toStdString();

      j["aiVisible"]      = _agent && _agent->isVisible();
      j["agentRole"]      = agentRoleName().toStdString();
      j["gitVisible"]     = _gitButton->isChecked();
      j["projectVisible"] = _projectButton->isChecked();
      //      j["aiModel"]       = agent()->currentModel().toStdString();

      //      j["aiExecuteMode"] = agent()->isExecuteMode();
      j["search"]  = searchPattern.pattern().toStdString();
      j["replace"] = replace.toStdString();

      if (_gitButton && _gitButton->isChecked() && _sidePanelStack->isVisible())
            gitWidth = splitter->sizes()[splitter->indexOf(_sidePanelStack)];
      if (_projectPanel && _projectPanel->isVisible())
            projectWidth = splitter->sizes()[splitter->indexOf(_projectPanel)];
      if (aiButton && aiButton->isChecked() && _sidePanelStack->isVisible())
            agentWidth = splitter->sizes()[splitter->indexOf(_sidePanelStack)];

      j["gitWidth"]     = gitWidth;
      j["projectWidth"] = projectWidth;
      j["agentWidth"]   = agentWidth;

      json kontexte = json::array();
      for (const auto& k : _kontextList) {
            json jk;
            jk["file"]     = k->file()->path().toStdString();
            jk["x"]        = k->fileCol();
            jk["y"]        = k->fileRow();
            jk["xoff"]     = k->fileCol() - k->screenCol();
            jk["yoff"]     = k->fileRow() - k->screenRow();
            jk["viewMode"] = int(k->viewMode());
            kontexte.push_back(jk);
            }
      j["kontexte"]       = kontexte;
      j["currentKontext"] = int(_currentKontext);
      j["fontSize"]       = _fontSize;
      j["scale"]          = _scale;

      std::ofstream fs(".nped.json");
      fs << j.dump(4);
      }

//---------------------------------------------------------
//   loadStatus
//    return true if status file (".nped") was found
//---------------------------------------------------------

bool Editor::loadStatus(int argc, char** argv) {
      // do not load history if there are filenames on the command line
      bool loadFiles = (argc == 0);

      auto screen           = qApp->primaryScreen();
      int h                 = screen->geometry().height();
      int w                 = (h * 8) / 10;
      bool geometryRestored = false;

      int idx = 0;

      std::ifstream fs(".nped.json");
      if (fs.is_open()) {
            try {
                  json j;
                  fs >> j;

                  if (j.contains("width"))
                        w = j["width"].get<int>();
                  if (j.contains("height"))
                        h = j["height"].get<int>();
                  if (j.contains("search"))
                        searchPattern.setPattern(QString::fromStdString(j["search"].get<std::string>()));
                  if (j.contains("replace"))
                        replace = QString::fromStdString(j["replace"].get<std::string>());
                  if (j.contains("currentKontext") && loadFiles)
                        idx = j["currentKontext"].get<int>();
                  if (j.contains("gitVisible"))
                        _gitVisible = j["gitVisible"].get<bool>();
                  if (j.contains("projectVisible"))
                        _projectVisible = j["projectVisible"].get<bool>();

                  if (j.contains("aiVisible"))
                        _aiVisible = j["aiVisible"].get<bool>();
                  if (j.contains("agentRole"))
                        setAgentRoleName(QString::fromStdString(j["agentRole"]));

                  if (j.contains("gitWidth"))
                        gitWidth = j["gitWidth"].get<int>();
                  if (j.contains("projectWidth"))
                        projectWidth = j["projectWidth"].get<int>();
                  if (j.contains("agentWidth"))
                        agentWidth = j["agentWidth"].get<int>();

                  if (j.contains("geometry")) {
                        QByteArray geom =
                            QByteArray::fromHex(QByteArray::fromStdString(j["geometry"].get<std::string>()));
                        geometryRestored = true;
                        restoreGeometry(geom);
                        }

                  if (j.contains("state")) {
                        QByteArray state =
                            QByteArray::fromHex(QByteArray::fromStdString(j["state"].get<std::string>()));
                        restoreState(state);
                        }

                  if (j.contains("splitterState")) {
                        QByteArray sState = QByteArray::fromHex(
                            QByteArray::fromStdString(j["splitterState"].get<std::string>()));
                        splitter->restoreState(sState);
                        }
                  if (j.contains("scale"))
                        setScale(j["scale"].get<qreal>());
                  if (j.contains("fontSize"))
                        setFontSize(j["fontSize"].get<qreal>());

                  // panels already created before loadStatus

                  if (loadFiles && j.contains("kontexte") && j["kontexte"].is_array()) {
                        for (const auto& jk : j["kontexte"]) {
                              if (!jk.contains("file"))
                                    continue;

                              QString path = QString::fromStdString(jk["file"].get<std::string>());
                              Kontext* k   = addFileInitial(path);

                              if (!k) {
                                    Critical("no file");
                                    }
                              else {
                                    int x  = jk.contains("x") ? jk["x"].get<int>() : 0;
                                    int y  = jk.contains("y") ? jk["y"].get<int>() : 0;
                                    int xo = jk.contains("xoff") ? jk["xoff"].get<int>() : 0;
                                    int yo = jk.contains("yoff") ? jk["yoff"].get<int>() : 0;
                                    int vm = jk.contains("viewMode") ? jk["viewMode"].get<int>()
                                                                     : int(ViewMode::File);

                                    k->blockSignals(true);
                                    y  = std::clamp(y, 0, k->rows() - 1);
                                    yo = std::clamp(yo, 0, y);
                                    k->setScreenPos(Pos(x - xo, y - yo));
                                    k->setFilePos(Pos(x, y));
                                    k->blockSignals(false);
                                    k->setViewMode(ViewMode(vm));
                                    }
                              }
                        }
                  }
            catch (const json::parse_error& e) {
                  Critical("Error parsing .nped.json: {}", e.what());
                  }
            catch (const json::type_error& e) {
                  Critical("Type error reading .nped/project.json: {}", e.what());
                  }
            }

      if (!geometryRestored)
            setGeometry(0, 0, w, h);
      for (int i = 0; i < argc; ++i)
            addFileInitial(argv[i]);

      if (_kontextList.empty()) {
            Critical("no file to edit");
            exit(0);
            }
      setCurrentKontext(std::clamp(idx, 0, int(_kontextList.size() - 1)));
      return true;
      }

//---------------------------------------------------------
//   createNewFile
//---------------------------------------------------------

File* Editor::createNewFile(const QFileInfo& fi) {
      File* file = new File(this, fi);
      connect(file, &File::modifiedChanged, [this]() { tabBar->modifiedChanged(); });
      tabBar->modifiedChanged();
      connect(file, &File::fileChanged, [this] { lsUpdateTimer->start(800); });
      file->load();
      files.push_back(file);
      return file;
      }

//---------------------------------------------------------
//   addFileInitial
//    EditWin() size is unknown at this point and we cannot
//    adjust cursorpositions
//---------------------------------------------------------

Kontext* Editor::addFileInitial(const QString& path) {
      QFileInfo fi(path);

      //---------------------------------------------------
      // Checken, ob das File schon geladen ist:
      //    inode und device vergleichen
      //---------------------------------------------------

      File* file = nullptr;
      if (fi.exists() && fi.isFile()) {
            for (auto f : files) {
                  if (fi == f->fi()) {
                        file = f;
                        break;
                        }
                  }
            }
      if (!file)
            file = createNewFile(fi);
      Kontext* k = new Kontext(this, file);
      connectKontext(k);
      _kontextList.push_back(k);
      int index = tabBar->addTab(fi.fileName());
      tabBar->setTabData(index, QVariant::fromValue<Kontext*>(k));
      tabBar->modifiedChanged();
      return k;
      }

//---------------------------------------------------------
//   connectKontext
//---------------------------------------------------------

void Editor::connectKontext(Kontext* k) {
      connect(k, &Kontext::cursorChanged, this, &Editor::updateCursor);
      connect(k->file(), &File::cursorChanged, [this, k](const Cursor cursor) {
            // this signal is send from undo/redo and should only update
            // the cursor of the current kontext
            if (kontext() == k)
                  k->setCursorAbs(cursor);
            });
      }

//---------------------------------------------------------
//   enterCmd
//---------------------------------------------------------

void Editor::enterCmd() {
      if (kontext()->file()->clearSearchMarks()) {
            update();
            // return;
            }
      if (endSelectionMode())
            return;
      if (completionsPopup->isVisible()) {
            hideCompletions();
            return;
            }
      QDir dir(kontext()->file()->fi().absolutePath());
      dir.setFilter(QDir::Files);
      enterLine->setSuggestions(dir.entryList());
      enterActive = true;
      enterLine->setText("");
      enter->show();
      enterLine->setFocus();
      }

//---------------------------------------------------------
//   leaveEnter
//---------------------------------------------------------

void Editor::leaveEnter() {
      enter->hide();
      enterActive = false;
      //      for (auto& a : _pedActions)
      //            a.sc->setEnabled(true);
      editWidget()->setFocus();
      }

//---------------------------------------------------------
//   nextKontext
//---------------------------------------------------------

void Editor::nextKontext() {
      kontext()->file()->undo()->endMacro();
      setCurrentKontext((_currentKontext + 1) % _kontextList.size());
      kontext()->file()->undo()->beginMacro();
      _editWidget->setFocus();
      }

//---------------------------------------------------------
//   preKontext
//---------------------------------------------------------

void Editor::prevKontext() {
      kontext()->file()->undo()->endMacro();
      int idx = _currentKontext - 1;
      if (idx < 0)
            idx = _kontextList.size() - 1;
      setCurrentKontext(idx);
      kontext()->file()->undo()->beginMacro();
      _editWidget->setFocus();
      }

//---------------------------------------------------------
//   advance
//---------------------------------------------------------

static void advance(Pos* p, const QString& s) {
      for (const auto& c : s)
            if (c == '\n') {
                  p->col  = 0;
                  p->row += 1;
                  }
            else
                  p->col += 1;
      }

//---------------------------------------------------------
//   input
//---------------------------------------------------------

void Editor::input(const QString& s) {
      if (endSelectionMode())
            return;
      if (kontext()->readOnly())
            return;
      Cursor cursor = kontext()->cursor();
      QString ss;
      QString cl = kontext()->currentLine();
      Cursor ac  = cursor;
      if (s == "\n") {
            if (cursor.filePos.col > cl.size())
                  cursor.filePos.col = cl.size();
            File* f    = kontext()->file();
            int indent = f->indent(QPoint(0, cursor.fileRow() + 1));
            ss         = "\n";

            // If the '\n' splits the line we have to insert the indent
            // as spaces otherwise simply moving the cursor is sufficient.
            int col = cursor.fileCol();
            if (col < cl.size())
                  ss += QString(indent, QChar(' '));
            ac.filePos.col  = indent;
            ac.filePos.row += 1;
            if (ac.screenPos.row < (editWidget()->rows() - 1))
                  ac.screenPos.row += 1;
            ac.screenPos.col = ac.filePos.col;
            }
      else {
            if (cl.size() < cursor.fileCol()) {
                  // fillup line with spaces upto cursor.column
                  int n              = ac.fileCol() - cl.size();
                  ss                 = QString(n, QChar(' '));
                  cursor.filePos.col = cl.size();
                  }
            ss += s;
            advance(&(ac.filePos), s);
            ac.screenPos.col = ac.filePos.col;
            }

      undoPatch(cursor.filePos, 0, ss, ac, kontext()->cursor());
      }

//---------------------------------------------------------
//   rubout
//---------------------------------------------------------

void Editor::rubout() {
      if (endSelectionMode())
            return;
      const auto cursor = kontext()->cursor();
      if (cursor.fileCol() == 0 && cursor.fileRow() == 0)
            return;
      if (cursor.fileCol() == 0) {
            // const auto& p = kontext()->text();
            auto s1 = kontext()->line(cursor.fileRow() - 1);
            Pos pt(s1.size(), cursor.fileRow() - 1);
            Cursor ac         = cursor;
            ac.filePos        = pt;
            ac.screenPos.row -= 1;
            ac.screenPos.col  = pt.col;
            undoPatch(pt, 1, "", ac, cursor);
            }
      else {
            Pos p(cursor.fileCol() - 1, cursor.fileRow());
            auto s = kontext()->currentLine();
            if (cursor.fileCol() > s.size())
                  kontext()->moveCursorRel(-1, 0);
            else
                  undoPatch(p, 1, "", Cursor(p, Pos(cursor.screenCol() - 1, cursor.screenRow())), cursor);
            }
      }

//---------------------------------------------------------
//   deleteChar
//    remove character at cursor position
//---------------------------------------------------------

void Editor::deleteChar() {
      if (endSelectionMode())
            return;
      if (!kontext()->atLineEnd()) {
            auto cursor = kontext()->cursor();
            undoPatch(cursor.filePos, 1, "", cursor, cursor);
            }
      }

//---------------------------------------------------------
//   deleteLine
//---------------------------------------------------------

void Editor::deleteLine() {
      const auto& cursor = kontext()->cursor();
      Selection& s       = kontext()->selection();
      pickText.mode      = s.mode;
      switch (s.mode) {
            case SelectionMode::NoSelect: {
                  pickText.text = kontext()->currentLine() + "\n";
                  Pos p         = cursor.filePos;
                  p.col         = 0;
                  undoPatch(p, pickText.text.size(), "", cursor, cursor);
                  pickText.mode = SelectionMode::RowSelect;
                  } break;
            case SelectionMode::RowSelect:
                  pickText.text = kontext()->selectionText();
                  undoPatch({0, kontext()->selection().y()}, pickText.text.size(), "", cursor, cursor);
                  break;
            case SelectionMode::ColSelect: {
                  pickText.text = kontext()->selectionText();
                  Pos pt        = kontext()->selection().start;
                  //                  kontext()->setCursor(pt);
                  QStringList sl = pickText.text.split('\n');
                  sl.pop_back();
                  int y          = pt.row;
                  auto file      = kontext()->file();
                  auto upatch    = new Patch(file, cursor, cursor);
                  const Lines& t = file->fileText();
                  for (const auto& s : sl) {
                        int n = std::min(int(s.size()), std::max(0, t[y].size() - pt.col));
                        upatch->add(PatchItem({pt.col, y}, n, ""));
                        ++y;
                        }
                  file->undo()->push(upatch);
                  } break;
            case SelectionMode::CharSelect:
                  pickText.text = kontext()->selectionText();
                  undoPatch(cursor.filePos, pickText.text.size(), "", cursor, cursor);
                  break;
            }
      QClipboard* cb = QApplication::clipboard();
      cb->setText(pickText.text, QClipboard::Clipboard);
      endSelectionMode();
      }

//---------------------------------------------------------
//   deleteRestOfLine
//---------------------------------------------------------

void Editor::deleteRestOfLine() {
      if (endSelectionMode())
            return;
      auto cursor       = kontext()->cursor();
      QString rightPart = kontext()->line(cursor.fileRow()).mid(cursor.fileCol());
      undoPatch(cursor.filePos, rightPart.size(), "", cursor, cursor);
      }

//---------------------------------------------------------
//   insertTab
//---------------------------------------------------------

void Editor::insertTab() {
      if (endSelectionMode())
            return;
      auto cursor = kontext()->cursor();
      if (kontext()->atLineEnd()) {
            // move cursor to next tab position
            kontext()->moveCursorRel((cursor.fileCol() / tab() + 1) * tab() - cursor.fileCol(), 0);
            return;
            }
      //
      // insert spaces
      //
      QString insert {" "};
      int x = cursor.fileCol();
      for (int i = 1; i < tab(); ++i) {
            if (((x + i) % tab()) == 0)
                  break;
            insert += " ";
            }
      Cursor ac         = cursor;
      ac.filePos.col   += insert.size();
      ac.screenPos.col += insert.size();
      kontext()->fixScreenCursor(ac);
      undoPatch(cursor.filePos, 0, insert, ac, cursor);
      }

//---------------------------------------------------------
//   pick
//---------------------------------------------------------

void Editor::pick() {
      pickText.clear();
      pickText.mode = kontext()->selection().mode;
      switch (kontext()->selection().mode) {
            case SelectionMode::NoSelect:
                  pickText.text += kontext()->currentLine() + "\n";
                  pickText.mode  = SelectionMode::RowSelect;
                  break;
            case SelectionMode::RowSelect:
            case SelectionMode::ColSelect:
            case SelectionMode::CharSelect: pickText.text = kontext()->selectionText(); break;
            }
      QClipboard* cb = QApplication::clipboard();
      cb->setText(pickText.text, QClipboard::Clipboard);
      endSelectionMode();
      }

//---------------------------------------------------------
//   setPickText
//---------------------------------------------------------

void Editor::setPickText(const QString& text, SelectionMode mode) {
      pickText.text = text;
      pickText.mode = mode;
      }

//---------------------------------------------------------
//   put
//---------------------------------------------------------

void Editor::put() {
      if (pickText.isEmpty()) {
            Debug("empty");
            return;
            }
      auto cursor = kontext()->cursor();
      switch (pickText.mode) {
            case SelectionMode::CharSelect: {
                  // if cursor column is behind current text then we must extend
                  // the text line with spaces upto the cursor position

                  QString s;                // real text to insert
                  Pos pos = cursor.filePos; // real insert position
                  int n   = kontext()->fileCol() - kontext()->columns();
                  if (n > 0) {
                        s        = QString(n, QChar(' '));
                        pos.col -= n;
                        }
                  s += pickText.text;
                  undoPatch(pos, 0, s, cursor, cursor);
                  } break;
            case SelectionMode::RowSelect: {
                  undoPatch(Pos(0, cursor.fileRow()), 0, pickText.text, cursor, cursor);
                  break;
                  }
            case SelectionMode::ColSelect: {
                  QStringList sl = pickText.text.split('\n');
                  File* file     = kontext()->file();
                  int y          = cursor.fileRow();
                  int x          = cursor.fileCol();
                  for (const auto& s : sl) {
                        int fill;
                        int n = x;
                        if (y >= file->fileRows()) // at text end?
                              fill = x;
                        else {
                              n = kontext()->line(y).size();
                              if (n < x)
                                    fill = x - n;
                              else
                                    fill = 0;
                              }
                        QString is = QString(fill, QChar(' ')) + s;
                        if ((y < file->fileRows()) && (x >= file->fileLine(y).size())) { // TODO: check
                              // remove trailing spaces
                              int n = is.size();
                              while (n && is[n - 1].isSpace())
                                    n--;
                              is = is.left(n);
                              }
                        undoPatch(Pos(std::min(n, x), y++), 0, is, cursor, cursor);
                        }
                  } break;
            case SelectionMode::NoSelect: break;
            }
      }

//---------------------------------------------------------
//   rowSelect
//---------------------------------------------------------

void Editor::rowSelect() {
      if (endSelectionMode())
            return;
      kontext()->setSelectionMode(SelectionMode::RowSelect);
      Pos pos          = kontext()->filePos();
      QRect selectRect = {pos.col, pos.row, 1, 1};
      kontext()->setSelection(selectRect);
      update();
      }

//---------------------------------------------------------
//   colSelect
//---------------------------------------------------------

void Editor::colSelect() {
      if (endSelectionMode())
            return;
      kontext()->setSelectionMode(SelectionMode::ColSelect);
      Pos pos          = kontext()->filePos();
      QRect selectRect = {pos.col, pos.row, 1, 1};
      kontext()->setSelection(selectRect);
      update();
      }

//---------------------------------------------------------
//   charSelect
//---------------------------------------------------------

void Editor::charSelect() {
      if (endSelectionMode())
            return;
      kontext()->setSelectionMode(SelectionMode::CharSelect);
      Pos pos          = kontext()->filePos();
      QRect selectRect = {pos.col, pos.row, 1, 1};
      kontext()->setSelection(selectRect);
      update();
      }

//---------------------------------------------------------
//   endSelectionMode
//    if in SelectionMode: end mode and return true
//    else return false
//---------------------------------------------------------

bool Editor::endSelectionMode() {
      if (kontext()->selection().mode != SelectionMode::NoSelect) {
            kontext()->setSelectionMode(SelectionMode::NoSelect);
            kontext()->setCursorAbs(kontext()->selectionStartCursor());
            return true;
            }
      return false;
      }

//---------------------------------------------------------
//   isWordchar
//---------------------------------------------------------

static bool isWordchar(QChar c) {
      return c.isLetterOrNumber() || c == '_';
      }

//---------------------------------------------------------
//   pickWord
//---------------------------------------------------------

QString Editor::pickWord() {
      QString d;
      if (endSelectionMode())
            return d;
      QString s = kontext()->currentLine();
      if (s.isEmpty())
            return d;

      int i = kontext()->fileCol();
      if (i >= s.size())
            return d;
      int ei = s.size();
      while (i && isWordchar(s[i]))
            --i;
      if (!isWordchar(s[i]))
            ++i;
      while (i < ei && isWordchar(s[i]))
            d.push_back(s[i++]);
      return d;
      }

//---------------------------------------------------------
//   pickNextWord
//---------------------------------------------------------

QString Editor::pickNextWord() {
      QString d;
      if (endSelectionMode())
            return d;
      QString s = kontext()->currentLine();
      if (s.isEmpty())
            return d;

      int i = kontext()->fileCol();
      if (i >= s.size())
            return d;

      int ei = s.size();
      if (s[i] == ' ') {
            while (i < ei && s[i] == ' ')
                  d.push_back(s[i++]);
            }
      else {
            while (i < ei && s[i] != ' ')
                  d.push_back(s[i++]);
            while (i < ei && s[i] == ' ')
                  d.push_back(s[i++]);
            }
      return d;
      }

//---------------------------------------------------------
//   deleteNextWord
//---------------------------------------------------------

void Editor::deleteNextWord() {
      if (endSelectionMode())
            return;
      QString d = pickNextWord();
      if (d.isEmpty())
            return;
      auto cursor = kontext()->cursor();
      undoPatch(cursor.filePos, d.size(), "", cursor, cursor);
      }

//---------------------------------------------------------
//   getNextWord
//---------------------------------------------------------

void Editor::getNextWord() {
      if (endSelectionMode())
            return;
      QString d = pickWord();
      if (d.isEmpty())
            return;
      enterActive = true;
      enterLine->setText(d);
      enter->show();
      enterLine->setFocus();
      //      for (auto& a : _pedActions)
      //            a.sc->setEnabled(false);
      }

//---------------------------------------------------------
//   cBrackets
//---------------------------------------------------------

void Editor::cBrackets() {
      const auto& s = kontext()->currentLine();
      //      Debug("<{}>", s);
      int level = 0;
      for (const auto& c : s)
            if (c == '(')
                  ++level;
            else if (c == ')')
                  --level;
      msg("() Balance {}", level);
      }

//---------------------------------------------------------
//   cLevel
//---------------------------------------------------------

void Editor::cLevel() {
      msg("{} Balance {}", "{}", kontext()->file()->indent(kontext()->filePos()) / kontext()->file()->tab());
      }

//---------------------------------------------------------
//   findFile
//---------------------------------------------------------

File* Editor::findFile(QString path) {
      for (const auto f : files)
            if (f->path() == path)
                  return f;
      return nullptr;
      }

//---------------------------------------------------------
//   readJson
//---------------------------------------------------------

json Editor::readJson(const QString& path) {
      QString fullPath = projectRoot() + "/" + path;
      std::ifstream ifs(fullPath.toStdString());
      if (!ifs.is_open())
            throw std::runtime_error("cannot open file: " + fullPath.toStdString());
      json j;
      ifs >> j;
      return j;
      }

//---------------------------------------------------------
//   undoPatch
//    p2 is position after undo
//    p1 is position after redo
//---------------------------------------------------------

void Editor::undoPatch(const Pos& pos, int len, const QString& s, const Cursor& p1, const Cursor& p2) {
      auto file = kontext()->file();
      file->undo()->push(new Patch(file, pos, len, s, p1, p2));
      //      updateTimer->start();
      }

//---------------------------------------------------------
//   lclient
//---------------------------------------------------------

LSclient* Editor::lclient() {
      return _kontextList.empty() ? nullptr : kontext()->file()->languageClient();
      }

//---------------------------------------------------------
//   gotoDefinition
//---------------------------------------------------------

void Editor::gotoDefinition() {
      auto f = kontext()->file();
      auto c = f->languageClient();
      if (c)
            c->gotoDefinition(f, kontext()->filePos());
      }

//---------------------------------------------------------
//   gotoTypeDefinition
//---------------------------------------------------------

void Editor::gotoTypeDefinition() {
      auto f = kontext()->file();
      auto c = f->languageClient();
      if (c)
            c->gotoTypeDefinition(f, kontext()->filePos());
      }

//---------------------------------------------------------
//   gotoImplementation
//---------------------------------------------------------

void Editor::gotoImplementation() {
      auto f = kontext()->file();
      auto c = f->languageClient();
      if (c)
            c->gotoImplementation(f, kontext()->filePos());
      }

//---------------------------------------------------------
//   gotoKontext
//---------------------------------------------------------

void Editor::gotoKontext(const QString& path, const Pos& pos) {
      HistoryRecord r {kontext(), kontext()->filePos(), kontext()->screenPos()};
      Kontext* k = nullptr;
      File* f    = findFile(path);
      if (!f) {
            k = addFile(path);
            if (!k)
                  return;
            }
      if (!k) {
            for (auto kk : _kontextList) {
                  if (kk->file() == f) {
                        k = kk;
                        break;
                        }
                  }
            }

      if (!k)
            return;
      if (k != kontext())
            setCurrentKontext(k);
      k->moveCursorAbs(pos.col, pos.row);
      history.push_back(r);
      update();
      }

//---------------------------------------------------------
//   backKontext
//---------------------------------------------------------

void Editor::backKontext() {
      if (history.empty())
            return;
      endCmd();
      auto h = history.back();
      history.pop_back();
      h.kontext->screenPos() = h.screenPosition;
      h.kontext->setCursorAbs(h.kontext->cursor());
      setCurrentKontext(h.kontext);
      startCmd();
      update();
      }

//---------------------------------------------------------
//   hover
//---------------------------------------------------------

void Editor::hover() {
      HoverKontext k(kontext()->file(), kontext()->filePos());
      auto c = kontext()->file()->languageClient();

      if (k == hoverKontext) {
            aiButton->toggle();
            }
      else {
            hoverKontext = k;
            if (c)
                  c->hover(hoverKontext.file, hoverKontext.cursor);
            }
      }

//---------------------------------------------------------
//   setInfoText
//---------------------------------------------------------

void Editor::setInfoText(const QString&) {
      //      infoPanel->setMarkdown(s);
      //      infoButton->setChecked(true); // show info panel
      }

//---------------------------------------------------------
//   clearInfoText
//---------------------------------------------------------

void Editor::clearInfoText() {
      //      infoPanel->setMarkdown("");
      }

//---------------------------------------------------------
//   copyKontext
//---------------------------------------------------------

void Editor::copyKontext() {
      auto k      = new Kontext(this, kontext()->file());
      k->cursor() = kontext()->cursor();
      addKontext(k, _currentKontext);
      setCurrentKontext(k);
      update();
      }

//---------------------------------------------------------
//   showProgress
//---------------------------------------------------------

void Editor::showProgress(bool v) {
      progressBar->reset();
      progressBar->setRange(0, 100);
      progressBar->setVisible(v);
      }

//---------------------------------------------------------
//   showProgress
//---------------------------------------------------------

void Editor::showProgress(double v) {
      progressBar->setValue(int(v));
      }

//---------------------------------------------------------
//   foldAll
//---------------------------------------------------------

void Editor::foldAll() {
      kontext()->file()->foldAll(true);
      }

//---------------------------------------------------------
//   unfoldAll
//---------------------------------------------------------

void Editor::unfoldAll() {
      kontext()->file()->foldAll(false);
      }

//---------------------------------------------------------
//   foldToggel
//---------------------------------------------------------

void Editor::foldToggle() {
      int row = kontext()->fileRow();
      kontext()->file()->toggleFold(row);
      }

//---------------------------------------------------------
//   modifiedChanged
//---------------------------------------------------------

void TabBar::modifiedChanged() {
      for (int i = 0; i < count(); ++i) {
            Kontext* kontext = tabData(i).value<Kontext*>();
            if (!kontext)
                  Fatal("no kontext for tab {}", i);
            QColor color;
            bool readOnly = kontext->readOnly();
            TextStyle ts  = kontext->editor->textStyle(TextStyle::Normal);
            bool modified = kontext->file()->modified();
            color         = readOnly ? QColor("#b0c0ff") : (modified ? QColor("#ff4040") : ts.fg);
            setTabTextColor(i, color);
            }
      }

//-----------------------------------------------------------------------------
//   setViewMode
//    The main editor window is stacked:
//          index 0:    editWidget, used for all view modes except WebView
//          index 1:    mdWidget, used for view mode WebView
//          index 2:    configWebView, used for the configuration page
//    switch to the right widget
//    call kontext->setViewMode  to setup the right content
//-----------------------------------------------------------------------------

void Editor::setViewMode(ViewMode viewMode) {
      switch (viewMode) {
            case ViewMode::Annotations:
            case ViewMode::Outline:
            case ViewMode::SearchResults:
            case ViewMode::File:
                  _stack->setCurrentWidget(_editWidget);
                  _editWidget->setFocus();
                  vScroll->setVisible(true);
                  break;

            case ViewMode::GitVersion:
                  _stack->setCurrentWidget(_editWidget);
                  _editWidget->setFocus();
                  vScroll->setVisible(true);
                  break;

            case ViewMode::GitDiff: {
                  mdWidget()->setFocus();
                  const auto& text = kontext()->file()->gitVersion();
                  mdWidget()->showGitDiff(text.join('\n'));
                  vScroll->setVisible(false); // mdWidget has its own scroll bar
                  _stack->setCurrentWidget(_mdContainer);
                  } break;

            case ViewMode::WebView: {
                  mdWidget()->setFocus();
                  auto lid         = kontext()->file()->languageId();
                  const auto& text = kontext()->file()->plainText();
                  if (lid == "markdown")
                        mdWidget()->setMarkdown(text, kontext()->fileRow());
                  else if (lid == "html")
                        mdWidget()->setHtml(text);
                  else if (lid == "image")
                        mdWidget()->load(QUrl::fromLocalFile(kontext()->file()->path()));
                  _stack->setCurrentWidget(_mdContainer);
                  vScroll->setVisible(false); // mdWidget has its own scroll bar
                  } break;
            }
      kontext()->setViewMode(viewMode);
      tabBar->modifiedChanged(); // update readOnly marking
      }

//---------------------------------------------------------
//   toggleViewMode
//---------------------------------------------------------

void Editor::toggleViewMode() {
      const auto& l = kontext()->file()->languageId();

      switch (kontext()->viewMode()) {
            case ViewMode::File:
                  if (l == "cpp" || l == "c")
                        if (kontext()->file()->currentGitHistory() == 0)
                              setViewMode(ViewMode::Outline);
                        else
                              showGitVersion(kontext()->file()->currentGitHistory());
                  else if (l == "markdown" || l == "html" || l == "image")
                        setViewMode(ViewMode::WebView);
                  break;
            case ViewMode::Outline:
                  //
                  setViewMode(ViewMode::File);
                  break;
            case ViewMode::Annotations:
                  //
                  setViewMode(ViewMode::File);
                  break;
            case ViewMode::GitVersion:
                  //
                  setViewMode(ViewMode::File);
                  break;
            case ViewMode::GitDiff:
                  //
                  setViewMode(ViewMode::File);
                  break;
            case ViewMode::SearchResults:
                  //
                  setViewMode(ViewMode::File);
                  break;
            case ViewMode::WebView:
                  //
                  setViewMode(ViewMode::File);
                  break;
            }
      }

//---------------------------------------------------------
//   showGitVersion
//---------------------------------------------------------

void Editor::showGitVersion(int row) {
      kontext()->file()->setCurrentGitHistory(row);
      if (row == 0) { // this is the HEAD version
            setViewMode(ViewMode::File);
            }
      else {
            auto history = kontext()->file()->gitHistory(row);
            auto oid     = history->oid;
            Lines lines;
            if (gitDiff) {
                  lines = git()->getDiff(kontext()->file()->fileText(), &oid);
                  kontext()->file()->setGitVersion(lines);
                  setViewMode(ViewMode::GitDiff);
                  }
            else {
                  lines = git()->getFile(&oid);
                  kontext()->file()->setGitVersion(lines);
                  setViewMode(ViewMode::GitVersion);
                  }
            }
      }

//---------------------------------------------------------
//   updateProjectPanel
//---------------------------------------------------------

void Editor::updateProjectPanel() {
      if (!_projectButton || !_projectButton->isChecked() || !_projectPanel)
            return;

      QString rootPath = projectRoot();

      if (_projectComboBox) {
            bool blocked = _projectComboBox->blockSignals(true);
            _projectComboBox->clear();
            for (const auto& p : _projects)
                  _projectComboBox->addItem(p);
            if (!_projects.contains(rootPath))
                  _projectComboBox->addItem(rootPath);
            _projectComboBox->setCurrentText(rootPath);
            _projectComboBox->blockSignals(blocked);
            }

      // Set the root path if not set correctly yet
      if (_projectModel->rootPath() != rootPath) {
            QModelIndex rootIndex = _projectModel->setRootPath(rootPath);
            _projectTreeView->setRootIndex(_projectProxyModel->mapFromSource(rootIndex));
            }

      Kontext* k = kontext();
      if (k && k->file()) {
            QString currentPath = k->file()->path();
            // Expand and select current file
            QModelIndex currentIndex = _projectModel->index(currentPath);
            if (currentIndex.isValid()) {
                  QModelIndex proxyIndex = _projectProxyModel->mapFromSource(currentIndex);
                  _projectTreeView->setCurrentIndex(proxyIndex);
                  _projectTreeView->scrollTo(proxyIndex);
                  }
            }
      }

//---------------------------------------------------------
//   switchProject
//---------------------------------------------------------

void Editor::switchProject(const QString& path) {
      if (_projectRoot == path)
            return;
      _projectRoot = path;
      _git.init();
      _hasGit = _git.isInitialized();
      if (_hasGit)
            _currentBranchName = _git.getCurrentBranch();
      updateProjectPanel();
      updateGitHistory();
      if (!_projects.contains(path))
            _projects.push_front(path);
      }

//---------------------------------------------------------
//   initFont
//---------------------------------------------------------

void Editor::initFont() {
      _font = QFont(_fontFamily);
      _font.setFixedPitch(true);
      _font.setPointSizeF(_fontSize);

      //      _font.setWeight(QFont::Weight(fontWeight));
      QFontMetricsF fm(_font, _editWidget);
      _fw = fm.horizontalAdvance(QChar('x'));
      _fh = fm.lineSpacing();
      _fa = fm.ascent();
      _fd = fm.descent();

      // scale system font
      auto f = qApp->font();
      f.setPointSizeF(_fontSize);

      tabBar->setFont(f);
      aiButton->setFont(f);
      _projectButton->setFont(f);
      _gitButton->setFont(f);
      branchLabel->setFont(f);
      urlLabel->setFont(f);
      lineLabel->setFont(f);
      colLabel->setFont(f);
      _keyLabel->setFont(f);
      completionsPopup->setListFont(f);
      _projectTreeView->setFont(f);
      if (_projectComboBox) {
            // _projectComboBox->setFont(f);
            _projectTreeView->setFont(_font);
            }
      if (_mdWidget)
            _mdWidget->setFont(f);
      if (gitListView)
            gitListView->setFont(f);
      qApp->setFont(f);
      emit fontChanged(f);
      }
