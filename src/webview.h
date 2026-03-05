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

#include "logger.h"
#include "editor.h"

class KeyLogger;

//---------------------------------------------------------
//   MarkdownWebView
//---------------------------------------------------------

class MarkdownWebView : public QWebEngineView
      {
      Q_OBJECT

      QString renderMarkdownToHtml(const QString& markdown);
      QString getGithubCss() const;
      QString getGithubDarkCss() const;
      QString getHighlightJsAssets(bool darkMode) const;

      QString _currentRawMarkdown;
      QString _currentRawHtml;

      std::vector<Action> textActions;
      Editor* editor;
      KeyLogger* kl;
      bool _darkMode{false};
      // Hilfsmethode für JS-Injection
      void executeScroll(int pixelsY);

    protected:
      // Hilft uns, das interne Chromium-Widget zu finden
      void childEvent(QChildEvent* event) override;
      void installFilterOnProxy();

    public:
      explicit MarkdownWebView(Editor*, QWidget* parent = nullptr);

      void setHtml(const QString& html);
      void setMarkdown(const QString& markdown);
      void setDarkMode(bool);
      // Scrolling Interface
      void scrollLineUp();
      void scrollLineDown();
      void scrollPageUp();
      void scrollPageDown();
      void scrollToTop();
      void scrollToBottom();
      };
