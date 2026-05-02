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
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QWebEngineSettings>
#include <QJsonDocument>
#include <QJsonObject>

#include "webview.h"
#include "kontext.h"
#include "editor.h"

#include <QDesktopServices>
#include <QWebEnginePage>
#include <QPrintDialog>
#include <QPrinter>
#include <QScopedPointer>
#include <QTimer>

//---------------------------------------------------------
//   MarkdownWebPage
//---------------------------------------------------------

MarkdownWebPage::MarkdownWebPage(Editor* e, QObject* parent) : QWebEnginePage(parent), _editor(e) {
      }

//---------------------------------------------------------
//   acceptNavigationRequest
//---------------------------------------------------------

bool MarkdownWebPage::acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) {
      // Handle nped:// custom scheme for JS → C++ communication
      if (url.scheme() == "nped") {
            if (url.host() == "copy-code") {
                  runJavaScript(QStringLiteral("window._npedCopyBuffer || ''"),
                                [this](const QVariant& result) {
                                      QString text = result.toString();
                                      if (!text.isEmpty() && _editor)
                                            _editor->setPickText(text, SelectionMode::CharSelect);
                                      });
                  }
            return false;
            }

      if (type == QWebEnginePage::NavigationTypeLinkClicked) {
            if (url.scheme() == "file") {
                  QString path = url.toLocalFile();
                  // Check if it's an anchor link within the same page
                  if (!url.fragment().isEmpty()) {
                        // Let the view handle anchor links if they are for the same file
                        if (_editor && _editor->kontext() && _editor->kontext()->file() &&
                            path == _editor->kontext()->file()->path())
                              return true;
                        }

                  // For local files, open in Editor instead
                  if (_editor) {
                        QTimer::singleShot(0, _editor, [e = _editor, path]() {
                              Kontext* k = e->addFile(path);
                              if (k) {
                                    e->setCurrentKontext(k);
                                    QFileInfo fi(path);
                                    QString ext = fi.suffix().toLower();
                                    if (ext == "md" || ext == "markdown" || ext == "jpg" || ext == "jpeg" ||
                                        ext == "png" || ext == "gif" || ext == "svg" || ext == "webp" ||
                                        ext == "html" || ext == "htm") {
                                          if (k->viewMode() != ViewMode::WebView)
                                                e->setViewMode(ViewMode::WebView);
                                          }
                                    }
                              });
                        }
                  return false;
                  }
            }
      return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
      }

//---------------------------------------------------------
//   createWindow
//---------------------------------------------------------

QWebEngineView* MarkdownWebView::createWindow(QWebEnginePage::WebWindowType /*type*/) {
      return this;
      }

//---------------------------------------------------------
//   MarkdownWebView
//---------------------------------------------------------

MarkdownWebView::MarkdownWebView(Editor* e, QWidget* _parent) : QWebEngineView(_parent), _editor(e) {
      setPage(new MarkdownWebPage(e, this));
      settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
      settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
      settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
      settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
      if (!settings()->testAttribute(QWebEngineSettings::PdfViewerEnabled))
            Warning("PDF-Viewer konnte in QWebEngineSettings nicht aktiviert werden!");
      _darkMode = e->darkMode();
      // Set initial background color to prevent white flash
      page()->setBackgroundColor(_darkMode ? QColor("#222222") : Qt::white);
      connect(page(), &QWebEnginePage::printRequested, this, [this]() {
            QPrinter* printer = new QPrinter(QPrinter::HighResolution);
            QPrintDialog printDialog(printer, this);
            printDialog.setWindowTitle(tr("Dokument drucken"));
            if (printDialog.exec() == QDialog::Accepted) {
                  connect(
                      this, &QWebEngineView::printFinished, this,
                      [printer](bool success) {
                            if (!success)
                                  qWarning() << "Fehler beim Drucken aufgetreten.";
                            delete printer;
                            },
                      Qt::SingleShotConnection);
                  this->print(printer);
                  }
            else {
                  delete printer;
                  }
            });

      textActions = {
         Action(e->getSC(Cmd::CMD_QUIT), [this] { _editor->quitCmd(); }),
         Action(e->getSC(Cmd::CMD_SAVE_QUIT), [this] { _editor->saveQuitCmd(); }),

         Action(e->getSC(Cmd::CMD_LINE_UP), [this] { scrollLineUp(); }),
         Action(e->getSC(Cmd::CMD_LINE_DOWN), [this] { scrollLineDown(); }),
         Action(e->getSC(Cmd::CMD_FILE_BEGIN), [this] { scrollToTop(); }),
         Action(e->getSC(Cmd::CMD_FILE_END), [this] { scrollToBottom(); }),
         Action(e->getSC(Cmd::CMD_PAGE_UP), [this] { scrollPageUp(); }),
         Action(e->getSC(Cmd::CMD_PAGE_DOWN), [this] { scrollPageDown(); }),
         Action(e->getSC(Cmd::CMD_LINK_BACK), [this] { back(); }),
         Action(e->getSC(Cmd::CMD_LINK_FORWARD), [this] { forward(); }),
#if 0
         Action(e->getSC(Cmd::CMD_LINE_TOP), [] { }),
         Action(e->getSC(Cmd::CMD_LINE_BOTTOM), [] {}),
#endif
         Action(e->getSC(Cmd::CMD_ENTER), [this] { _editor->enterCmd(); }),
         Action(e->getSC(Cmd::CMD_SAVE), [this] { _editor->kontext()->file()->save(); }),
         Action(e->getSC(Cmd::CMD_KONTEXT_COPY), [this] { _editor->copyKontext(); }),
         Action(e->getSC(Cmd::CMD_KONTEXT_PREV), [this] { _editor->prevKontext(); }),
         Action(e->getSC(Cmd::CMD_KONTEXT_NEXT), [this] { _editor->nextKontext(); }),

         Action(e->getSC(Cmd::CMD_KONTEXT_UP),
                [this] { _editor->kontext()->moveCursorRel(0, -1, MoveType::Roll); }),
         Action(e->getSC(Cmd::CMD_KONTEXT_DOWN),
                [this] { _editor->kontext()->moveCursorRel(0, 1, MoveType::Roll); }),
         Action(e->getSC(Cmd::CMD_PICK), [] {}),
         Action(e->getSC(Cmd::CMD_PUT), [this] { _editor->put(); }),
         Action(e->getSC(Cmd::CMD_SELECT_ROW), [] {}),
         Action(e->getSC(Cmd::CMD_SELECT_COL), [] {}),
         Action(e->getSC(Cmd::CMD_VIEW_FUNCTIONS), [this] { _editor->toggleViewMode(); }),
         Action(e->getSC(Cmd::CMD_SEARCH_NEXT), [] {}),
         Action(e->getSC(Cmd::CMD_SEARCH_PREV), [] {}),
         Action(e->getSC(Cmd::CMD_TOGGLE_GIT),
                [this] { _editor->gitButton()->setChecked(!_editor->gitButton()->isChecked()); }),
         Action(e->getSC(Cmd::CMD_SCREENSHOT), [this] { _editor->screenshot(); }),
            };

      kl = new KeyLogger(&textActions, this);
      connect(kl, &KeyLogger::triggered, [this](Action* a) {
            _editor->startCmd();
            a->func();
            _editor->endCmd();
            });
      connect(kl, &KeyLogger::keyLabelChanged, [this](QString s) { _editor->keyLabel()->setText(s); });
      installEventFilter(kl);
      connect(this, &QWebEngineView::loadFinished, this, [this] {
            isLoaded = true;
            if (!_pendingDiff.isEmpty() && url().toString().contains("diff.html")) {
                  showGitDiff(_pendingDiff);
                  _pendingDiff.clear();
                  }
            });
      }

//---------------------------------------------------------
//   childEvent
//---------------------------------------------------------

void MarkdownWebView::childEvent(QChildEvent* event) {
      if (event->type() == QEvent::ChildAdded) {
            QObject* child = event->child();
            if (child && kl)
                  child->installEventFilter(kl);
            }
      QWebEngineView::childEvent(event);
      }

//---------------------------------------------------------
//   installFilterOnProxy
//---------------------------------------------------------

void MarkdownWebView::installFilterOnProxy() {
      if (focusProxy())
            focusProxy()->installEventFilter(kl);
      this->installEventFilter(kl);
      }

//---------------------------------------------------------
//   setHtml
//---------------------------------------------------------

void MarkdownWebView::setHtml(const QString& _html, const QUrl& _baseUrl) {
      isLoaded     = false;
      QUrl baseUrl = _baseUrl;
      if (baseUrl.isEmpty() && _editor->kontext() && _editor->kontext()->file()) {
            QFileInfo fi(_editor->kontext()->file()->path());
            baseUrl = QUrl::fromLocalFile(fi.absoluteDir().absolutePath() + "/");
            }
      QWebEngineView::setHtml(_html, baseUrl);
      }

//---------------------------------------------------------
//   setDarkMode
//---------------------------------------------------------

void MarkdownWebView::setDarkMode(bool enabled) {
      if (_darkMode == enabled)
            return;
      _darkMode = enabled;
      page()->setBackgroundColor(_darkMode ? QColor("#222222") : Qt::white);
      if (!_currentRawMarkdown.isEmpty() && !url().toString().contains("diff.html"))
            setMarkdown(_currentRawMarkdown);
      else if (url().toString().contains("diff.html") && !_currentDiff.isEmpty())
            showGitDiff(_currentDiff);
      showGitDiff(_currentDiff);
      }

//---------------------------------------------------------
//   setMarkdown
//---------------------------------------------------------

void MarkdownWebView::setMarkdown(const QString& _markdown, int cursorLine) {
      if (cursorLine < 0 && _markdown == _currentRawMarkdown)
            return;
      _currentRawMarkdown = _markdown;
      if (_markdown.isEmpty()) {
            setHtml("");
            return;
            }

      QString _processedMarkdown = _markdown;
      if (_processedMarkdown.contains("[TOC]"))
            _processedMarkdown.replace("[TOC]", "\n<div id='table-of-contents'></div>\n");

      if (cursorLine >= 0) {
            QStringList lines = _processedMarkdown.split('\n');
            if (cursorLine < lines.size()) {
                  QString& line = lines[cursorLine];
                  QRegularExpression blockMarkersRe(
                      "^(\\s*(?:#{1,6}\\s+|[-*+]\\s+|\\d+\\.\\s+|>+\\s*))(.*)$");
                  if (auto match = blockMarkersRe.match(line); match.hasMatch())
                        line = match.captured(1) + "<span id=\"nped-cursor-pos\"></span>" + match.captured(2);
                  else if (line.trimmed().startsWith("```"))
                        lines.insert(cursorLine, "<span id=\"nped-cursor-pos\"></span>");
                  else
                        line.prepend("<span id=\"nped-cursor-pos\"></span>");
                  _processedMarkdown = lines.join('\n');
                  }
            }

      QString _convertedHtml    = renderMarkdownToHtml(_processedMarkdown.toStdString());
      auto _css                 = _darkMode ? getGithubDarkCss() : getGithubCss();
      _css                     += getScrollbarCss(_darkMode).toStdString();
      QString _highlightAssets  = getHighlightJsAssets(_darkMode);
      auto _anchorScript        = getAnchorJs();
      auto _tocScript           = getTocJs();
      QString _mermaidScript    = getMermaidJs(_darkMode);
      QString _katexScript      = getKaTexJs();

      QString _scrollScript;
      if (cursorLine >= 0) {
            _scrollScript = R"(
                <script>
                window.addEventListener('load', function() {
                    var el = document.getElementById('nped-cursor-pos');
                    if (el) { el.scrollIntoView({behavior: 'instant', block: 'center'}); }
                });
                </script>
            )";
            }

      QString _fullHtml = QString::fromStdString(std::format(
          R"(<!DOCTYPE html>
        <html>
        <head>
            <meta charset="utf-8">
            <meta name="color-scheme" content="{}">
            <style>{}</style>
            {}
        </head>
        <body class="markdown-body">
            {}
            {} {} {} {} {} </body>
        </html>)",
          _darkMode ? "dark" : "light", _css, _highlightAssets.toStdString(), _convertedHtml.toStdString(),
          _anchorScript, _tocScript, _mermaidScript.toStdString(), _katexScript.toStdString(),
          _scrollScript.toStdString()));

      QUrl baseUrl;
      if (_editor && _editor->kontext() && _editor->kontext()->file()) {
            QFileInfo fi(_editor->kontext()->file()->path());
            baseUrl = QUrl::fromLocalFile(fi.absoluteDir().absolutePath() + "/");
            }
      setHtml(_fullHtml, baseUrl);
      }

//---------------------------------------------------------
//   getScrollbarCss
//---------------------------------------------------------

QString MarkdownWebView::getScrollbarCss(bool darkMode) const {
      if (darkMode) {
            return R"(
            ::-webkit-scrollbar { width: 12px; height: 12px; }
            ::-webkit-scrollbar-track { background: #1e1e1e; }
            ::-webkit-scrollbar-thumb { background: #555; border: 2px solid #1e1e1e; border-radius: 6px; }
            ::-webkit-scrollbar-thumb:hover { background: #777; }
            ::-webkit-scrollbar-corner { background: #1e1e1e; }
            )";
            }
      else {
            return R"(
            ::-webkit-scrollbar { width: 12px; height: 12px; }
            ::-webkit-scrollbar-track { background: #ffffff; }
            ::-webkit-scrollbar-thumb { background: #c1c1c1; border: 2px solid #ffffff; border-radius: 6px; }
            ::-webkit-scrollbar-thumb:hover { background: #a8a8a8; }
            ::-webkit-scrollbar-corner { background: #ffffff; }
            )";
            }
      }

//---------------------------------------------------------
//   getHighlightJsAssets
//---------------------------------------------------------

QString MarkdownWebView::getHighlightJsAssets(bool darkMode) const {
      QString theme  = darkMode ? "github-dark.min.css" : "github.min.css";
      QString js     = R"(
        <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/)";
      js            += theme;
      js            += R"(">
        <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
        <script>
            document.addEventListener('DOMContentLoaded', (event) => { if (typeof hljs !== 'undefined') hljs.highlightAll(); });
            function fallbackCopy(text) {
                const ta = document.createElement('textarea');
                ta.value = text; ta.style.position = 'fixed'; ta.style.opacity = '0';
                document.body.appendChild(ta); ta.focus(); ta.select();
                try { document.execCommand('copy'); } catch(e) {}
                document.body.removeChild(ta);
                  }
            function notifyEditor(text) {
                window._npedCopyBuffer = text;
                const a = document.createElement('a');
                a.href = 'nped://copy-code';
                a.style.display = 'none';
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                  }
            function copyCode(btn) {
                const codeBlock = btn.parentElement.nextElementSibling.querySelector('code');
                const text = codeBlock.innerText;
                const originalHTML = btn.innerHTML;
                const ok = () => {
                    btn.innerHTML = '&#10003;';
                    setTimeout(() => btn.innerHTML = originalHTML, 2000);
                    notifyEditor(text);
                    };
                if (navigator.clipboard) { navigator.clipboard.writeText(text).then(ok).catch(() => { fallbackCopy(text); ok(); });
                      } else { fallbackCopy(text); ok(); }
                  }
        </script>
    )";
      return js;
      }

//---------------------------------------------------------
//   getKaTexJs
//---------------------------------------------------------

QString MarkdownWebView::getKaTexJs() const {
      return QString::fromStdString(R"HTML(
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/katex@0.16.8/dist/katex.min.css" crossorigin="anonymous">
<script defer src="https://cdn.jsdelivr.net/npm/katex@0.16.8/dist/katex.min.js" crossorigin="anonymous"></script>
<script defer src="https://cdn.jsdelivr.net/npm/katex@0.16.8/dist/contrib/auto-render.min.js" crossorigin="anonymous"
    onload="renderMathInElement(document.body, {
        delimiters: [ {left: '$$', right: '$$', display: true}, {left: '$', right: '$', display: false}, {left: '\\(', right: '\\)', display: false}, {left: '\\[', right: '\\]', display: true} ],
        throwOnError: false
                                                    });"></script>)HTML");
      }

//---------------------------------------------------------
//   getMermaidJs
//---------------------------------------------------------

QString MarkdownWebView::getMermaidJs(bool darkMode) const {
      QString js  = R"(
        <script type="module">
          import mermaid from 'https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.esm.min.mjs';
          mermaid.initialize({ startOnLoad: false, theme: ')";
      js         += darkMode ? "dark" : "default";
      js         += R"(', themeVariables: { fontFamily: 'inherit', background: 'transparent' } });
          window.runMermaid = async () => { try { await mermaid.run({ querySelector: '.mermaid', suppressErrors: true }); } catch (e) {} };
          document.addEventListener('DOMContentLoaded', async () => { await window.runMermaid(); });
        </script>
        <style> .mermaid { background-color: transparent; } </style>
      )";
      return js;
      }

//---------------------------------------------------------
//   md4c_callback
//---------------------------------------------------------

void md4c_callback(const MD_CHAR* _data, MD_SIZE _size, void* _userData) {
      static_cast<QString*>(_userData)->append(QString::fromUtf8(_data, _size));
      }

//---------------------------------------------------------
//   renderMarkdownToHtml
//---------------------------------------------------------

QString MarkdownWebView::renderMarkdownToHtml(const std::string& _stdMarkdown) {
      QString _output;

      // GFM-Flags: Tabellen, Tasklisten, Durchgestrichen, Autolinks
      // unsigned int _flags = MD_DIALECT_GITHUB | MD_FLAG_NOINDENTEDCODEBLOCKS;
      unsigned int _flags = MD_DIALECT_GITHUB;
      int _result = md_html(_stdMarkdown.c_str(), static_cast<MD_SIZE>(_stdMarkdown.size()), md4c_callback,
                            &_output, _flags, 0);
      if (_result != 0) {
            Critical("Markdown conversion failed with code: {}", _result);
            return "<b>Error: Markdown rendering failed.</b>";
            }

      // Convert links to JPGs into image tags
      QRegularExpression imgRe("<a href=\"([^\"]+\\.jpe?g)\"[^>]*>([^<]*)</a>",
                               QRegularExpression::CaseInsensitiveOption);
      _output.replace(imgRe, "<img src=\"\\1\" alt=\"\\2\" style=\"max-width: 100%;\" />");

      // Code-Block Wrapping (mit weniger strengem Regex)
      QRegularExpression re("<pre><code( class=\"language-(.*?)\")?>(.*?)</code></pre>",
                            QRegularExpression::DotMatchesEverythingOption);

      int offset = 0;
      QString result;
      QRegularExpressionMatch match;
      while ((match = re.match(_output, offset)).hasMatch()) {
            result       += _output.mid(offset, match.capturedStart() - offset);
            QString lang  = match.captured(2).isEmpty() ? "code" : match.captured(2);
            QString codeClass =
                match.captured(2).isEmpty() ? "language-plaintext" : "language-" + match.captured(2);
            QString code = match.captured(3);

            if (lang.toLower() == "mermaid") {
                  // Gib Mermaid-Blöcke unverändert (aber als div class=\"mermaid\") aus, damit sie gerendert werden können.
                  // Wir dekorieren sie nicht mit Copy-Button oder Code-Header.

                  result += QString(R"X(<div class="mermaid">%1</div>)X").arg(code);
                  }
            else {
                  result += QString(R"X(
<div class="code-container">
   <div class="code-header">
     <span>%1</span>
     <button class="copy-btn" onclick="copyCode(this)" title="Copy"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M0 6.75C0 5.784.784 5 1.75 5h1.5a.75.75 0 0 1 0 1.5h-1.5a.25.25 0 0 0-.25.25v7.5c0 .138.112.25.25.25h7.5a.25.25 0 0 0 .25-.25v-1.5a.75.75 0 0 1 1.5 0v1.5A1.75 1.75 0 0 1 9.25 16h-7.5A1.75 1.75 0 0 1 0 14.25Z"/><path d="M5 1.75C5 .784 5.784 0 6.75 0h7.5C15.216 0 16 .784 16 1.75v7.5A1.75 1.75 0 0 1 14.25 11h-7.5A1.75 1.75 0 0 1 5 9.25Zm1.75-.25a.25.25 0 0 0-.25.25v7.5c0 .138.112.25.25.25h7.5a.25.25 0 0 0 .25-.25v-7.5a.25.25 0 0 0-.25-.25Z"/></svg></button>
     </div>
   <div class="code-body">
      <pre><code class="%2">%3</code></pre>
      </div>
   </div>
)X")
                                .arg(lang, codeClass, code);
                  }

            offset = match.capturedEnd();
            }
      result += _output.mid(offset);
      return result;
      }

//---------------------------------------------------------
//   getGithubCss
//---------------------------------------------------------

std::string MarkdownWebView::getGithubCss() const {
      const std::string s = R"(
        body {
            color-scheme: light;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif;
            line-height: 1.5; padding: 32px; max-width: 980px; margin: 0 auto; color: #1f2328; background-color: #ffffff;
            scroll-behavior: smooth;
        }
        .code-container { border: 1px solid #d0d7de; border-radius: 6px; margin: 10px 0; display: block; }
        .code-header { background: #f6f8fa; padding: 5px 10px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #d0d7de; font-size: 0.8em; font-family: sans-serif; }
        .code-body { max-height: 400px; overflow-y: auto; overflow-x: auto; display: block; }
        .code-body pre { margin: 0; padding: 10px; overflow: visible; }
        .copy-btn { cursor: pointer; background: #fff; border: 1px solid #d0d7de; border-radius: 4px; padding: 3px 6px; font-size: 0.9em; display: inline-flex; align-items: center; justify-content: center; line-height: 1; }
        .copy-btn:hover { background: #f0f0f0; }

        .markdown-body h1, .markdown-body h2 { border-bottom: 1px solid #d8dee4; padding-bottom: .3em; margin-top: 24px; margin-bottom: 16px; font-weight: 600; }
        .markdown-body a { color: #0969da; text-decoration: none; }
        .markdown-body a:hover { text-decoration: underline; }
        .markdown-body code {
            background-color: rgba(175,184,193,0.2); border-radius: 6px; font-size: 100%; padding: 0.2em 0.4em;
            font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, "Liberation Mono", monospace;
        }
        .markdown-body pre {
            background-color: #f6f8fa; border-radius: 6px; padding: 16px; font-size: 85%; line-height: 1.45;
        }
        .markdown-body pre code {
            background-color: transparent; border: 0; display: inline; line-height: inherit; margin: 0; padding: 0;
        }
        .markdown-body table { border-spacing: 0; border-collapse: collapse; margin-top: 0; margin-bottom: 16px; width: 100%; }
        .markdown-body table th, .markdown-body table td { border: 1px solid #d0d7de; padding: 6px 13px; }
        .markdown-body table tr { background-color: #ffffff; border-top: 1px solid #hsla(210,18%,87%,1); }
        .markdown-body table tr:nth-child(2n) { background-color: #f6f8fa; }
      /* Anker Styling (GitHub Style) */
.anchor {
    float: left;
    padding-right: 4px;
    margin-left: -20px;
    line-height: 1;
    text-decoration: none;
    cursor: pointer;
    opacity: 0; /* Standardmäßig unsichtbar */
}

/* Sichtbar machen beim Hover über die Überschrift */
h1:hover .anchor, h2:hover .anchor, h3:hover .anchor,
h4:hover .anchor, h5:hover .anchor, h6:hover .anchor {
    opacity: 1;
}

.anchor:focus {
    opacity: 1;
}

/* Icon Farbe anpassen */
.anchor svg {
    fill: currentColor; /* Nimmt Textfarbe an */
}
      /* ToC Container Styling */
#table-of-contents {
    background-color: #f6f8fa;
    border: 1px solid #d0d7de;
    border-radius: 6px;
    padding: 16px;
    margin-bottom: 24px;
    width: fit-content;
    min-width: 200px;
    max-width: 100%;
}

.toc-title {
    font-weight: 600;
    margin-bottom: 8px;
    font-size: 1.2em;
}

.toc-list {
    list-style: none;
    padding: 0;
    margin: 0;
}

.toc-item {
    margin: 4px 0;
}

.toc-item a {
    text-decoration: none;
    color: #0969da;
}

.toc-item a:hover {
    text-decoration: underline;
}

/* Einrückungen für Hierarchie */
.toc-h1 { padding-left: 0px; font-weight: 600; }
.toc-h2 { padding-left: 15px; }
.toc-h3 { padding-left: 30px; font-size: 0.95em; }
.toc-h4 { padding-left: 45px; font-size: 0.9em; }
.toc-h5 { padding-left: 60px; font-size: 0.9em; font-style: italic; }
.toc-h6 { padding-left: 75px; }
    )";
      return s;
      }

//---------------------------------------------------------
//   getGithubDarkCss
//---------------------------------------------------------

std::string MarkdownWebView::getGithubDarkCss() const {
      std::string s = R"(
        body {
            color-scheme: dark;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif;
            line-height: 1.5; padding: 32px; max-width: 980px; margin: 0 auto;
            color: #c9d1d9;
            background-color: #222222;
            scroll-behavior: smooth;
        }

        .code-container { border: 1px solid #30363d; border-radius: 6px; margin: 10px 0; display: block; }
        .code-header { background: #333333; padding: 5px 10px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #30363d; font-size: 0.8em; font-family: sans-serif; }
        .code-body { max-height: 400px; overflow-y: auto; overflow-x: auto; display: block; }
        .code-body pre { margin: 0; padding: 10px; overflow: visible; }
        .copy-btn { cursor: pointer; background: #222222; border: 1px solid #30363d; border-radius: 4px; padding: 3px 6px; font-size: 0.9em; color: #c9d1d9; display: inline-flex; align-items: center; justify-content: center; line-height: 1; }
        .copy-btn:hover { background: #21262d; }

        .markdown-body h1, .markdown-body h2 {
            border-bottom: 1px solid #21262d;
            color: #e6edf3;
        }

        .markdown-body a { color: #58a6ff; text-decoration: none; }
        .markdown-body a:hover { text-decoration: underline; }

        .markdown-body code {
            background-color: rgba(110,118,129,0.4);
            border-radius: 6px;
            padding: 0.2em 0.4em;
            font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, monospace;
            color: #c9d1d9;
        }

        .markdown-body pre {
            background-color: #333333;
            border-radius: 6px;
            padding: 16px;
            border: 1px solid #30363d;
        }

        .markdown-body table th, .markdown-body table td {
            border: 1px solid #30363d;
            padding: 6px 13px;
        }
        .markdown-body table tr {
            background-color: #222222;
            border-top: 1px solid #21262d;
        }
        .markdown-body table tr:nth-child(2n) {
            background-color: #333333;
        }

        /* Anker Styling */
        .anchor {
            float: left;
            padding-right: 4px;
            margin-left: -20px;
            line-height: 1;
            text-decoration: none;
            cursor: pointer;
            opacity: 0;
        }

        h1:hover .anchor, h2:hover .anchor, h3:hover .anchor {
            opacity: 1;
        }

        .anchor svg {
            fill: currentColor;
        }

        /* ToC Container */
        #table-of-contents {
            background-color: #333333;
            border: 1px solid #30363d;
            padding: 16px;
            margin-bottom: 24px;
            border-radius: 6px;
        }

        .toc-title { color: #e6edf3; font-weight: 600; margin-bottom: 8px; }
        .toc-item a { color: #58a6ff; text-decoration: none; }
        .toc-item a:hover { text-decoration: underline; }
    )";
      return s;
      }

//---------------------------------------------------------
//   Scroll / Other
//---------------------------------------------------------

static constexpr int SCROLL_LINE_HEIGHT = 30;
void MarkdownWebView::executeScroll(int _pixelsY) {
      QString _js = QString("window.scrollBy({ top: %1, left: 0, behavior: 'smooth' });").arg(_pixelsY);
      page()->runJavaScript(_js);
      }

void MarkdownWebView::scrollLineUp() {
      executeScroll(-SCROLL_LINE_HEIGHT);
      }

void MarkdownWebView::scrollLineDown() {
      executeScroll(SCROLL_LINE_HEIGHT);
      }

void MarkdownWebView::scrollPageUp() {
      page()->runJavaScript(
          "window.scrollBy({ top: -window.innerHeight * 0.9, left: 0, behavior: 'smooth' });");
      }

void MarkdownWebView::scrollPageDown() {
      page()->runJavaScript(
          "window.scrollBy({ top: window.innerHeight * 0.9, left: 0, behavior: 'smooth' });");
      }

void MarkdownWebView::scrollToTop() {
      page()->runJavaScript("window.scrollTo({ top: 0, behavior: 'smooth' });");
      }

void MarkdownWebView::scrollToBottom() {
      page()->runJavaScript("window.scrollTo({ top: document.body.scrollHeight, behavior: 'smooth' });");
      }

const std::string& MarkdownWebView::getAnchorJs() const {
      static const std::string s =
          R"raw(<script>document.addEventListener("DOMContentLoaded", function() {
    const headings = document.querySelectorAll('h1, h2, h3, h4, h5, h6');
    headings.forEach((heading, index) => {
        if (!heading.id) {
            heading.id = 'heading-' + index;
        }
        const anchor = document.createElement('a');
        anchor.className = 'anchor';
        anchor.href = '#' + heading.id;
        anchor.innerHTML = '<svg class="octicon octicon-link" viewBox="0 0 16 16" version="1.1" width="16" height="16" aria-hidden="true"><path fill-rule="evenodd" d="M7.775 3.275a.75.75 0 001.06 1.06l1.25-1.25a2 2 0 112.83 2.83l-2.5 2.5a2 2 0 01-2.83 0 .75.75 0 00-1.06 1.06 3.5 3.5 0 004.95 0l2.5-2.5a3.5 3.5 0 00-4.95-4.95l-1.25 1.25zm-4.69 9.64a2 2 0 010-2.83l2.5-2.5a2 2 0 012.83 0 .75.75 0 001.06-1.06 3.5 3.5 0 00-4.95 0l-2.5 2.5a3.5 3.5 0 004.95 4.95l1.25-1.25a.75.75 0 00-1.06-1.06l-1.25 1.25a2 2 0 01-2.83 0z"></path></svg>';
        heading.insertBefore(anchor, heading.firstChild);
    });
      });</script>)raw";
      return s;
      }

const std::string& MarkdownWebView::getTocJs() const {
      static const std::string s =
          R"raw(<script>document.addEventListener("DOMContentLoaded", function() {
    const tocContainer = document.getElementById('table-of-contents');
    if (!tocContainer) return;

    const headings = document.querySelectorAll('h1, h2, h3, h4, h5, h6');
    if (headings.length === 0) return;

    const tocList = document.createElement('ul');
    tocList.className = 'toc-list';
    tocContainer.appendChild(tocList);

    headings.forEach((heading, index) => {
        if (!heading.id) {
            heading.id = 'heading-' + index;
        }

        const li = document.createElement('li');
        li.className = 'toc-item toc-' + heading.tagName.toLowerCase();

        const a = document.createElement('a');
        a.href = '#' + heading.id;
        a.textContent = heading.textContent;

        li.appendChild(a);
        tocList.appendChild(li);
    });
      });</script>)raw";
      return s;
      }

void MarkdownWebView::showGitDiff(const QString& diffOutput) {
      _currentDiff = diffOutput;
      if (!url().toString().contains("diff.html")) {
            _pendingDiff = diffOutput;
            setUrl(QUrl("qrc:/res/diff.html"));
            return;
            }
      // JSON-Escape für den Diff-Output
      // JSON-Escape für den Diff-Output
      QJsonDocument doc(QJsonObject {
               { "diff",                   diffOutput},
               {"theme", _darkMode ? "dark" : "light"}
            });

      QString safeScript = QString("if(typeof renderDiff === 'function') renderDiff(%1);")
                               .arg(QString::fromUtf8(doc.toJson()));
      this->page()->runJavaScript(safeScript);
      }
