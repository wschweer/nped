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
      QString currentStreamingThought;
      QString currentStreamingText;

      QString getHighlightJsCss() const;
      QString getHighlightJsDarkCss() const;
      QString getChatCss() const;
      QString getChatDarkCss() const;

    public:
      ChatDisplay(Editor* e, QWidget* parent = nullptr) : MarkdownWebView(e, parent) {}
      void setup();
      void appendMessageWithThought(const QString& role, const QString& thought, const QString& text);
      void appendMessage(const QString& role, const QString& text);
      void scrollToBottom() { MarkdownWebView::scrollToBottom(); }
      void clear() {
            setup();
            bool busy = true;
            connect(this, &QWebEngineView::loadFinished, this, [&busy] {
                  busy = false;
                  }, Qt::QueuedConnection | Qt::SingleShotConnection);
            while (busy) {
                  qApp->processEvents();
                  }
            }
      QWidget* widget() { return (QWidget*)this; }
      void setFont(QFont f) { MarkdownWebView::setFont(f); }
      QString quoteForJs(const QString& str);
      void startNewStreamingMessage(const QString& role) {
            currentStreamingThought.clear();
            currentStreamingText.clear();
            auto s = QString("startNewStreamingMessage('%1');").arg(role);
            page()->runJavaScript(s);
            }
      void appendStaticHtml(const QString& role, const QString& html, const QString& thoughtHtml = "");

      void setDarkMode(bool enabled) override;
      //      void setHtml(const QString&) { Critical("not impl."); }
      void setMarkdown(const QString&) { Critical("not impl."); };
      void append(const QString& t) { appendMessage("system", t); }
    public slots:
      void handleIncomingChunk(const QString& thoughtChunk, const QString& textChunk);
      };
