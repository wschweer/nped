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

#pragma once

#include <QWidget>
#include <QFrame>
#include <QMainWindow>
#include <QFont>
#include <QColor>
#include <QFlags>
#include <QTabBar>
#include <QRegularExpression>
#include <QStatusBar>
#include <QResizeEvent>
#include <QDialog>
#include <QListView>
#include <QTextEdit>
#include <QQuickWidget>
// #include <QFileSystemWatcher>
#include <vector>
#include "file.h"
#include "completer.h"
#include "git.h"
#include "agent.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class LSclient;
class File;
class EditWidget;
class Kontext;
class QLabel;
class QScrollBar;
class QStackedWidget;
class QGridLayout;
class QMouseEvent;
class HistoryLineEdit;
class QShortcut;
class QTimer;
class HistoryLineEdit;
class QSplitter;
class QToolButton;
class QListWidget;
class CompletionsPopup;
class Completion;
class QProgressBar;
class Agent;
class MarkdownWebView;
class ScreenshotHelper;

using Completions = std::vector<Completion>;

//---------------------------------------------------------
//   Cmd
//    list of editor commands
//---------------------------------------------------------

enum class Cmd {
      CMD_QUIT,
      CMD_SAVE_QUIT,
      CMD_CHAR_RIGHT,
      CMD_CHAR_LEFT,
      CMD_LINE_UP,
      CMD_LINE_DOWN,
      CMD_LINE_START,
      CMD_LINE_END,
      CMD_LINE_TOP,
      CMD_LINE_BOTTOM,
      CMD_PAGE_UP,
      CMD_PAGE_DOWN,
      CMD_FILE_BEGIN,
      CMD_FILE_END,
      CMD_WORD_LEFT,
      CMD_WORD_RIGHT,
      CMD_ENTER,
      CMD_SAVE,
      CMD_KONTEXT_COPY,
      CMD_KONTEXT_PREV,
      CMD_KONTEXT_NEXT,
      CMD_SHOW_BRACKETS,
      CMD_SHOW_LEVEL,
      CMD_KONTEXT_UP,
      CMD_KONTEXT_DOWN,
      CMD_DELETE_LINE,
      CMD_DELETE_LINE_RIGHT,
      CMD_UNDO,
      CMD_REDO,
      CMD_RUBOUT,
      CMD_CHAR_DELETE,
      CMD_INSERT_LINE,
      CMD_TAB,
      CMD_SELECT_ROW,
      CMD_SELECT_COL,
      CMD_SEARCH_NEXT,
      CMD_RENAME,
      CMD_PICK,
      CMD_PUT,
      CMD_SEARCH_PREV,
      CMD_DELETE_WORD,
      CMD_ENTER_WORD,
      CMD_GOTO_BACK,
      CMD_SHOW_INFO,
      CMD_FORMAT,
      CMD_VIEW_FUNCTIONS,
      CMD_VIEW_BUGS,
      CMD_GOTO_TYPE_DEFINITION,
      CMD_GOTO_IMPLEMENTATION,
      CMD_GOTO_DEFINITION,
      CMD_EXPAND_MACROS,
      CMD_COMPLETIONS,
      CMD_FOLD_ALL,
      CMD_UNFOLD_ALL,
      CMD_FOLD_TOGGLE,
      CMD_FUNCTION_HEADER,
      CMD_GIT_TOGGLE,
      CMD_ENTER_ADD_FILE,
      CMD_ENTER_SEARCH,
      CMD_ENTER_CREATE_FUNCTION,
      CMD_ENTER_GOTO_LINE,
      CMD_SCREENSHOT,
      // Web-Navigation:
      CMD_LINK_BACK,
      CMD_LINK_FORWARD
      };

//---------------------------------------------------------
//   ShortcutConfig
//---------------------------------------------------------

struct ShortcutConfig {
      Q_GADGET

      Q_ENUM(Cmd)
      Q_PROPERTY(Cmd cmd MEMBER cmd)
      Q_PROPERTY(QString id MEMBER id)
      Q_PROPERTY(QString description MEMBER description)
      Q_PROPERTY(QString sequence MEMBER sequence)

    public:
      Cmd cmd;
      QString id;
      QString description;
      QString sequence;
      bool operator==(const ShortcutConfig& other) const = default;
      };

//---------------------------------------------------------
//   FileTypeConfig
//---------------------------------------------------------

struct FileTypeConfig {
      Q_GADGET
      Q_PROPERTY(QString extensions MEMBER extensions)
      Q_PROPERTY(QString languageId MEMBER languageId)
      Q_PROPERTY(QString languageServer MEMBER languageServer)
      Q_PROPERTY(int tabSize MEMBER tabSize)
      Q_PROPERTY(bool parse MEMBER parse)

    public:
      QString extensions; // Regex oder *.cpp;*.h
      QString languageId;
      QString languageServer;
      int tabSize                                        = 4;
      bool parse                                         = false;
      bool operator==(const FileTypeConfig& other) const = default;
      };

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

Q_DECLARE_METATYPE(ShortcutConfig)
Q_DECLARE_METATYPE(FileTypeConfig)
Q_DECLARE_METATYPE(LanguageServerConfig)

//---------------------------------------------------------
//   Match
//---------------------------------------------------------

struct Match {
      int line;
      int col1;
      int col2;
      };

//---------------------------------------------------------
//   TabBar
//---------------------------------------------------------

class TabBar : public QTabBar
      {
      Q_OBJECT
      void tabInserted(int idx) override {
            QTabBar::tabInserted(idx);
            updateGeometry(); // without this horizontal size is not reliable set
            }

    public:
      TabBar(QWidget* parent = nullptr) : QTabBar(parent) { setMovable(true); }
      void modifiedChanged() {
            for (int i = 0; i < count(); ++i) {
                  File* f = tabData(i).value<File*>();
                  setTabTextColor(i, f->readOnly() ? QColor("blue") : (f->modified() ? QColor("red") : QColor("black")));
                  }
            }
      };

//---------------------------------------------------------
//   LanguageServer
//---------------------------------------------------------

struct LanguageServer {
      QString name;
      LSclient* client;
      };

//---------------------------------------------------------
//   LanguageServerList
//---------------------------------------------------------

struct LanguageServerList : public std::vector<LanguageServer> {
      };

//---------------------------------------------------------
//   Vector
//---------------------------------------------------------

template <typename T> class Vector : public std::vector<T>
      {
    public:
      void remove(int index) { std::vector<T>::erase(std::vector<T>::begin() + index); }
      void insert(int index, const T& k) { std::vector<T>::insert(std::vector<T>::begin() + index, k); }
      void erase(int index) { std::vector<T>::erase(std::vector<T>::begin() + index); }
      };

//---------------------------------------------------------
//   History
//---------------------------------------------------------

struct HistoryRecord {
      Kontext* kontext;
      Pos cursor;
      Pos screenPosition;
      };

using History = std::vector<HistoryRecord>;

//---------------------------------------------------------
//   HoverKontext
//---------------------------------------------------------

struct HoverKontext {
      File* file{nullptr};
      Pos cursor;
      bool operator==(const HoverKontext& k) const { return k.file == file && k.cursor == cursor; }
      };

//---------------------------------------------------------
//   Action
//---------------------------------------------------------

struct Action {
      std::function<void(void)> func;
      std::list<QKeySequence> seq;
      Action(const ShortcutConfig& sc, std::function<void(void)> f) : func(f) {
            QStringList sl = sc.sequence.split(';');
            for (auto s : sl) {
                  auto ks = QKeySequence::fromString(s);
                  seq.push_back(ks);
                  }
            }
      };

//---------------------------------------------------------
//   Editor
//---------------------------------------------------------

class Editor : public QMainWindow
      {
      Q_OBJECT
      QML_ELEMENT
      QML_UNCREATABLE("no")

      Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
      Q_PROPERTY(QList<ShortcutConfig> shortcuts READ shortcuts WRITE setShortcuts NOTIFY shortcutsChanged)
      Q_PROPERTY(QList<FileTypeConfig> fileTypes READ fileTypes WRITE setFileTypes NOTIFY fileTypesChanged)
      Q_PROPERTY(QList<LanguageServerConfig> languageServersConfig READ languageServersConfig WRITE setLanguageServersConfig NOTIFY
                     languageServersConfigChanged)
      Q_PROPERTY(QStringList monospacedFonts READ monospacedFonts CONSTANT)
      Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
      Q_PROPERTY(QColor fgColor READ fgColor WRITE setFgColor NOTIFY fgColorChanged)
      Q_PROPERTY(QColor bgColor READ bgColor WRITE setBgColor NOTIFY bgColorChanged)

      QList<ShortcutConfig> _shortcuts;
      QList<FileTypeConfig> _fileTypes;
      QList<LanguageServerConfig> _languageServersConfig;

      std::vector<File*> files;
      Vector<Kontext*> _kontextList;
      History history;
      CompletionsPopup* completionsPopup;
      Completions completions;

      LanguageServerList languageServers;

      QWidget* enter;
      QStackedWidget* _stack;
      EditWidget* _editWidget;
      MarkdownWebView* _mdWidget;
      size_t _currentKontext{0};
      QLabel* urlLabel;
      QLabel* lineLabel;
      QLabel* colLabel;
      QLabel* _keyLabel;
      QLabel* branchLabel{nullptr};

      TabBar* tabBar{nullptr};
      QWidget* eframe;
      QScrollBar* hScroll;
      QScrollBar* vScroll;
      Completer* enterLine;
      QString pickText;
      bool pickTextRows;
      QTimer* cursorTimer;
      QTimer* lsUpdateTimer;
      Agent* agent;
      static const int agentMinimumWidth { 500 };
      int agentWidth { agentMinimumWidth };
      static const int gitPanelMinimumWidth { 300 };
      int gitPanelWidth { gitPanelMinimumWidth };
      QListView* gitPanel;
      GitList gitList;

      QSplitter* splitter;
      QToolButton* aiButton;
      QToolButton* _gitButton;
      QToolButton* configButton;
      QProgressBar* progressBar;
      ScreenshotHelper* screenshotHelper{nullptr};

      QRegularExpression searchPattern;
      QString replace;
      bool doReplace{false};
      std::vector<Match> matches;

      std::vector<Action> _pedActions;
      HoverKontext hoverKontext;

      QString _projectRoot;
      bool _projectMode{false};
      bool _hasGit{false};
      QString _currentBranchName;
      Git _git;
      bool _darkMode;
      QColor _fgColor{0, 0, 0};
      QColor _bgColor{240, 240, 240};

      QString _settingsLLModel;
//       QFileSystemWatcher* fileWatcher;

      QFont _font;
      QString _fontFamily{"Source Code Pro"};
      qreal _fontSize{14.0};
      QFont::Weight fontWeight{QFont::Normal};
      qreal _fw;
      qreal _fh;
      qreal _fa;
      qreal _fd;

      bool enterActive{false};

      void initFont();
      void saveStatus();
      void saveProjectStatus();
      bool loadStatus(int argc, char** argv);
      void loadProjectStatus();
      void initEnterWidget();
      void updateIcons(bool dark);

      void addKontext(Kontext* k, int idx = -1);
      void removeKontext(int idx);
      void setCurrentKontext(size_t idx);
      void setCurrentKontext(Kontext*);

      void leaveEnter();
      void rubout();
      void deleteChar();
      void deleteLine();
      void deleteRestOfLine();
      void insertTab();
      void pick();
      void put();
      void rowSelect();
      void colSelect();
      bool initSearch(const QString&);
      bool createMatchList();
      void getNextWord();
      void deleteNextWord();
      QString pickNextWord();
      void cBrackets();
      void cLevel();
      void hideCursor();
      void gotoDefinition();
      void gotoTypeDefinition();
      void gotoImplementation();
      void backKontext();
      Kontext* addFileInitial(const QString& path);
      void requestCompletions();
      File* createNewFile(const QFileInfo& fi);
      void connectKontext(Kontext*);
      void updateStyle();

    public slots:
      void hScrollTo(int);
      void vScrollTo(int);
      void updateCursor();
      void apply();
      void update();
      void quitCmd();
      void saveQuitCmd();
      void formatting();
      void startCmd();
      void endCmd();
      void updateViewMode();
      void screenshot();

    signals:
      void fontFamilyChanged();
      void fontChanged(QFont);
      void shortcutsChanged();
      void fileTypesChanged();
      void languageServersConfigChanged();
      void configApplied(); // Signal an Editor Core, Daten neu zu laden
      void darkModeChanged(bool);
      void fgColorChanged();
      void bgColorChanged();

    public:
      Editor(int argc, char** argv);
      ~Editor();

      const ShortcutConfig& getSC(Cmd cmd);
      void saveAll();
      Kontext* kontext() {
            // _kontextList should never be empty
            if (_currentKontext >= _kontextList.size())
                  _currentKontext = 0;
            return _kontextList[_currentKontext];
            }
      const Kontext* kontext() const {
            return _kontextList.at(_currentKontext);
            }
      Agent* getAgent() const { return agent; }
      QFont font() { return _font; }
      qreal fh() const { return _fh; }
      qreal fa() const { return _fa; }
      qreal fw() const { return _fw; }
      void update(QRect);
      void updateVScrollbar();
      void updateHScrollbar();
      void updateGitHistory();
      EditWidget* editWidget() { return _editWidget; }
      void input(const QString& s);
      int tab() const;
      void searchNext();
      void searchPrev();
      void search(const QString&);
      void rename();
      void rename(Kontext*, const QString& name, int row, int col1, int col2);
      qreal fontSize() const { return _fontSize; }
      void setFontSize(qreal);
      template <typename... Args> void msg(std::string_view rt_fmt_str, Args&&... args) {
            auto s = QString::fromStdString(std::vformat(rt_fmt_str, std::make_format_args(args...)));
            statusBar()->showMessage(s, 20000);
            }
      bool initProject();
      LSclient* lclient();
      File* findFile(QString path);
      void undoPatch(const Pos&, int, const QString&, const Cursor&, const Cursor&);
      LSclient* getLSclient(const QString& serverName);
      void gotoKontext(const QString& path, const Pos& cursor);
      void hover();
      void setInfoText(const QString& s);
      void clearInfoText();
      QLabel* keyLabel() { return _keyLabel; }
      const std::vector<Action>& pedActions() { return _pedActions; }
      void showProgress(bool);
      void showProgress(double);
      bool endSelectionMode();
      QString pickWord();
      void clearSearchMarks();
      Git* git() { return &_git; }
      void foldAll();
      void unfoldAll();
      void foldToggle();
      Kontext* lookupKontext(const QString& path);
      QString projectRoot() const { return _projectRoot; }
      bool projectMode() const { return _projectMode; }
      json readJson(const QString& path);
      QString evalPP(const QString& pdata);
      std::vector<File*>& getFiles() { return files; }
      QString settingsLLModel() { return _settingsLLModel; }
      Kontext* addFile(const QString& path);

      void enterCmd();
      void nextKontext();
      void prevKontext();
      void copyKontext();
      QToolButton* gitButton() { return _gitButton; }
      QStackedWidget* stack() { return _stack; }
      QStringList monospacedFonts() const;
      QString fontFamily() const { return _fontFamily; }
      void setFontFamily(const QString& v) {
            if (_fontFamily != v) {
                  _fontFamily = v;
                  emit fontFamilyChanged();
                  }
            }
      QList<ShortcutConfig> shortcuts() const { return _shortcuts; }
      QList<FileTypeConfig> fileTypes() const { return _fileTypes; }
      QList<LanguageServerConfig> languageServersConfig() const { return _languageServersConfig; }
      void loadDefaults();
      void loadSettings();
      void saveSettings();
      void setShortcuts(const QList<ShortcutConfig>& s);
      void setFileTypes(const QList<FileTypeConfig>& f);
      void setLanguageServersConfig(const QList<LanguageServerConfig>& l);
      void resetToDefaults();
      bool darkMode() const { return _darkMode; }
      void setDarkMode(bool v) {
            if (v != _darkMode) {
                  _darkMode = v;
                  if (_darkMode) {
                        setBgColor(QColor(40, 40, 40));
                        setFgColor(QColor(220, 220, 220));
                        }
                  else {
                        setBgColor(QColor(240, 240, 240));
                        setFgColor(QColor(0, 0, 0));
                        }
                  emit darkModeChanged(_darkMode);
                  }
            }
      QColor fgColor() const { return _fgColor; }
      QColor bgColor() const { return _bgColor; }
      void setFgColor(QColor v) {
            if (v != _fgColor) {
                  _fgColor = v;
                  emit fgColorChanged();
                  }
            }
      void setBgColor(QColor v) {
            if (v != _bgColor) {
                  _bgColor = v;
                  emit bgColorChanged();
                  }
            }
      void showCompletions(const Completions&);
//       QFileSystemWatcher* getFileWatcher() const { return fileWatcher; }
      enum UpdateFlag { UpdateNothing = 0, UpdateLine = 1, UpdateScreen = 2, UpdateAll = 4 };
      Q_DECLARE_FLAGS(UpdateFlags, UpdateFlag)
      };

//---------------------------------------------------------
//   KeyLogger
//---------------------------------------------------------

class KeyLogger : public QObject
      {
      Q_OBJECT
      std::array<QKeyCombination, 4> keys;
      int n;
      Editor* _editor;
      std::vector<Action>* actions;

    protected:
      bool eventFilter(QObject*, QEvent*) override;

    signals:
      void triggered(Action*);
      void keyLabelChanged(QString);

    public:
      std::vector<Action> _pedActions;
      KeyLogger(std::vector<Action>* a, QObject* parent = nullptr) : QObject(parent), actions(a) { clear(); }
      void clear();
      };

//---------------------------------------------------------
//   ConfigDialogWrapper
//---------------------------------------------------------

class ConfigDialogWrapper : public QDialog
      {
      Q_OBJECT
      QQuickWidget* _quickWidget;

    protected:
      // Größe beim Start anpassen
      void showEvent(QShowEvent* event) override;

    public:
      explicit ConfigDialogWrapper(Editor*, QWidget* parent = nullptr);
      ~ConfigDialogWrapper() override = default;
      };

QDataStream& operator<<(QDataStream& out, const ShortcutConfig& v);
QDataStream& operator>>(QDataStream& in, ShortcutConfig& v);

QDataStream& operator<<(QDataStream& out, const FileTypeConfig& v);
QDataStream& operator>>(QDataStream& in, FileTypeConfig& v);

QDataStream& operator<<(QDataStream& out, const LanguageServerConfig& v);
QDataStream& operator>>(QDataStream& in, LanguageServerConfig& v);
