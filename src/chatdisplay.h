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
      std::string currentStreamingThought;
      std::string currentStreamingText;

      QString getHighlightJsCss() const;
      QString getHighlightJsDarkCss() const;
      QString getChatCss() const;
      QString getChatDarkCss() const;

    public slots:
      void handleIncomingChunk(const std::string& thoughtChunk, const std::string& textChunk);

    public:
      ChatDisplay(Editor* e, QWidget* parent = nullptr) : MarkdownWebView(e, parent) {}
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
      void startNewStreamingMessage(const std::string& role) {
            currentStreamingThought.clear();
            currentStreamingText.clear();
            auto s = std::format("startNewStreamingMessage('{}');", role);
            page()->runJavaScript(QString::fromStdString(s));
            }
      void appendStaticHtml(const QString& role, const QString& html, const QString& thoughtHtml = "");

      void setDarkMode(bool enabled) override;
      void setMarkdown(const QString&) { Fatal("not impl."); };
      void addMessage(const std::string& role, const std::string& text) {
            startNewStreamingMessage(role);
            handleIncomingChunk("", text);
            scrollToBottom();
            }
      };
