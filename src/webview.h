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
#include <QString>
#include <string>
#include <vector>
#include <format>
#include <iostream>
#include <QUrl>

#include "logger.h"
#include "editor.h"

class KeyLogger;

//---------------------------------------------------------
//   MarkdownWebView
//---------------------------------------------------------

class MarkdownWebView : public QWebEngineView
      {
      Q_OBJECT

      QString getHighlightJsAssets(bool darkMode) const;
      QString getAnchorJs() const;
      QString getTocJs() const;

      QString _currentRawMarkdown;
      QString _currentRawHtml;

      std::vector<Action> textActions;
      Editor* editor;
      KeyLogger* kl{nullptr};
      // Hilfsmethode für JS-Injection
      void executeScroll(int pixelsY);

    protected:
      bool _darkMode{false};
      bool isLoaded{true};
      // Hilft uns, das interne Chromium-Widget zu finden
      void childEvent(QChildEvent* event) override;
      void installFilterOnProxy();
      QString getGithubCss() const;

    public slots:
      QString getGithubDarkCss() const;
      virtual void setDarkMode(bool);

    public:
      explicit MarkdownWebView(Editor*, QWidget* parent = nullptr);

      void setHtml(const QString& html, const QUrl& baseUrl = QUrl());
      void setMarkdown(const QString& markdown);
      void append(const QString&);
      void clear() { setMarkdown(""); }
      QString renderMarkdownToHtml(const std::string& markdown);

      // Scrolling Interface
      void scrollLineUp();
      void scrollLineDown();
      void scrollPageUp();
      void scrollPageDown();
      void scrollToTop();
      void scrollToBottom();
      };
