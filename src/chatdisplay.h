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
#include "webview.h"

//---------------------------------------------------------
//   ChatDisplay
//---------------------------------------------------------

class ChatDisplay : public MarkdownWebView
      {
      Q_OBJECT
      Editor* _editor;
      std::string currentStreamingThought;
      std::string currentStreamingText;

      QString getHighlightJsCss() const;
      QString getHighlightJsDarkCss() const;
      QString getChatCss() const;
      QString getChatDarkCss() const;
      bool mustStartMessage { false };
      std::string role;
      void safeRunJs(const QString& call);

    public slots:
      void handleIncomingChunk(const std::string& thoughtChunk, const std::string& textChunk);

    public:
      ChatDisplay(Editor* e, QWidget* parent = nullptr) : MarkdownWebView(e, parent), _editor(e) {}
      void scrollToBottom() { MarkdownWebView::scrollToBottom(); }
      void setup();
      void clear() {
            setup();
            while (!isLoaded)
                  qApp->processEvents();
            }
      QWidget* widget() { return (QWidget*)this; }
      void setFont(QFont f) { MarkdownWebView::setFont(f); }
      QString quoteForJs(const QString& str);

      void startNewStreamingMessage(const std::string& r) {
            mustStartMessage = true;
            role = r;
            }
      void startMessage();
      void appendStaticHtml(const QString& role, const QString& html, const QString& thoughtHtml = "", bool isActive = true);

      void setDarkMode(bool enabled);
      void setMarkdown(const QString&) { Fatal("not impl."); };
      void addMessage(const std::string& role, const std::string& text) {
            startNewStreamingMessage(role);
            handleIncomingChunk("", text);
            scrollToBottom();
            }
      };
