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
#include <QListView>
#include <QTextEdit>
#include <QDialog>
// #include <QFileSystemWatcher>
#include <vector>
#include "file.h"
#include "completer.h"
#include "git.h"
#include "agent.h"
#include "ls.h"
#include "mcp.h"

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
class QTreeView;
class QComboBox;
class QFileSystemModel;
class QModelIndex;
class CompletionsPopup;
class Completion;
class QProgressBar;
class Agent;
class MarkdownWebView;
class ConfigWebView;
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
      CMD_SELECT_CHAR,
      CMD_FLIP_CURSOR,

      CMD_SEARCH_NEXT,
      CMD_RENAME,
      CMD_PICK,
      CMD_PUT,
      CMD_SEARCH_PREV,
      CMD_DELETE_WORD,
      CMD_ENTER_WORD,
      CMD_GOTO_BACK,

      CMD_TOGGLE_AI,
      CMD_TOGGLE_GIT,
      CMD_TOGGLE_PROJECT,
      CMD_TOGGLE_CONFIG,

      CMD_FOLD_TOGGLE,
      CMD_FORMAT,
      CMD_VIEW_FUNCTIONS,
      CMD_ANNOTATIONS,
      CMD_GOTO_TYPE_DEFINITION,
      CMD_GOTO_IMPLEMENTATION,
      CMD_GOTO_DEFINITION,
      CMD_EXPAND_MACROS,
      CMD_COMPLETIONS,
      CMD_FOLD_ALL,
      CMD_UNFOLD_ALL,
      CMD_FUNCTION_HEADER,
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
      //      Q_PROPERTY(Cmd cmd MEMBER cmd)
      Q_PROPERTY(QString id MEMBER id)
      Q_PROPERTY(QString description MEMBER description)
      Q_PROPERTY(QString buildin MEMBER sequence)
      Q_PROPERTY(QString sequence MEMBER sequence)

    public:
      ShortcutConfig() { sequence = buildin; }
      ShortcutConfig(QString b, QString c, QString d) : id(b), description(c), buildin(d), sequence(d) {}
      //      Cmd cmd;
      QString id;
      QString description;
      QString buildin;
      QString sequence;
      bool operator==(const ShortcutConfig& other) const = default;
      };

Q_DECLARE_METATYPE(ShortcutConfig)

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

    protected:
      bool ro {false};
      bool modified {false};

    public slots:
      void modifiedChanged();

    public:
      TabBar(QWidget* parent = nullptr) : QTabBar(parent) {
            setMovable(true);
            //            connect(this, &TabBar::currentChanged, this, &TabBar::modifiedChanged);
            }
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
      File* file {nullptr};
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

      Q_PROPERTY(QStringList projects READ projects WRITE setProjects NOTIFY projectsChanged)
      Q_PROPERTY(Models models READ models WRITE setModels NOTIFY modelsChanged)
      Q_PROPERTY(AgentRoles agentRoles READ agentRoles WRITE setAgentRoles NOTIFY agentRolesChanged)
      Q_PROPERTY(QString agentRoleName READ agentRoleName WRITE setAgentRoleName NOTIFY agentRoleNameChanged)
      Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
      Q_PROPERTY(QString fontDemo READ fontDemo WRITE setFontDemo NOTIFY fontDemoChanged)
      Q_PROPERTY(QList<ShortcutConfig> shortcuts READ shortcuts WRITE setShortcuts NOTIFY shortcutsChanged)
      Q_PROPERTY(QList<FileType> fileTypes READ fileTypes NOTIFY fileTypesChanged)
      Q_PROPERTY(QList<LanguageServerConfig> languageServersConfig READ languageServersConfig WRITE
                     setLanguageServersConfig NOTIFY languageServersConfigChanged)
      Q_PROPERTY(McpServerConfigs mcpServersConfig READ mcpServersConfig WRITE setMcpServersConfig NOTIFY
                     configChanged)
      Q_PROPERTY(QStringList monospacedFonts READ monospacedFonts CONSTANT)
      Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
      Q_PROPERTY(TextStyles textStylesLight READ textStylesLight WRITE setTextStylesLight NOTIFY
                     textStylesLightChanged)
      Q_PROPERTY(
          TextStyles textStylesDark READ textStylesDark WRITE setTextStylesDark NOTIFY textStylesDarkChanged)

      static std::map<Cmd, ShortcutConfig> _shortcuts;

      QStringList _projects;

      FileTypes _fileTypes;
      TextStyles _textStylesLight;
      TextStyles _textStylesDark;
      QList<ShortcutConfig> _shortcutsList;
      LanguageServersConfig _languageServersConfig;
      McpServerConfigs _mcpServersConfig;

      std::vector<File*> files;
      Vector<Kontext*> _kontextList;
      History history;
      CompletionsPopup* completionsPopup;
      Completions completions;

      LanguageServerList languageServers;

      QWidget* box;
      bool _aiVisible {false};
      bool _gitVisible {false};

      QWidget* enter;
      QStackedWidget* _stack;
      EditWidget* _editWidget;
      MarkdownWebView* _mdWidget {nullptr};
      QWidget* _mdContainer {nullptr};
      ConfigWebView* _configWebView {nullptr};
      QWidget* _configContainer {nullptr};

      size_t _currentKontext {0};
      QLabel* urlLabel;
      QLabel* lineLabel;
      QLabel* colLabel;
      QLabel* _keyLabel;
      QLabel* branchLabel {nullptr};

      TabBar* tabBar {nullptr};
      QWidget* eframe;
      QScrollBar* hScroll;
      QScrollBar* vScroll;
      Completer* enterLine;

      QTimer* cursorTimer;
      QTimer* lsUpdateTimer;
      Agent* _agent {nullptr};
      Models _models;
      AgentRoles _agentRoles;
      QString _agentRoleName;

      static const int agentMinimumWidth {500};
      static const int gitPanelMinimumWidth {300};
      int agentWidth {agentMinimumWidth};
      int gitWidth {gitPanelMinimumWidth};
      QStackedWidget* _sidePanelStack {nullptr};
      QWidget* _gitPanel {nullptr};
      QListView* gitListView {nullptr};
      bool gitDiff {false};
      GitList gitList;

      static const int projectPanelMinimumWidth {300};
      int projectWidth {projectPanelMinimumWidth};
      QWidget* _projectPanel {nullptr};
      QComboBox* _projectComboBox {nullptr};
      void switchProject(const QString& path);
      QTreeView* _projectTreeView {nullptr};
      QFileSystemModel* _projectModel {nullptr};
      QToolButton* _projectButton {nullptr};
      bool _projectVisible {false};

      void updateProjectPanel();

      QSplitter* splitter;
      QToolButton* aiButton;
      QToolButton* _gitButton;
      QToolButton* configButton;
      QProgressBar* progressBar;
      ScreenshotHelper* screenshotHelper {nullptr};

      QRegularExpression searchPattern;
      QString replace;
      bool doReplace {false};
      std::vector<Match> matches;

      std::vector<Action> _pedActions;
      HoverKontext hoverKontext;

      QString _projectRoot;
      bool _projectMode {false};
      bool _hasGit {false};
      QString _currentBranchName;
      Git _git;
      bool _darkMode {false};

      //      QString _settingsLLModel;
      //       QFileSystemWatcher* fileWatcher;

      QFont _font;
      QString _fontFamily {"Source Code Pro"};
      qreal _fontSize {14.0};
      QFont::Weight fontWeight {QFont::Normal};
      qreal _fw;
      qreal _fh;
      qreal _fa;
      qreal _fd;

      QString _fontDemo {"The quick brown fox jumps over the lazy dog"};

      PickText pickText;

      Agent* agent();
      MarkdownWebView* mdWidget();

      bool enterActive {false};

      void initFont();
      void updateProjectTreeColors();
      void saveStatus();
      bool loadStatus(int argc, char** argv);
      void initEnterWidget();
      void updateIcons(bool dark);

      void addKontext(Kontext* k, int idx = -1);
      void removeKontext(int idx);

      void leaveEnter();
      void rubout();
      void deleteChar();
      void deleteLine();
      void deleteRestOfLine();
      void insertTab();
      void pick();

    public:
      static QIcon createStatefulIcon(const QString& svgPath, const QColor& normalColor, const QColor& hoverColor = QColor(), const QColor& checkedColor = QColor());
      void put();
      void setPickText(const QString& text, SelectionMode mode = SelectionMode::CharSelect);

    private:
      void rowSelect();
      void colSelect();
      void charSelect();
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
      void setViewMode(ViewMode vm);
      void screenshot();
      void applyCompletion(int idx);

    signals:
      void fontFamilyChanged();
      void fontChanged(QFont);
      void fontDemoChanged();
      void shortcutsChanged();
      void fileTypesChanged();
      void languageServersConfigChanged();
      void configChanged();
      void configApplied(); // Signal an Editor Core, Daten neu zu laden
      void darkModeChanged(bool);
      void textStylesLightChanged();
      void textStylesDarkChanged();
      void modelsChanged();
      void agentRolesChanged();
      void agentRoleNameChanged();
      void screenshotReady(const QImage& image);
      void projectsChanged();

    public:
      Editor(int argc, char** argv);
      ~Editor();

      void setCurrentKontext(size_t idx);
      void setCurrentKontext(Kontext*);
      void toggleViewMode();

      const ShortcutConfig& getSC(Cmd cmd);
      void saveAll();
      Kontext* kontext() {
            // _kontextList should never be empty
            if (_currentKontext >= _kontextList.size())
                  return nullptr;
            return _kontextList[_currentKontext];
            }
      const Kontext* kontext() const { return _kontextList.at(_currentKontext); }
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
      //      QString settingsLLModel() { return _settingsLLModel; }
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
      QString fontDemo() const { return _fontDemo; }
      void setFontDemo(const QString& v) {
            if (_fontDemo != v) {
                  _fontDemo = v;
                  emit fontDemoChanged();
                  }
            }
      FileTypes fileTypes() const { return _fileTypes; }
      void setFileTypes(const FileTypes& ft) {
            if (_fileTypes != ft) {
                  _fileTypes = ft;
                  emit fileTypesChanged();
                  }
            }
      TextStyles textStylesLight() { return _textStylesLight; }
      TextStyles textStylesDark() { return _textStylesDark; }
      void setTextStylesLight(const TextStyles& v) {
            if (v != _textStylesLight) {
                  _textStylesLight = v;
                  emit textStylesLightChanged();
                  }
            }
      void setTextStylesDark(const TextStyles& v) {
            if (v != _textStylesDark) {
                  _textStylesDark = v;
                  emit textStylesDarkChanged();
                  }
            }
      QList<LanguageServerConfig> languageServersConfig() const { return _languageServersConfig; }
      McpServerConfigs mcpServersConfig() const { return _mcpServersConfig; }
      void setMcpServersConfig(const McpServerConfigs& c) {
            if (_mcpServersConfig != c) {
                  _mcpServersConfig = c;
                  agent()->mcpManager()->applyConfigs(c);
                  emit configChanged();
                  }
            }
      void setLanguageServersConfig(const QList<LanguageServerConfig>& l) {
            LanguageServersConfig c;
            for (const auto& cfg : l) // ugh!
                  c.push_back(cfg);
            if (_languageServersConfig != c) {
                  _languageServersConfig = c;
                  emit languageServersConfigChanged();
                  }
            }
      void loadSettings();
      void saveSettings();
      Q_INVOKABLE void resetToDefaults();
      void updateShortcut(const QString& id, const QString& sequence);
      void showConfig();
      void hideConfig();
      QList<ShortcutConfig> shortcutsList() const { return shortcuts(); }
      bool darkMode() const { return _darkMode; }
      void setDarkMode(bool v) {
            if (v != _darkMode) {
                  _darkMode = v;
                  updateStyle();
                  initFont();
                  emit darkModeChanged(_darkMode);
                  updateProjectTreeColors();
                  update();
                  }
            }
      void showCompletions(const Completions&);
      void hideCompletions();
      //       QFileSystemWatcher* getFileWatcher() const { return fileWatcher; }
      enum UpdateFlag { UpdateNothing = 0, UpdateLine = 1, UpdateScreen = 2, UpdateAll = 4 };
      Q_DECLARE_FLAGS(UpdateFlags, UpdateFlag)
      const TextStyle& textStyle(TextStyle::Style style) {
            return darkMode() ? _textStylesDark[int(style)] : _textStylesLight[int(style)];
            }
      Q_INVOKABLE TextStyle textStyle(bool dark, int style) {
            return dark ? _textStylesDark[int(style)] : _textStylesLight[int(style)];
            }
      Q_INVOKABLE void setTextStyle(const TextStyle& s, bool dark, int style) {
            Debug("dark {} style {}", dark, style);
            if (dark) {
                  _textStylesDark[style] = s;
                  // emit textStylesDarkChanged();
                  }
            else {
                  _textStylesLight[style] = s;
                  // emit textStylesLightChanged();
                  }
            update();
            }
      Model model(int idx) { return _models.at(idx); }
      void setModel(int idx, const Model& m) { _models[idx] = m; }
      void addModel(const Model& m) { _models.push_back(m); }
      const Models& models() const { return _models; }
      void setModels(const Models& m) {
            if (_models != m) {
                  _models = m;
                  emit modelsChanged();
                  }
            }
      void showGitVersion(int row);
      QWidget* gitPanel();
      AgentRole agentRole(int idx) { return _agentRoles[idx]; }
      void setAgentRole(int idx, const AgentRole& r) { _agentRoles[idx] = r; }
      const AgentRoles& agentRoles() const { return _agentRoles; }
      void addAgentRole(const AgentRole& r) { _agentRoles.push_back(r); }
      void setAgentRoles(const AgentRoles& a) {
            if (_agentRoles != a) {
                  _agentRoles = a;
                  emit agentRolesChanged();
                  }
            }
      void setAgentRoleName(const QString& s) { _agentRoleName = s; }
      QString agentRoleName() const { return _agentRoleName; }
      QList<ShortcutConfig> shortcuts() const;
      void setShortcuts(const QList<ShortcutConfig>& l) {
            _shortcutsList = l;
            emit shortcutsChanged();
            }
      QStringList projects() const { return _projects; }
      void setProjects(const QStringList& v) {
            if (v != _projects) {
                  _projects = v;
                  emit projectsChanged();
                  }
            }
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

    public:
      explicit ConfigDialogWrapper(Editor*, QWidget* parent = nullptr);
      ~ConfigDialogWrapper() override = default;
      };
