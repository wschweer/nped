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

#include <md4c-html.h>
#include <QStackedWidget>
#include <QToolButton>
#include <QLabel>

#include "webview.h"
#include "commands.h"
#include "kontext.h"

//---------------------------------------------------------
//   MarkdownWebView
//---------------------------------------------------------

MarkdownWebView::MarkdownWebView(Editor* e, QWidget* _parent) : QWebEngineView(_parent), editor(e) {
      setZoomFactor(1.5);
      textActions = {
         Action(CMD_QUIT, [this] { editor->quitCmd(); }),
         Action(CMD_SAVE_QUIT, [this] { editor->saveQuitCmd(); }),

         Action(CMD_LINE_UP, [this] { scrollLineUp(); }),
         Action(CMD_LINE_DOWN, [this] { scrollLineDown(); }),
         Action(CMD_FILE_BEGIN, [this] { scrollToTop(); }),
         Action(CMD_FILE_END, [this] { scrollToBottom(); }),
         Action(CMD_PAGE_UP, [this] { scrollPageUp(); }),
         Action(CMD_PAGE_DOWN, [this] { scrollPageDown(); }),
#if 0
         Action(CMD_LINE_TOP, [] { }),
         Action(CMD_LINE_BOTTOM, [] {}),
         Action(CMD_ENTER, [] {}),
#endif
         Action(CMD_SAVE, [this] { editor->kontext()->file()->save(); }),
         Action(CMD_KONTEXT_COPY, [this] { editor->copyKontext(); }),
         Action(CMD_KONTEXT_PREV, [this] { editor->prevKontext(); }),
         Action(CMD_KONTEXT_NEXT, [this] { editor->nextKontext(); }),

         Action(CMD_KONTEXT_UP, [this] { editor->kontext()->moveCursorRel(0, -1, MoveType::Roll); }),
         Action(CMD_KONTEXT_DOWN, [this] { editor->kontext()->moveCursorRel(0, 1, MoveType::Roll); }),
         Action(CMD_PICK, [] {}),
         Action(CMD_SELECT_ROW, [] {}),
         Action(CMD_SELECT_COL, [] {}),
         Action(CMD_VIEW_FUNCTIONS,
                [this] {
                      editor->kontext()->toggleViewMode();
                      editor->updateViewMode();
                      }),
         Action(CMD_SEARCH_NEXT, [] {}),
         Action(CMD_SEARCH_PREV, [] {}),
         Action(CMD_GIT_TOGGLE, [this] { editor->gitButton()->setChecked(!editor->gitButton()->isChecked()); }),
            };

      kl = new KeyLogger(&textActions, this);
      installEventFilter(kl);

      connect(kl, &KeyLogger::triggered, [this](Action* a) {
            editor->startCmd();
            a->func();
            editor->endCmd();
            });
      connect(kl, &KeyLogger::keyLabelChanged, [this](QString s) {
            editor->keyLabel()->setText(s);
            });
      }

//---------------------------------------------------------
//   childEvent
//   fängt interne Widgets ab, die QtWebEngine später erstellt
//---------------------------------------------------------

void MarkdownWebView::childEvent(QChildEvent* event) {
      if (event->type() == QEvent::ChildAdded) {
            // Wenn ein Kind hinzugefügt wird, prüfen wir, ob wir darauf lauschen müssen
            QObject* child = event->child();
            if (child)
                  child->installEventFilter(kl);
            }
      QWebEngineView::childEvent(event);
      }

//---------------------------------------------------------
//   installFilterOnProxy
//---------------------------------------------------------

void MarkdownWebView::installFilterOnProxy() {
      // Manchmal hat der View selbst schon einen FocusProxy
      if (focusProxy())
            focusProxy()->installEventFilter(kl);
      // Und sicherheitshalber auf uns selbst
      this->installEventFilter(kl);
      }

//---------------------------------------------------------
//   setHtml
//---------------------------------------------------------

void MarkdownWebView::setHtml(const QString& _html) {
      _currentRawHtml = _html;
      Debug("Setting direct HTML content. Length: {}", _html.length());
      QWebEngineView::setHtml(_html);
      }

//---------------------------------------------------------
//   setDarkMode
//---------------------------------------------------------

void MarkdownWebView::setDarkMode(bool enabled) {
      if (_darkMode == enabled)
            return;

      _darkMode = enabled;

      // Wenn wir schon Content haben, rendern wir ihn sofort neu mit dem neuen Style
      if (!_currentRawMarkdown.isEmpty())
            setMarkdown(_currentRawMarkdown);
      }

//---------------------------------------------------------
//   setMarkdown
//---------------------------------------------------------

void MarkdownWebView::setMarkdown(const QString& _markdown) {
      _currentRawMarkdown = _markdown; // Speichern für späteren Reload

      if (_markdown.isEmpty()) {
            setHtml("");
            return;
            }

      QString _convertedHtml = renderMarkdownToHtml(_markdown);

      QString _css    = _darkMode ? getGithubDarkCss() : getGithubCss();
      QString _assets = getHighlightJsAssets(_darkMode);

      QString _fullHtml = QString::fromStdString(std::format(
          R"(<!DOCTYPE html>
        <html>
        <head>
            <meta charset="utf-8">
            <style>
                {}
                /* Override für Highlight.js Hintergrund im Darkmode,
                   damit es nahtlos in den pre-Block passt */
                .hljs {{ background: transparent !important; }}
            </style>
            {}
        </head>
        <body class="markdown-body">
            {}
        </body>
        </html>)",
          _css.toStdString(), _assets.toStdString(), _convertedHtml.toStdString()));

      setHtml(_fullHtml);
      }

//---------------------------------------------------------
//   getHighlightJsAssets
//---------------------------------------------------------

QString MarkdownWebView::getHighlightJsAssets(bool darkMode) const {
      // Wähle das passende Theme: 'github' vs 'github-dark'
      QString theme = darkMode ? "github-dark.min.css" : "github.min.css";

      // Nutze std::format für sauberen String-Bau
      return QString::fromStdString(std::format(R"(
        <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/{}">
        <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
        <script>
            document.addEventListener('DOMContentLoaded', (event) => {{
                document.querySelectorAll('pre code').forEach((el) => {{
                    hljs.highlightElement(el);
                }});
            }});
        </script>
    )", theme.toStdString()));
      }

//---------------------------------------------------------
//   md4c_callback
//    Callback für md4c
//---------------------------------------------------------

void md4c_callback(const MD_CHAR* _data, MD_SIZE _size, void* _userData) {
      static_cast<QString*>(_userData)->append(QString::fromUtf8(_data, _size));
      }

//---------------------------------------------------------
//   renderMarkdownToHtml
//---------------------------------------------------------

QString MarkdownWebView::renderMarkdownToHtml(const QString& _markdown) {
      QString _output;
      std::string _stdMarkdown = _markdown.toStdString();

      // GFM-Flags: Tabellen, Tasklisten, Durchgestrichen, Autolinks
      unsigned int _flags = MD_DIALECT_GITHUB;

      int _result = md_html(_stdMarkdown.c_str(), static_cast<MD_SIZE>(_stdMarkdown.size()), md4c_callback, &_output, _flags, 0);

      if (_result != 0) {
            Critical("Markdown conversion failed with code: {}", _result);
            return "<b>Error: Markdown rendering failed.</b>";
            }

      Debug("Markdown rendering successful.");
      return _output;
      }

//---------------------------------------------------------
//   getGithubCss
//---------------------------------------------------------

QString MarkdownWebView::getGithubCss() const {
      return R"(
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif;
            line-height: 1.5; padding: 32px; max-width: 980px; margin: 0 auto; color: #1f2328; background-color: #ffffff;
            scroll-behavior: smooth;
        }
        .markdown-body h1, .markdown-body h2 { border-bottom: 1px solid #d8dee4; padding-bottom: .3em; margin-top: 24px; margin-bottom: 16px; font-weight: 600; }
        .markdown-body a { color: #0969da; text-decoration: none; }
        .markdown-body a:hover { text-decoration: underline; }
        .markdown-body code {
            background-color: rgba(175,184,193,0.2); border-radius: 6px; font-size: 100%; padding: 0.2em 0.4em;
            font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, "Liberation Mono", monospace;
        }
        .markdown-body pre {
            background-color: #f6f8fa; border-radius: 6px; padding: 16px; overflow: auto; font-size: 85%; line-height: 1.45;
        }
        .markdown-body pre code {
            background-color: transparent; border: 0; display: inline; line-height: inherit; margin: 0; padding: 0;
        }
        .markdown-body table { border-spacing: 0; border-collapse: collapse; margin-top: 0; margin-bottom: 16px; width: 100%; }
        .markdown-body table th, .markdown-body table td { border: 1px solid #d0d7de; padding: 6px 13px; }
        .markdown-body table tr { background-color: #ffffff; border-top: 1px solid #hsla(210,18%,87%,1); }
        .markdown-body table tr:nth-child(2n) { background-color: #f6f8fa; }
    )";
      }

//---------------------------------------------------------
//   getGithubDarkCss
//---------------------------------------------------------

QString MarkdownWebView::getGithubDarkCss() const {
      return R"(
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif;
            line-height: 1.5; padding: 32px; max-width: 980px; margin: 0 auto;

            /* DARK MODE FARBEN */
            color: #c9d1d9;             /* Text: Helles Grau */
            background-color: #0d1117;  /* Hintergrund: Sehr dunkles Blau-Grau */
            scroll-behavior: smooth;
        }

        /* Überschriften */
        .markdown-body h1, .markdown-body h2 {
            border-bottom: 1px solid #21262d;
            color: #e6edf3;
        }

        /* Links */
        .markdown-body a { color: #58a6ff; text-decoration: none; }
        .markdown-body a:hover { text-decoration: underline; }

        /* Inline Code (`...`) */
        .markdown-body code {
            background-color: rgba(110,118,129,0.4);
            border-radius: 6px;
            padding: 0.2em 0.4em;
            font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, monospace;
            color: #c9d1d9;
        }

        /* Code Blöcke (```...```) */
        .markdown-body pre {
            background-color: #161b22;
            border-radius: 6px;
            padding: 16px;
            overflow: auto;
            border: 1px solid #30363d;
        }

        /* Zitate */
        .markdown-body blockquote {
            border-left: .25em solid #30363d;
            color: #8b949e;
            padding: 0 1em;
        }

        /* Tabellen */
        .markdown-body table th, .markdown-body table td {
            border: 1px solid #30363d;
            padding: 6px 13px;
        }
        .markdown-body table tr {
            background-color: #0d1117;
            border-top: 1px solid #21262d;
        }
        .markdown-body table tr:nth-child(2n) {
            background-color: #161b22;
        }

        /* Scrollbars dunkel machen (Webkit spezifisch) */
        ::-webkit-scrollbar { width: 10px; height: 10px; }
        ::-webkit-scrollbar-track { background: #0d1117; }
        ::-webkit-scrollbar-thumb { background: #30363d; border-radius: 5px; }
        ::-webkit-scrollbar-thumb:hover { background: #8b949e; }
    )";
      }

// Konstante Annäherung an eine Zeilenhöhe (Base Font 16px * 1.5 Line-Height = 24px)
static constexpr int SCROLL_LINE_HEIGHT = 30; // Etwas mehr für bessere Lesbarkeit

//---------------------------------------------------------
//   executeScroll
//---------------------------------------------------------

void MarkdownWebView::executeScroll(int _pixelsY) {
      // Wir nutzen 'window.scrollBy' mit 'smooth' behavior für weiche Animation
      QString _js = QString::fromStdString(std::format("window.scrollBy({{ top: {}, left: 0, behavior: 'smooth' }});", _pixelsY));

      page()->runJavaScript(_js);
      }

//---------------------------------------------------------
//   scrollLineUp
//---------------------------------------------------------

void MarkdownWebView::scrollLineUp() {
      executeScroll(-SCROLL_LINE_HEIGHT);
      }

//---------------------------------------------------------
//   scrollLineDown
//---------------------------------------------------------

void MarkdownWebView::scrollLineDown() {
      executeScroll(SCROLL_LINE_HEIGHT);
      }

//---------------------------------------------------------
//   scrollPageUp
//---------------------------------------------------------

void MarkdownWebView::scrollPageUp() {
      // window.innerHeight gibt die Höhe des sichtbaren Bereichs zurück
      // Wir nehmen 0.9 davon, damit man beim Lesen den Anschluss nicht verliert (Overlap)
      page()->runJavaScript("window.scrollBy({ top: -window.innerHeight * 0.9, left: 0, behavior: 'smooth' });");
      }

//---------------------------------------------------------
//   scrollPageDown
//---------------------------------------------------------

void MarkdownWebView::scrollPageDown() {
      page()->runJavaScript("window.scrollBy({ top: window.innerHeight * 0.9, left: 0, behavior: 'smooth' });");
      }

//---------------------------------------------------------
//   scrollToTop
//---------------------------------------------------------

void MarkdownWebView::scrollToTop() {
      page()->runJavaScript("window.scrollTo({ top: 0, behavior: 'smooth' });");
      }

//---------------------------------------------------------
//   scrollToBottom
//---------------------------------------------------------

void MarkdownWebView::scrollToBottom() {
      page()->runJavaScript("window.scrollTo({ top: document.body.scrollHeight, behavior: 'smooth' });");
      }
