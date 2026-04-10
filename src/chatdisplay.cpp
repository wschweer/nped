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

#include "chatdisplay.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QApplication>

//---------------------------------------------------------
//   getHighlightJsCss
//---------------------------------------------------------

QString ChatDisplay::getHighlightJsCss() const {
      return R"(
pre code.hljs{display:block;overflow-x:auto;padding:1em}code.hljs{padding:3px 5px}.hljs{color:#24292e;background:#fff})";
      }

//---------------------------------------------------------
//   getHighlightJsDarkCss
//---------------------------------------------------------

QString ChatDisplay::getHighlightJsDarkCss() const {
//      return R"(pre code.hljs{display:block;overflow-x:auto;padding:1em}code.hljs{padding:3px 5px}.hljs{color:#c9d1d9;background:#0d1117})";
      // blue codeblock
      return R"(pre code.hljs{display:block;overflow-x:auto;padding:1em}code.hljs{padding:3px 5px}.hljs{color:#c9d1d9;background:#313144})";
      }

//---------------------------------------------------------
//   getChatCss
//        code     {{ background-color: rgba(128,128,128,0.2); border-radius: 4px; padding: 2px 4px; }}
//        .code-body pre {{ margin: 0; padding: 10px; overflow: visible; background: #f6f8fa; border: none; border-radius: 0; }}
//---------------------------------------------------------

QString ChatDisplay::getChatCss() const {
      auto md = _editor->textStyle(TextStyle::Normal);
      std::string fg = md.fg.name().toStdString();
      std::string bg = md.bg.name().toStdString();

      return QString::fromStdString(std::format(R"(
        body {{ font-family: 'Inter', sans-serif; background: {0}; color: {1}; line-height: 1.5; margin: 0; padding: 4px 8px; box-sizing: border-box; }}
        #chat-container {{ display: flex; flex-direction: column; width: 100%; box-sizing: border-box; }}
        .message {{ padding: 12px 16px; margin: 6px 0; border-radius: 12px; max-width: 96%; box-sizing: border-box; word-wrap: break-word; }}
        .ai      {{ background: rgba(0,0,0,0.05); border-left: 4px solid {1}; align-self: flex-start; width: 100%; }}
        .user    {{ background: rgba(0,0,0,0.1); border-right: 4px solid {1}; align-self: flex-end; width: 96%; }}
        code     {{ background-color: rgba(100,100,100,0.5); border-radius: 4px; padding: 2px 4px; }}
                  /* Tabellen Styling */
        table {{ border-collapse: collapse; width: 100%; margin: 10px 0; }}
        th, td {{ border: 1px solid #d0d7de; padding: 8px; text-align: left; }}
        th {{ background: #f6f8fa; }}
        /* Code-Container (erzeugt von renderMarkdownToHtml) */
        .code-container {{ border: 1px solid #d0d7de; border-radius: 6px; margin: 10px 0; display: block; }}
        .code-header {{ background: #f6f8fa; padding: 5px 10px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #d0d7de; font-size: 0.8em; font-family: sans-serif; }}
        .code-body {{ max-height: 400px; overflow-y: auto; overflow-x: auto; display: block; }}
        .code-body pre {{ margin: 0; padding: 10px; overflow: visible; background: #f6f8fa; border: none; border-radius: 0; }}
        .copy-btn {{ cursor: pointer; background: #fff; border: 1px solid #d0d7de; border-radius: 4px; padding: 3px 6px; font-size: 0.9em; display: inline-flex; align-items: center; justify-content: center; line-height: 1; }}
        .copy-btn:hover {{ background: #f0f0f0; }}
        /* Fallback für einfaches pre/code ohne Wrapper */
        pre {{ background: #f6f8fa; padding: 12px; border-radius: 6px; overflow-x: auto; border: 1px solid #d0d7de; }}
        code {{ font-family: 'Fira Code', monospace; font-size: 0.9em; }}
        .thought-box {{
          background-color: #f6f8fa;
          border: 1px solid #d0d7de;
          border-radius: 8px;
          margin-bottom: 10px;
          font-size: 0.9em;
          color: #555;
          }}
        .thought-box summary {{
          padding: 8px;
          cursor: pointer;
          font-weight: bold;
          color: #0969da;
          outline: none;
          }}
       .thought-content {{
          padding: 8px 12px;
          border-top: 1px solid #d0d7de;
          font-style: italic;
          }}
    )", bg, fg));
      }

//---------------------------------------------------------
//   getChatDarkCss
//        code {{ background-color: rgba(255,255,255,0.1); border-radius: 4px; padding: 2px 4px; }}
//---------------------------------------------------------

QString ChatDisplay::getChatDarkCss() const {
      auto md = _editor->textStyle(TextStyle::Normal);
      std::string fg = md.fg.name().toStdString();
      std::string bg = md.bg.name().toStdString();
      return QString::fromStdString(std::format(R"(
        body {{ font-family: 'Inter', sans-serif; background: {0}; color: {1}; line-height: 1.5; margin: 0; padding: 4px 8px; box-sizing: border-box; }}
        #chat-container {{ display: flex; flex-direction: column; width: 100%; box-sizing: border-box; }}
        .message {{ padding: 12px 16px; margin: 6px 0; border-radius: 12px; max-width: 96%; box-sizing: border-box; }}
        .ai      {{ background: rgba(255,255,255,0.05); border-left: 4px solid {1}; align-self: flex-start; width: 100%; }}
        .user    {{ background: rgba(255,255,255,0.1); border-right: 4px solid {1}; align-self: flex-end; width: 96%; }}
        code {{ background-color: rgba(230,230,230,0.15); border-radius: 10px; padding: 2px 4px; }}
    )",  bg, fg));
      }

 //---------------------------------------------------------
//   setup
//---------------------------------------------------------

void ChatDisplay::setup() {
      QString hljsCss = _darkMode ? getHighlightJsDarkCss() : getHighlightJsCss();
      QString chatCss = _darkMode ? getChatDarkCss() : getChatCss();

      auto html = QString(
                      R"(<!DOCTYPE html>
<html>
<head>
      <meta charset="utf-8">
      <meta name="color-scheme" content="dark light">
      <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
    <style id="hljs-theme">
%1
    </style>
    <style id="chat-theme">
%2
    </style>
</head>
<body>
      <div id="chat-container"></div>
      %3
      %4
      <script>
      function fallbackCopy(text) {
            const ta = document.createElement('textarea');
            ta.value = text;
            ta.style.position = 'fixed';
            ta.style.opacity = '0';
            document.body.appendChild(ta);
            ta.focus();
            ta.select();
            try { document.execCommand('copy'); } catch(e) {}
            document.body.removeChild(ta);
      }

      function copyCode(btn) {
            const codeBlock = btn.parentElement.nextElementSibling.querySelector('code');
            const text = codeBlock.innerText;
            const originalHTML = btn.innerHTML;
            const ok = () => { btn.innerHTML = '&#10003;'; setTimeout(() => btn.innerHTML = originalHTML, 2000); };
            if (navigator.clipboard) {
                  navigator.clipboard.writeText(text).then(ok).catch(() => { fallbackCopy(text); ok(); });
            } else {
                  fallbackCopy(text);
                  ok();
            }
      }

      let currentStreamingMessage = null;

      function startNewStreamingMessage(role) {
            const isScrolledToBottom = Math.ceil(window.innerHeight + window.scrollY) >= document.body.scrollHeight - 50;
            const container = document.getElementById('chat-container');
            const wrapper = document.createElement('div');

            wrapper.className = 'message ' + (role === 'User' || role === 'user'
               || role === 'tool' || role === 'function' ? 'user' : 'ai');

            // Eindeutige ID für dieses Streaming-Event
            const msgId = 'msg-' + Date.now();
            wrapper.id = msgId;

            wrapper.innerHTML = `
        <strong style="display: block; margin-bottom: 5px;">${role}:</strong>
        <details class="thought-box" id="${msgId}-thought-container" style="display:none;">
            <summary style="color: #0078d4; cursor: pointer; font-weight: bold;">
                Reasoning...
            </summary>
            <div class="thought-content" id="${msgId}-thought-body" style="padding: 8px; border-left: 2px solid #333; font-style: italic;">
            </div>
        </details>
        <div class="message-text" id="${msgId}-text-body"></div>
    `;

            container.appendChild(wrapper);
            currentStreamingMessage = msgId;
            if (isScrolledToBottom) {
                  window.scrollTo(0, document.body.scrollHeight);
            }
            }

      function updateStreamingThought(htmlContent) {
            if (!currentStreamingMessage)
                  return;
            const isScrolledToBottom = Math.ceil(window.innerHeight + window.scrollY) >= document.body.scrollHeight - 50;
            const container = document.getElementById(currentStreamingMessage + '-thought-container');
            const body = document.getElementById(currentStreamingMessage + '-thought-body');

            if (htmlContent && htmlContent.trim() !== "") {
                  container.style.display = 'block';
                  body.innerHTML = htmlContent;
                  // Highlighting nur, wenn nötig
                  if (htmlContent.includes('</code>')) {
                        if (typeof hljs !== 'undefined') hljs.highlightAll();
                        }
                  }
            if (isScrolledToBottom) {
                  window.scrollTo(0, document.body.scrollHeight);
            }
            }
      function updateStreamingText(htmlContent) {
            if (!currentStreamingMessage)
                  return;
            const isScrolledToBottom = Math.ceil(window.innerHeight + window.scrollY) >= document.body.scrollHeight - 50;
            const body = document.getElementById(currentStreamingMessage + '-text-body');
            body.innerHTML = htmlContent;

            // Code-Highlighting nur triggern, wenn ein Code-Block abgeschlossen scheint (Performance)
            if (htmlContent.includes('</code>')) {
                  if (typeof hljs !== 'undefined') hljs.highlightAll();
                  }
            if (typeof renderMathInElement === 'function') {
                  renderMathInElement(document.body, {
                        delimiters: [
                              {left: '$$', right: '$$', display: true},
                              {left: '$', right: '$', display: false},
                              {left: '\\(', right: '\\)', display: false},
                              {left: '\\[', right: '\\]', display: true}
                        ],
                        throwOnError: false
                  });
            }
            if (htmlContent.includes('class="mermaid"')) {
                  if (typeof window.runMermaid === 'function') {
                        window.runMermaid();
                  }
            }
            if (isScrolledToBottom) {
                  window.scrollTo(0, document.body.scrollHeight);
            }
            }

      function appendStaticMessage(role, htmlContent, thoughtHtml, isActive) {
            const isScrolledToBottom = Math.ceil(window.innerHeight + window.scrollY) >= document.body.scrollHeight - 50;
            const container = document.getElementById('chat-container');
            const wrapper = document.createElement('div');
            let baseClass = 'message ' + (role === 'User' || role === 'user' || role === 'tool' || role === 'function' ? 'user' : 'ai');
            if (isActive === false) baseClass += ' inactive-message';
            wrapper.className = baseClass;

            let thoughtHtmlPart = "";
            if (thoughtHtml && thoughtHtml.trim() !== "") {
                thoughtHtmlPart = `
                   <details class="thought-box" open>
                       <summary style="color: #0078d4; cursor: pointer; font-weight: bold;">Thought:</summary>
                       <div class="thought-content" style="padding: 8px; border-left: 2px solid #333; font-style: italic;">${thoughtHtml}</div>
                   </details>
                `;
            }

            wrapper.innerHTML = `
                   <strong style="display: block; margin-bottom: 5px;">${role}:</strong>
                   ${thoughtHtmlPart}
                   <div class="message-text">${htmlContent}</div>
                `;
            container.appendChild(wrapper);
            if (typeof hljs !== 'undefined') hljs.highlightAll();
            if (typeof renderMathInElement === 'function') {
                  renderMathInElement(document.body, {
                        delimiters: [
                              {left: '$$', right: '$$', display: true},
                              {left: '$', right: '$', display: false},
                              {left: '\\(', right: '\\)', display: false},
                              {left: '\\[', right: '\\]', display: true}
                        ],
                        throwOnError: false
                  });
            }
            if (htmlContent.includes('class="mermaid"')) {
                  if (typeof window.runMermaid === 'function') {
                        window.runMermaid();
                  }
            }
            if (isScrolledToBottom) {
                  window.scrollTo(0, document.body.scrollHeight);
            }
            }

      </script>
      </body>
</html>
)")
                      .arg(hljsCss, chatCss, getMermaidJs(_darkMode), getKaTexJs());
      setHtml(html);
      }

//---------------------------------------------------------
//   setDarkMode
//---------------------------------------------------------

void ChatDisplay::setDarkMode(bool enabled) {
      if (_darkMode == enabled)
            return;
      _darkMode = enabled;
      // settings()->setAttribute(QWebEngineSettings::ForceDarkMode, _darkMode);

      QString js = "(function() { "
                   "  var el1 = document.getElementById('hljs-theme'); "
                   "  if (el1) el1.textContent = " +
                   quoteForJs(_darkMode ? getHighlightJsDarkCss() : getHighlightJsCss()) +
                   "; "
                   "  var el2 = document.getElementById('chat-theme'); "
                   "  if (el2) el2.textContent = " +
                   quoteForJs(_darkMode ? getChatDarkCss() : getChatCss()) +
                   "; "
                   "  var mermaidStyles = document.querySelectorAll('.mermaid'); "
                   "  mermaidStyles.forEach(function(el) { "
                   "      el.style.backgroundColor = '" +
                   (_darkMode ? "#161b22" : "#f6f8fa") +
                   "'; "
                   "  }); "
                   "})();";

      page()->runJavaScript(js, [](const QVariant& res) { (void)res; });
      }

//---------------------------------------------------------
//   startMessage
//---------------------------------------------------------

void ChatDisplay::startMessage() {
      currentStreamingThought.clear();
      currentStreamingText.clear();
      auto s = std::format("startNewStreamingMessage('{}');", role);
      page()->runJavaScript(QString::fromStdString(s));
      }

//---------------------------------------------------------
//   handleIncomingChunk
//---------------------------------------------------------

void ChatDisplay::handleIncomingChunk(const std::string& thoughtChunk, const std::string& textChunk) {
      // Logic fix: Only start message if there is actual content
      if (mustStartMessage && (!thoughtChunk.empty() || !textChunk.empty())) {
            mustStartMessage = false;
            startMessage();
            }

      if (!thoughtChunk.empty()) {
            currentStreamingThought += thoughtChunk;
            QString html             = renderMarkdownToHtml(currentStreamingThought);

            QString js = QString("updateStreamingThought(%1);").arg(quoteForJs(html));
            page()->runJavaScript(js);
            }

      if (!textChunk.empty()) {
            currentStreamingText += textChunk;
            QString html          = renderMarkdownToHtml(currentStreamingText);

            // JS Aufruf
            QString js = QString("updateStreamingText(%1);").arg(quoteForJs(html));
            page()->runJavaScript(js);
            }
      }

//---------------------------------------------------------
//   quoteForJs
//---------------------------------------------------------

QString ChatDisplay::quoteForJs(const QString& str) {
      if (str.isEmpty())
            return "''";
      QByteArray json = QJsonDocument(QJsonArray{str}).toJson(QJsonDocument::Compact);
      return QString::fromUtf8(json.mid(1, json.length() - 2));
      }

//---------------------------------------------------------
//   appendStaticHtml
//---------------------------------------------------------

void ChatDisplay::appendStaticHtml(const QString& role, const QString& html, const QString& thoughtHtml, bool isActive) {
      QString js =
          QString("if(typeof appendStaticMessage === 'function') { appendStaticMessage(%1, %2, %3, %4); } else { "
                  "  setTimeout(function() { if(typeof appendStaticMessage === 'function') { appendStaticMessage(%1, %2, %3, %4); } "
                  "else { console.error('appendStaticMessage not found!'); } }, 100);"
                  "}")
              .arg(quoteForJs(role), quoteForJs(html), quoteForJs(thoughtHtml), isActive ? "true" : "false");
      page()->runJavaScript(js, [](const QVariant& res) { (void)res; });
      }
