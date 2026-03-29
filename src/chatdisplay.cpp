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

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QWebEngineSettings>

#include "chatdisplay.h"

//---------------------------------------------------------
//   getHighlightJsCss
//---------------------------------------------------------

QString ChatDisplay::getHighlightJsCss() const {
      return R"(
pre code.hljs{display:block;overflow-x:auto;padding:1em}code.hljs{padding:3px 5px}/*!
  Theme: GitHub
  Description: Light theme as seen on github.com
  Author: github.com
  Maintainer: @Hirse
  Updated: 2021-05-15

  Outdated base version: https://github.com/primer/github-syntax-light
  Current colors taken from GitHub's CSS
*/.hljs{color:#24292e;background:#fff}.hljs-doctag,.hljs-keyword,.hljs-meta .hljs-keyword,.hljs-template-tag,.hljs-template-variable,.hljs-type,.hljs-variable.language_{color:#d73a49}.hljs-title,.hljs-title.class_,.hljs-title.class_.inherited__,.hljs-title.function_{color:#6f42c1}.hljs-attr,.hljs-attribute,.hljs-literal,.hljs-meta,.hljs-number,.hljs-operator,.hljs-selector-attr,.hljs-selector-class,.hljs-selector-id,.hljs-variable{color:#005cc5}.hljs-meta .hljs-string,.hljs-regexp,.hljs-string{color:#032f62}.hljs-built_in,.hljs-symbol{color:#e36209}.hljs-code,.hljs-comment,.hljs-formula{color:#6a737d}.hljs-name,.hljs-quote,.hljs-selector-pseudo,.hljs-selector-tag{color:#22863a}.hljs-subst{color:#24292e}.hljs-section{color:#005cc5;font-weight:700}.hljs-bullet{color:#735c0f}.hljs-emphasis{color:#24292e;font-style:italic}.hljs-strong{color:#24292e;font-weight:700}.hljs-addition{color:#22863a;background-color:#f0fff4}.hljs-deletion{color:#b31d28;background-color:#ffeef0}
      )";
      }

//---------------------------------------------------------
//   getHighlightJsDarkCss
//---------------------------------------------------------

QString ChatDisplay::getHighlightJsDarkCss() const {
      return R"(
pre code.hljs{display:block;overflow-x:auto;padding:1em}code.hljs{padding:3px 5px}/*!
  Theme: GitHub Dark
  Description: Dark theme as seen on github.com
  Author: github.com
  Maintainer: @Hirse
  Updated: 2021-05-15

  Outdated base version: https://github.com/primer/github-syntax-dark
  Current colors taken from GitHub's CSS
*/.hljs{color:#c9d1d9;background:#0d1117}.hljs-doctag,.hljs-keyword,.hljs-meta .hljs-keyword,.hljs-template-tag,.hljs-template-variable,.hljs-type,.hljs-variable.language_{color:#ff7b72}.hljs-title,.hljs-title.class_,.hljs-title.class_.inherited__,.hljs-title.function_{color:#d2a8ff}.hljs-attr,.hljs-attribute,.hljs-literal,.hljs-meta,.hljs-number,.hljs-operator,.hljs-selector-attr,.hljs-selector-class,.hljs-selector-id,.hljs-variable{color:#79c0ff}.hljs-meta .hljs-string,.hljs-regexp,.hljs-string{color:#a5d6ff}.hljs-built_in,.hljs-symbol{color:#ffa657}.hljs-code,.hljs-comment,.hljs-formula{color:#8b949e}.hljs-name,.hljs-quote,.hljs-selector-pseudo,.hljs-selector-tag{color:#7ee787}.hljs-subst{color:#c9d1d9}.hljs-section{color:#1f6feb;font-weight:700}.hljs-bullet{color:#f2cc60}.hljs-emphasis{color:#c9d1d9;font-style:italic}.hljs-strong{color:#c9d1d9;font-weight:700}.hljs-addition{color:#aff5b4;background-color:#033a16}.hljs-deletion{color:#ffdcd7;background-color:#67060c}
      )";
      }

//---------------------------------------------------------
//   getChatCss
//---------------------------------------------------------

QString ChatDisplay::getChatCss() const {
      return R"(
        body { font-family: 'Inter', sans-serif; background: #ffffff; color: #1f2328; line-height: 1.0; margin: 0; padding: 4px 8px; box-sizing: border-box; }
        #chat-container { display: flex; flex-direction: column; width: 100%; box-sizing: border-box; }
        .message {
            padding: 12px 16px;
            margin: 6px 0;
            border-radius: 12px;
            max-width: 96%;
            box-sizing: border-box;
            word-wrap: break-word;
            }
        .ai      { background: #f6f8fa; border-left: 4px solid #0969da; align-self: flex-start; width: 100%; }
        .user    { background: #e6f0ff; border-right: 4px solid #555; align-self: flex-end; width: 96%;}
        /* Tabellen Styling */
        table { border-collapse: collapse; width: 100%; margin: 10px 0; }
        th, td { border: 1px solid #d0d7de; padding: 8px; text-align: left; }
        th { background: #f6f8fa; }
        /* Code-Container (erzeugt von renderMarkdownToHtml) */
        .code-container { border: 1px solid #d0d7de; border-radius: 6px; margin: 10px 0; display: block; }
        .code-header { background: #f6f8fa; padding: 5px 10px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #d0d7de; font-size: 0.8em; font-family: sans-serif; }
        .code-body { max-height: 400px; overflow-y: auto; overflow-x: auto; display: block; }
        .code-body pre { margin: 0; padding: 10px; overflow: visible; background: #f6f8fa; border: none; border-radius: 0; }
        .copy-btn { cursor: pointer; background: #fff; border: 1px solid #d0d7de; border-radius: 4px; padding: 3px 6px; font-size: 0.9em; display: inline-flex; align-items: center; justify-content: center; line-height: 1; }
        .copy-btn:hover { background: #f0f0f0; }
        /* Fallback für einfaches pre/code ohne Wrapper */
        pre { background: #f6f8fa; padding: 12px; border-radius: 6px; overflow-x: auto; border: 1px solid #d0d7de; }
        code { font-family: 'Fira Code', monospace; font-size: 0.9em; }
        .thought-box {
          background-color: #f6f8fa;
          border: 1px solid #d0d7de;
          border-radius: 8px;
          margin-bottom: 10px;
          font-size: 0.9em;
          color: #555;
          }
        .thought-box summary {
          padding: 8px;
          cursor: pointer;
          font-weight: bold;
          color: #0969da;
          outline: none;
          }
       .thought-content {
          padding: 8px 12px;
          border-top: 1px solid #d0d7de;
          font-style: italic;
          }
      )";
      }

//---------------------------------------------------------
//   getChatDarkCss
//---------------------------------------------------------

QString ChatDisplay::getChatDarkCss() const {
      return R"(
        body { font-family: 'Inter', sans-serif; background: #121212; color: #e0e0e0; line-height: 1.0; margin: 0; padding: 4px 8px; box-sizing: border-box; }
        #chat-container { display: flex; flex-direction: column; width: 100%; box-sizing: border-box; }
        .message {
            padding: 12px 16px;
            margin: 6px 0;
            border-radius: 12px;
            max-width: 96%;
            box-sizing: border-box;
            word-wrap: break-word;
            }
        .ai      { background: #1e1e1e; border-left: 4px solid #0078d4; align-self: flex-start; width: 100%; }
        .user    { background: #2c2c2c; border-right: 4px solid #555; align-self: flex-end; width: 96%;}
        .inactive-message { opacity: 0.5; filter: grayscale(50%); }
        /* Tabellen Styling */
        table { border-collapse: collapse; width: 100%; margin: 10px 0; }
        th, td { border: 1px solid #444; padding: 8px; text-align: left; }
        th { background: #333; }
        /* Code-Container (erzeugt von renderMarkdownToHtml) */
        .code-container { border: 1px solid #30363d; border-radius: 6px; margin: 10px 0; display: block; }
        .code-header { background: #161b22; padding: 5px 10px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #30363d; font-size: 0.8em; font-family: sans-serif; color: #c9d1d9; }
        .code-body { max-height: 400px; overflow-y: auto; overflow-x: auto; display: block; }
        .code-body pre { margin: 0; padding: 10px; overflow: visible; background: #0d1117; border: none; border-radius: 0; }
        .copy-btn { cursor: pointer; background: #0d1117; border: 1px solid #30363d; border-radius: 4px; padding: 3px 6px; font-size: 0.9em; color: #c9d1d9; display: inline-flex; align-items: center; justify-content: center; line-height: 1; }
        .copy-btn:hover { background: #21262d; }
        /* Fallback für einfaches pre/code ohne Wrapper */
        pre { background: #000; padding: 12px; border-radius: 6px; overflow-x: auto; }
        code { font-family: 'Fira Code', monospace; font-size: 0.9em; }
        .thought-box {
          background-color: #1a1a1a;
          border: 1px solid #333;
          border-radius: 8px;
          margin-bottom: 10px;
          font-size: 0.9em;
          color: #aaa;
          }
        .thought-box summary {
          padding: 8px;
          cursor: pointer;
          font-weight: bold;
          color: #0078d4;
          outline: none;
          }
       .thought-content {
          padding: 8px 12px;
          border-top: 1px solid #333;
          font-style: italic;
          }
      )";
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
                        window.hljs.highlightAll();
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
                  window.hljs.highlightAll();
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
            window.hljs.highlightAll();
            if (isScrolledToBottom) {
                  window.scrollTo(0, document.body.scrollHeight);
            }
            }

      </script>
      </body>
</html>
)")
                      .arg(hljsCss, chatCss);
      setHtml(html);
      }

//---------------------------------------------------------
//   setDarkMode
//---------------------------------------------------------

void ChatDisplay::setDarkMode(bool enabled) {
      if (_darkMode == enabled)
            return;
      _darkMode = enabled;
      settings()->setAttribute(QWebEngineSettings::ForceDarkMode, _darkMode);

      QString js = "(function() { "
                   "  var el1 = document.getElementById('hljs-theme'); "
                   "  if (el1) el1.textContent = " +
                   quoteForJs(_darkMode ? getHighlightJsDarkCss() : getHighlightJsCss()) +
                   "; "
                   "  var el2 = document.getElementById('chat-theme'); "
                   "  if (el2) el2.textContent = " +
                   quoteForJs(_darkMode ? getChatDarkCss() : getChatCss()) +
                   "; "
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
      if (thoughtChunk.empty() && textChunk.empty())
            return;

      // startMessage was delayed until some date arrived
      // this avoids display of empty messages

      if (mustStartMessage) {
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
      QString js = QString("if(typeof appendStaticMessage === 'function') { appendStaticMessage(%1, %2, %3, %4); } else { "
                           "  setTimeout(function() { if(typeof appendStaticMessage === 'function') { appendStaticMessage(%1, %2, %3, %4); } "
                           "else { console.error('appendStaticMessage not found!'); } }, 100);"
                           "}")
                       .arg(quoteForJs(role), quoteForJs(html), quoteForJs(thoughtHtml), isActive ? "true" : "false");
      page()->runJavaScript(js, [](const QVariant& res) { (void)res; });
      }
