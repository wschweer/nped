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
#include <QMainWindow>
#include <QBoxLayout>
#include <QClipboard>
#include <QCompleter>
#include <QDir>
#include <QFile>
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

#include <nlohmann/json.hpp>
using json = nlohmann::json;

extern bool persistent;

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
      emit keyLabelChanged(QKeySequence(keys[0], keys[1], keys[2], keys[3]).toString(QKeySequence::NativeText));
      return QObject::eventFilter(obj, event);
      }

//---------------------------------------------------------
//   getSC
//---------------------------------------------------------

const ShortcutConfig& Editor::getSC(Cmd cmd) {
      for (const auto& sc : _shortcuts)
            if (sc.cmd == cmd)
                  return sc;
      Fatal("no command <{}>", int(cmd));
      };

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
      qRegisterMetaType<ShortcutConfig>("ShortcutConfig");
      qRegisterMetaType<FileTypeConfig>("FileTypeConfig");
      qRegisterMetaType<LanguageServerConfig>("LanguageServerConfig");
      qRegisterMetaType<Model>("Model");

      fileWatcher = new QFileSystemWatcher(this);
#if 0 // experimental
      connect(fileWatcher, &QFileSystemWatcher::fileChanged, [](const QString& path) {
            // QFileSystemWatcher emits fileChanged for both modification and deletion
            File* f = findFile(path);
            if (f) {
                  QFileInfo fi(path);
                  if (fi.exists())
                        Critical("file <{}> changed on disk", path);
                  else
                        Critical("file <{}> deleted from disk", path);
                        }
                  });
#endif

      if (!initProject())
            Critical("init project failed");
      loadDefaults();
      loadProjectStatus();
      loadSettings();

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
         Action(getSC(Cmd::CMD_LINE_END), [this] { kontext()->moveCursorAbs(kontext()->currentLine().size(), -1); }),
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
         Action(getSC(Cmd::CMD_FORMAT), [this] { formatting(); }),
         Action(getSC(Cmd::CMD_VIEW_FUNCTIONS),
                [this] {
                      kontext()->toggleViewMode();
                      updateViewMode();
                      }),
         Action(getSC(Cmd::CMD_VIEW_BUGS), [this] { kontext()->setViewMode(ViewMode::Bugs); }),
         Action(getSC(Cmd::CMD_SEARCH_NEXT), [this] { searchNext(); }),
         Action(getSC(Cmd::CMD_SEARCH_PREV), [this] { searchPrev(); }),
         Action(getSC(Cmd::CMD_DELETE_WORD), [this] { deleteNextWord(); }),
         Action(getSC(Cmd::CMD_ENTER_WORD), [this] { getNextWord(); }),
         Action(getSC(Cmd::CMD_GOTO_TYPE_DEFINITION), [this] { gotoTypeDefinition(); }),
         Action(getSC(Cmd::CMD_GOTO_IMPLEMENTATION), [this] { gotoImplementation(); }),
         Action(getSC(Cmd::CMD_GOTO_DEFINITION), [this] { gotoDefinition(); }),
         Action(getSC(Cmd::CMD_GOTO_BACK), [this] { backKontext(); }),
         Action(getSC(Cmd::CMD_SHOW_INFO), [this] { hover(); }),
         Action(getSC(Cmd::CMD_EXPAND_MACROS), [this] { kontext()->file()->expandMacros(); }),
         Action(getSC(Cmd::CMD_COMPLETIONS), [this] { requestCompletions(); }),
         Action(getSC(Cmd::CMD_FUNCTION_HEADER), [this] { kontext()->createFunctionHeader(); }),
         Action(getSC(Cmd::CMD_GIT_TOGGLE), [this] { _gitButton->setChecked(!_gitButton->isChecked()); }),
         Action(getSC(Cmd::CMD_FOLD_ALL), [this] { foldAll(); }),
         Action(getSC(Cmd::CMD_UNFOLD_ALL), [this] { unfoldAll(); }),
         Action(getSC(Cmd::CMD_FOLD_TOGGLE), [this] { foldToggle(); }),
         //             Action(getSC(Cmd::CMD_SEARCH_LIST, [this] { kontext()->setViewMode(ViewMode::SearchResults); }),
         Action(getSC(Cmd::CMD_RENAME), [this] { rename(); }),
            };

      KeyLogger* kl = new KeyLogger(&_pedActions, this);
      connect(kl, &KeyLogger::triggered, [this](Action* a) {
            startCmd();
            a->func();
            endCmd();
            keyLabel()->setText("");
            });
      connect(kl, &KeyLogger::keyLabelChanged, [this](QString s) { _keyLabel->setText(s); });

      QWidget* box = new QWidget;
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
      //    AiButton
      //*****************************************

      infoButton = new QToolButton();
      infoButton->setText("Ai");
      infoButton->setCheckable(true);
      infoButton->setChecked(false);
      infoButton->setToolTip("toggle AI panel");
      hbox->addWidget(infoButton, 0, Qt::AlignRight);
      connect(infoButton, &QToolButton::toggled, [this] { agent->setVisible(infoButton->isChecked()); });

      _stack      = new QStackedWidget(this);
      hScroll     = new QScrollBar(Qt::Horizontal);
      vScroll     = new QScrollBar(Qt::Vertical);
      _editWidget = new EditWidget(nullptr, this);
      _mdWidget   = new MarkdownWebView(this, this);
      _mdWidget->setZoomFactor(1.5);

      _stack->addWidget(_editWidget);
      _stack->addWidget(_mdWidget);

      connect(this, &Editor::darkModeChanged, [this](bool dark) {
            markerDefinitions.setDarkMode(dark);
            updateStyle();
            update();
            });
      // Initial style
      markerDefinitions.setDarkMode(darkMode());
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
      splitter->addWidget(w);

      agent = new Agent(this, box);
      agent->setMinimumWidth(500);
      splitter->addWidget(agent);
      splitter->setCollapsible(splitter->indexOf(agent), false);

      //*****************************************
      //    Git
      //*****************************************

      _gitButton = new QToolButton();
      _gitButton->setIcon(QIcon(":images/Git-Icon-1788C.svg"));
      _gitButton->setToolTip("toggle GIT panel");
      _gitButton->setCheckable(true);
      _gitButton->setChecked(false);
      hbox->addWidget(_gitButton, 0);

      connect(_gitButton, &QToolButton::toggled, [this] {
            updateGitHistory();
            gitPanel->setVisible(_gitButton->isChecked());
            });

      //*****************************************
      //    Config
      //*****************************************

      configButton = new QToolButton();
      configButton->setObjectName("configButton");
      QString iconPath = darkMode() ? ":/images/configure_white.svg" : ":/images/configure.svg";
      configButton->setIcon(QIcon(iconPath));
      configButton->setToolTip("Configure...");
      connect(this, &Editor::darkModeChanged, [this]() {
            QString iconPath = darkMode() ? ":/images/configure_white.svg" : ":/images/configure.svg";
            configButton->setIcon(QIcon(iconPath));
            });
      connect(configButton, &QToolButton::clicked, [this] {
            ConfigDialogWrapper dialog(this, this);
            if (dialog.exec() == QDialog::Accepted)
                  initFont();
            else
                  Debug("FAIL");
            });
      hbox->addWidget(configButton, 0, Qt::AlignRight);

      gitPanel = new QListView(box);
      gitPanel->setObjectName("gitPanel");
      gitPanel->setMinimumWidth(500);
      gitPanel->setVisible(false);
      gitPanel->setModel(&gitList);
      connect(gitPanel, &QListView::clicked, [this](const QModelIndex& index) {
            kontext()->file()->showGitVersion(index.row());
            tabBar->modifiedChanged();
            _editWidget->setFocus();
            update();
            });
      splitter->addWidget(gitPanel);
      splitter->setCollapsible(splitter->indexOf(gitPanel), false);

      vbox->addWidget(splitter, 500);

      initEnterWidget();
      vbox->addWidget(enter, 0);

      //*****************************************
      //    StatusBar
      //*****************************************

      auto sbw = new QWidget();
      grid->addWidget(sbw, 3, 0, 1, 2);

      auto sb = new QHBoxLayout(sbw);

      urlLabel = new QLabel();
      urlLabel->setText("url");
      urlLabel->setToolTip("url current document");
      sb->addWidget(urlLabel, 2);

      auto spacer = new QWidget();
      sb->addWidget(spacer, 2);

      progressBar = new QProgressBar();
      sb->addWidget(progressBar, 2);
      progressBar->setVisible(false);

      _keyLabel = new QLabel;
      _keyLabel->setToolTip("current shortcut sequence");
      sb->addWidget(_keyLabel, 2);

      lineLabel = new QLabel;
      lineLabel->setToolTip("current line");
      sb->addWidget(lineLabel, 0);

      colLabel = new QLabel;
      colLabel->setToolTip("current column");
      sb->addWidget(colLabel, 0);

      connect(tabBar, &QTabBar::tabBarClicked, [this](int index) { setCurrentKontext(index); });
      connect(tabBar, &QTabBar::tabBarDoubleClicked, [this](int index) {
            if (_kontextList.size() > 1) {
                  tabBar->removeTab(index);
                  removeKontext(index);
                  }
            });
      connect(tabBar, &QTabBar::tabMoved, [this](int from, int to) {
            _currentKontext = from;
            auto k          = _kontextList[to];
            _kontextList.remove(to);
            _kontextList.insert(from, k);
            });

      connect(hScroll, SIGNAL(valueChanged(int)), this, SLOT(hScrollTo(int)));
      connect(vScroll, SIGNAL(valueChanged(int)), this, SLOT(vScrollTo(int)));

      completionsPopup = new CompletionsPopup(nullptr);
      completionsPopup->hide();

      //*****************************************
      //    Initialize from .nped
      //*****************************************

      loadStatus(argc, argv);

      cursorTimer = new QTimer(this);
      connect(cursorTimer, &QTimer::timeout, [this] { unsetCursor(); });
      lsUpdateTimer = new QTimer(this);
      lsUpdateTimer->setSingleShot(true);
      connect(lsUpdateTimer, &QTimer::timeout, [this] { kontext()->file()->updateAST(); });

      _editWidget->setFocus();
      _editWidget->installEventFilter(kl);
      initFont();
      _git.init();
      updateGitHistory();

      connect(this, &Editor::fgColorChanged, [this] { update(); });
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
//   initEnterWidget
//---------------------------------------------------------

void Editor::initEnterWidget() {
      enter                   = new QWidget();
      QBoxLayout* enterLayout = new QBoxLayout(QBoxLayout::LeftToRight, enter);
      enterLayout->setContentsMargins(0, 0, 0, 0);
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

//-----------------------------------------------------------------------------
//   updateViewMode
//    The main editor window is stacked:
//          index 0:    editWidget, used for all view modes except WebView
//          index 1:    mdWidget, used for view mode WebView
//-----------------------------------------------------------------------------

void Editor::updateViewMode() {
      auto viewMode = kontext()->viewMode();
      switch (viewMode) {
            default:
            case ViewMode::File:
                  if (_stack->currentIndex() != 0)
                        _stack->setCurrentIndex(0);
                  _editWidget->setFocus();
                  break;

            case ViewMode::WebView: {
                  if (_stack->currentIndex() != 1)
                        _stack->setCurrentIndex(1);
                  _mdWidget->setFocus();
                  auto lid = kontext()->file()->languageId();
                  if (lid == "markdown")
                        _mdWidget->setMarkdown(kontext()->file()->plainText());
                  else if (lid == "html")
                        _mdWidget->setHtml(kontext()->file()->plainText());
                  } break;

                  //            case ViewMode::Functions:
                  //                kontext()->setViewMode(ViewMode::Functions);
                  //                  break;
            }
      }

//---------------------------------------------------------
//   toggleViewMode
//---------------------------------------------------------

void Kontext::toggleViewMode() {
      const auto& l = file()->languageId();
      if (l == "cpp" || l == "c") {
#if 0
            switch (_viewMode) {
                  default:
                  case ViewMode::Functions:
                        _viewMode = ViewMode::File;
                        break;
                  case ViewMode::File:
                        _viewMode = ViewMode::Functions;
                        break;
                                                      }
#endif
            setViewMode(ViewMode::Functions);
            }
      else if (l == "markdown" || l == "html") {
            switch (_viewMode) {
                  default:
                  case ViewMode::File: _viewMode = ViewMode::WebView; break;
                  case ViewMode::WebView: _viewMode = ViewMode::File; break;
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
            Debug("new language server started: <{}>", serverName);
            }
      else
            Debug("could not start language server: <{}>", serverName);
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
      kontext()->setCursorAbs(kontext()->cursor());
      tabBar->setCurrentIndex(_currentKontext);
      urlLabel->setText(kontext()->file()->path());
      update();
      updateGitHistory();
      updateCursor();
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
      tabBar->setTabData(index, QVariant::fromValue<File*>(k->file()));
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
      if (_currentKontext >= _kontextList.size() - 1)
            _currentKontext -= 1;
      setCurrentKontext(_currentKontext);
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
      int total   = kontext()->file()->rows() - 2;
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
      int total   = kontext()->file()->maxLineLength();
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
      kontext()->cursor().screenPos = Pos(kontext()->fileCol() - xpos, kontext()->screenRow());
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
      int maxCursorY                = std::min(ypos + editWidget()->visibleSize().height(), int(kontext()->file()->rows()));
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
      File* f = kontext()->file();
      if (f->mustUpdateLS())
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
      saveProjectStatus();
      saveStatus();
      agent->saveStatus();
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
      saveStatus();
      close();
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
      infoButton->setFont(f);
      _gitButton->setFont(f);
      urlLabel->setFont(f);
      lineLabel->setFont(f);
      colLabel->setFont(f);
      _keyLabel->setFont(f);
      qApp->setFont(f);
      emit fontChanged(f);
      }

//---------------------------------------------------------
//   setFontSize
//---------------------------------------------------------

void Editor::setFontSize(qreal s) {
      _fontSize = s;
      initFont();
      }

//---------------------------------------------------------
//   saveStatus
//---------------------------------------------------------

void Editor::saveStatus() {
      if (!persistent)
            return;
      json j;
      j["version"] = "1.0";
      j["width"]   = width();
      j["height"]  = height();

      QByteArray geom = saveGeometry();
      j["geometry"]   = geom.toHex().toStdString();

      QByteArray state = saveState();
      j["state"]       = state.toHex().toStdString();

      j["fontSize"]   = fontSize();
      j["fontFamily"] = _fontFamily.toStdString();

      QByteArray splitterState = splitter->saveState();
      j["splitter"]            = splitterState.toHex().toStdString();

      j["aiVisible"]     = agent->isVisible();
      j["aiModel"]       = agent->currentModel().toStdString();
      j["aiExecuteMode"] = agent->isExecuteMode();
      j["search"]        = searchPattern.pattern().toStdString();
      j["replace"]       = replace.toStdString();
      j["gitPanel"]      = _gitButton->isChecked();

      json kontexte = json::array();
      for (const auto k : _kontextList) {
            json jk;
            jk["file"] = k->file()->path().toStdString();
            jk["x"]    = k->fileCol();
            jk["y"]    = k->fileRow();
            jk["xoff"] = k->fileCol() - k->screenCol();
            jk["yoff"] = k->fileRow() - k->screenRow();
            kontexte.push_back(jk);
            }
      j["kontexte"]       = kontexte;
      j["currentKontext"] = int(_currentKontext);

      std::ofstream fs(".nped.json");
      fs << j.dump(4);
      agent->saveStatus();
      }

//---------------------------------------------------------
//   saveProjectStatus
//---------------------------------------------------------

void Editor::saveProjectStatus() {
      if (!persistent)
            return;
      json j;
      j["version"] = "1.0";
      j["aiModel"] = agent->currentModel().toStdString();

      std::ofstream fs(projectRoot().toStdString() + "/.nped/project.json");
      fs << j.dump(4);
      agent->saveStatus();
      }

//---------------------------------------------------------
//   loadProjectStatus
//---------------------------------------------------------

void Editor::loadProjectStatus() {
      std::ifstream fs(projectRoot().toStdString() + "/.nped/project.json");
      if (fs.is_open()) {
            try {
                  json j;
                  fs >> j;

                  if (j.contains("aiModel"))
                        _settingsLLModel = QString::fromStdString(j["aiModel"].get<std::string>());
                  }
            catch (const json::parse_error& e) {
                  Critical("Error parsing ./.nped/project.json: {}", e.what());
                  }
            catch (const json::type_error& e) {
                  Critical("Type error reading .nped/project.json: {}", e.what());
                  }
            }
      }

//---------------------------------------------------------
//   loadStatus
//    return true if status file (".nped") was found
//---------------------------------------------------------

bool Editor::loadStatus(int argc, char** argv) {
      loadSettings(); // TODO: inline here
      // do not load history if there are filenames on the command line
      bool loadFiles = (argc == 0);

      auto screen        = qApp->primaryScreen();
      int h              = screen->geometry().height();
      int w              = (h * 8) / 10;
      bool stateRestored = false;

      std::ifstream fs(".nped.json");
      if (fs.is_open()) {
            try {
                  json j;
                  fs >> j;

                  if (j.contains("width"))
                        w = j["width"].get<int>();
                  if (j.contains("height"))
                        h = j["height"].get<int>();
                  if (j.contains("fontSize"))
                        setFontSize(j["fontSize"].get<double>());
                  if (j.contains("fontFamily"))
                        _fontFamily = QString::fromStdString(j["fontFamily"].get<std::string>());
                  if (j.contains("search"))
                        searchPattern.setPattern(QString::fromStdString(j["search"].get<std::string>()));
                  if (j.contains("replace"))
                        replace = QString::fromStdString(j["replace"].get<std::string>());
                  if (j.contains("currentKontext") && loadFiles)
                        _currentKontext = j["currentKontext"].get<int>();

                  if (j.contains("splitter")) {
                        QByteArray splitterState = QByteArray::fromHex(QByteArray::fromStdString(j["splitter"].get<std::string>()));
                        splitter->restoreState(splitterState);
                        }

                  if (j.contains("aiVisible")) {
                        bool v = j["aiVisible"].get<bool>();
                        agent->setVisible(v);
                        infoButton->setChecked(v);
                        }

                  if (j.contains("aiExecuteMode")) {
                        bool v = j["aiExecuteMode"].get<bool>();
                        agent->setExecuteMode(v);
                        }

                  if (j.contains("aiModel"))
                        _settingsLLModel = QString::fromStdString(j["aiModel"].get<std::string>());

                  if (j.contains("state")) {
                        QByteArray state = QByteArray::fromHex(QByteArray::fromStdString(j["state"].get<std::string>()));
                        restoreState(state);
                        stateRestored = true;
                        }

                  if (j.contains("geometry")) {
                        QByteArray geom = QByteArray::fromHex(QByteArray::fromStdString(j["geometry"].get<std::string>()));
                        restoreGeometry(geom);
                        }

                  if (j.contains("gitPanel")) {
                        bool val = j["gitPanel"].get<bool>();
                        _gitButton->setChecked(val);
                        }

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

                                    k->blockSignals(true);
                                    y  = std::clamp(y, 0, k->file()->rows() - 1);
                                    yo = std::clamp(yo, 0, y);
                                    k->setScreenPos(Pos(x - xo, y - yo));
                                    k->setFilePos(Pos(x, y));
                                    k->blockSignals(false);
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

      if (!stateRestored)
            setGeometry(0, 0, w, h);
      for (int i = 0; i < argc; ++i)
            addFileInitial(argv[i]);

      if (_kontextList.empty()) {
            Critical("no file to edit");
            exit(0);
            }
      tabBar->setCurrentIndex(_currentKontext);
      urlLabel->setText(kontext()->file()->path());
      update();
      updateCursor();
      return true;
      }

//---------------------------------------------------------
//   createNewFile
//---------------------------------------------------------

File* Editor::createNewFile(const QFileInfo& fi) {
      File* file = new File(this, fi);
      connect(file, &File::modifiedChanged, [this]() { tabBar->modifiedChanged(); });
      tabBar->modifiedChanged();
      if (file->mustUpdateLS())
            connect(file, &File::fileChanged, [this] { lsUpdateTimer->start(400); });
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
      int index = tabBar->addTab(fi.fileName());
      tabBar->setTabData(index, QVariant::fromValue<File*>(k->file()));
      _kontextList.push_back(k);
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
            completionsPopup->hide();
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
      updateViewMode();
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
      updateViewMode();
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
      if (kontext()->file()->readOnly())
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
            auto s1 = kontext()->file()->line(cursor.fileRow() - 1);
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
      switch (kontext()->selectionMode()) {
            case SelectionMode::NoSelect: {
                  pickText     = kontext()->currentLine() + "\n";
                  pickTextRows = true;
                  Pos p        = cursor.filePos;
                  p.col        = 0;
                  undoPatch(p, pickText.size(), "", cursor, cursor);
                  } break;
            case SelectionMode::RowSelect:
                  pickText     = kontext()->selectionText();
                  pickTextRows = true;
                  undoPatch({0, kontext()->selection().y()}, pickText.size(), "", cursor, cursor);
                  break;
            case SelectionMode::ColSelect: {
                  pickText     = kontext()->selectionText();
                  pickTextRows = false;
                  QPoint pt    = kontext()->selection().topLeft();
                  //                  kontext()->setCursor(pt);
                  QStringList sl = pickText.split('\n');
                  sl.pop_back();
                  int y          = pt.y();
                  auto file      = kontext()->file();
                  auto upatch    = new Patch(file, cursor, cursor);
                  const Lines& t = file->fileText();
                  for (const auto& s : sl) {
                        int n = std::min(int(s.size()), std::max(0, t[y].size() - pt.x()));
                        upatch->add(PatchItem({pt.x(), y}, n, ""));
                        ++y;
                        }
                  file->undo()->push(upatch);
                  } break;
            }
      QClipboard* cb = QApplication::clipboard();
      cb->setText(pickText, QClipboard::Clipboard);
      endSelectionMode();
      }

//---------------------------------------------------------
//   deleteRestOfLine
//---------------------------------------------------------

void Editor::deleteRestOfLine() {
      if (endSelectionMode())
            return;
      auto cursor       = kontext()->cursor();
      File* file        = kontext()->file();
      QString rightPart = file->line(cursor.fileRow()).mid(cursor.fileCol());
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
      QString insert{" "};
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
      switch (kontext()->selectionMode()) {
            case SelectionMode::NoSelect:
                  pickText     += kontext()->currentLine() + "\n";
                  pickTextRows  = true;
                  break;
            case SelectionMode::RowSelect:
                  pickText     = kontext()->selectionText();
                  pickTextRows = true;
                  break;
            case SelectionMode::ColSelect:
                  pickText     = kontext()->selectionText();
                  pickTextRows = false;
                  break;
            }
      QClipboard* cb = QApplication::clipboard();
      cb->setText(pickText, QClipboard::Clipboard);
      endSelectionMode();
      }

//---------------------------------------------------------
//   put
//---------------------------------------------------------

void Editor::put() {
      if (pickText.isEmpty())
            return;
      auto cursor = kontext()->cursor();
      if (pickTextRows) {
            undoPatch(Pos(0, cursor.fileRow()), 0, pickText, cursor, cursor);
            }
      else {
            QStringList sl = pickText.split('\n');
            File* file     = kontext()->file();
            int y          = cursor.fileRow();
            int x          = cursor.fileCol();
            for (const auto& s : sl) {
                  int fill;
                  int n = x;
                  if (y >= file->rows()) // at text end?
                        fill = x;
                  else {
                        n = file->line(y).size();
                        if (n < x)
                              fill = x - n;
                        else
                              fill = 0;
                        }
                  QString is = QString(fill, QChar(' ')) + s;
                  if ((y < file->rows()) && (x >= file->line(y).size())) { // TODO: check
                        // remove trailing spaces
                        int n = is.size();
                        while (n && is[n - 1].isSpace())
                              n--;
                        is = is.left(n);
                        }
                  undoPatch(Pos(std::min(n, x), y++), 0, is, cursor, cursor);
                  }
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
//   endSelectionMode
//    if in SelectionMode: end mode and return true
//    else return false
//---------------------------------------------------------

bool Editor::endSelectionMode() {
      if (kontext()->selectionMode() != SelectionMode::NoSelect) {
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
      Debug("===={}", std::this_thread::get_id());
      const auto& s = kontext()->currentLine();
      Debug("<{}>", s);
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
      return kontext()->file()->languageClient();
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
      HistoryRecord r{kontext(), kontext()->filePos(), kontext()->screenPos()};
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
            infoButton->toggle();
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
