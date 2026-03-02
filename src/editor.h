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
#include <vector>

#include "file.h"
#include "completion.h"
#include "git.h"

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
class QTextEdit;
class QSplitter;
class QToolButton;
class QListWidget;
class CompletionsPopup;
class QProgressBar;
class Agent;

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
      const char* keys;
      std::function<void(void)> func;
      std::list<QKeySequence> seq;
      Action(const char* s, std::function<void(void)> f) : keys(s), func(f) {
            QStringList sl = QString(keys).split(';');
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

      std::vector<File*> files;
      Vector<Kontext*> _kontextList;
      History history;

      LanguageServerList languageServers;

      QWidget* enter;
      EditWidget* _editWidget;
      size_t _currentKontext{0};
      QLabel* urlLabel;
      QLabel* lineLabel;
      QLabel* colLabel;
      QLabel* _keyLabel;

      TabBar* tabBar{nullptr};
      QWidget* eframe;
      QStackedWidget* stack;
      QScrollBar* hScroll;
      QScrollBar* vScroll;
      HistoryLineEdit* enterLine;
      QString pickText;
      bool pickTextRows;
      QTimer* cursorTimer;
      QTimer* lsUpdateTimer;
      Agent* agent;
      QListView* gitPanel;
      GitList gitList;

      QSplitter* splitter;
      QToolButton* infoButton;
      QToolButton* gitButton;
      QProgressBar* progressBar;

      QRegularExpression searchPattern;
      QString replace;
      bool doReplace{false};
      std::vector<Match> matches;

      std::vector<Action> _pedActions;
      HoverKontext hoverKontext;
      CompletionsPopup* completionsPopup;
      Completions completions;

      QString _projectRoot;
      Git _git;

      QString _settingsLLModel;

    public:
      enum UpdateFlag { UpdateNothing = 0, UpdateLine = 1, UpdateScreen = 2, UpdateAll = 4 };
      Q_DECLARE_FLAGS(UpdateFlags, UpdateFlag)

    private:
      QFont _font;
      //      QString fontFamily       { "Bitstream Vera Sans Mono"};
      //      QString fontFamily{"Hack"};
      //      QString fontFamily{"DejaVu Sans Mono"};
      QString fontFamily{"Source Code Pro"};
      qreal _fontSize{14.0};
      QFont::Weight fontWeight{QFont::Normal};
      qreal _fw;
      qreal _fh;
      qreal _fa;
      qreal _fd;

      bool enterActive{false};

      void quitCmd();
      void saveQuitCmd();
      void initFont();
      void saveStatus();
      void saveProjectStatus();
      bool loadStatus(int argc, char** argv);
      void loadProjectStatus();
      void enterCmd();
      void initEnterWidget();

      void addKontext(Kontext* k, int idx = -1);
      void removeKontext(int idx);
      void setCurrentKontext(size_t idx);
      void setCurrentKontext(Kontext*);
      void nextKontext();
      void prevKontext();
      void copyKontext();

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

    public slots:
      void hScrollTo(int);
      void vScrollTo(int);
      void updateCursor();

   signals:
      void fontChanged(QFont);

    public:
      Editor(int argc, char** argv);
      ~Editor();

      void saveAll();
      void formatting();
      Kontext* kontext() {
            if (_currentKontext >= _kontextList.size()) {
                  // Fatal("bad context {} size {}", _currentKontext, _kontext.size());
                  return nullptr;
                  }
            return _kontextList[_currentKontext];
            }
      const Kontext* kontext() const { return _kontextList[_currentKontext]; }
      Agent* getAgent() const { return agent; }
      QFont font() { return _font; }
      qreal fh() const { return _fh; }
      qreal fa() const { return _fa; }
      qreal fw() const { return _fw; }
      QColor fgColor() const { return QColor(0, 0, 0); }
      QColor bgColor() const { return QColor(240, 240, 240); }
      void update(QRect);
      void update();
      void updateVScrollbar();
      void updateGitHistory();
      EditWidget* editWidget() { return _editWidget; }
      void input(const QString& s);
      void startCmd();
      void endCmd();
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
      void showCompletions(const Completions&);

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
      json readJson(const QString& path);
      QString evalPP(const QString& pdata);
      std::vector<File*>& getFiles() { return files; }
      QString settingsLLModel() { return _settingsLLModel; }
      Kontext* addFile(const QString& path);
      };

//---------------------------------------------------------
//   KeyLogger
//---------------------------------------------------------

class KeyLogger : public QObject
      {
      Q_OBJECT
      std::array<QKeyCombination, 4> keys;
      int n;

    protected:
      bool eventFilter(QObject*, QEvent*) override;

    public:
      KeyLogger(QObject* parent = nullptr) : QObject(parent) { clear(); }
      Editor* editor() { return static_cast<Editor*>(parent()); }
      void clear();
      };
