#include <QChildEvent>
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

#pragma once

#include <QWebEngineHistory>
#include <QWebEngineView>
#include <QWheelEvent>
#include <QWebEnginePage>
#include <QString>
#include <string>
#include <vector>
#include <QUrl>

#include "logger.h"
#include "editor.h"

class KeyLogger;

//---------------------------------------------------------
//   MarkdownWebPage
//---------------------------------------------------------

class MarkdownWebPage : public QWebEnginePage
      {
      Q_OBJECT
      Editor* _editor;

    protected:
       
      bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override;

    public:
      explicit MarkdownWebPage(Editor* e, QObject* parent = nullptr);
      };

//---------------------------------------------------------
//   MarkdownWebView
//---------------------------------------------------------

class MarkdownWebView : public QWebEngineView
      {
      Q_OBJECT

      QString getHighlightJsAssets(bool darkMode) const;
      const std::string& getAnchorJs() const;
      const std::string& getTocJs() const;

      QString _currentRawMarkdown;
      std::vector<Action> textActions;
      QString _pendingDiff;
      QString _currentDiff;
      Editor* _editor;
      KeyLogger* kl {nullptr};
      // Hilfsmethode für JS-Injection
      void executeScroll(int pixelsY);

    protected:
      bool eventFilter(QObject* obj, QEvent* event) override;
      bool _darkMode {false};
      bool isLoaded {true};
      // Hilft uns, das interne Chromium-Widget zu finden
      QWebEngineView* createWindow(QWebEnginePage::WebWindowType type) override;
      void childEvent(QChildEvent* event) override;
      void installFilterOnProxy();
      std::string getGithubCss() const;
      std::string getGithubDarkCss() const;

    public slots:
      virtual void updateStyle();
      Editor* editor() const { return _editor; }

    public:
      explicit MarkdownWebView(Editor*, QWidget* parent = nullptr);

      void setHtml(const QString& html, const QUrl& baseUrl = QUrl());
      void setMarkdown(const QString& markdown, int cursorLine = -1);
      void append(const QString&);
      void clear() { setMarkdown(""); }
      QString renderMarkdownToHtml(const std::string& markdown);
      QString getMermaidJs(bool darkMode) const;
      QString getKaTexJs() const;
      QString getScrollbarCss(bool darkMode) const;
      void showGitDiff(const QString& diffOutput);

      // Scrolling Interface
      void scrollLineUp();
      void scrollLineDown();
      void scrollPageUp();
      void scrollPageDown();
      void scrollToTop();
      void scrollToBottom();
      void wheelEvent(QWheelEvent* ev) override;
      };
