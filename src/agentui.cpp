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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QLabel>
#include <QTimer>
#include <QAction>
#include <QScrollBar>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QNetworkAccessManager>

#include "agent.h"
#include "editor.h"
#include "configdialog.h"
#include "logger.h"

//---------------------------------------------------------
//   isWorking
//---------------------------------------------------------

bool Agent::isWorking() const {
      return spinnerTimer->isActive();
      }

//---------------------------------------------------------
//   setInputEnabled
//---------------------------------------------------------

void Agent::setInputEnabled(bool enabled) {
      userInput->setEnabled(enabled);
      if (enabled) {
            spinnerTimer->stop();
            statusLabel->setText(">");
            statusLabel->setStyleSheet("color: normal; font-weight: bold;");
            }
      else {
            spinnerFrame = 0;
            spinnerTimer->start(100);
            statusLabel->setStyleSheet("color: #ff8800; font-weight: bold;");
            }
      }

//---------------------------------------------------------
//   updateSpinner
//---------------------------------------------------------

void Agent::updateSpinner() {
      const QString frames = "|/-\\";
      statusLabel->setText(QString(frames[spinnerFrame % 4]));
      spinnerFrame++;
      }

//---------------------------------------------------------
//   eventFilter
//   Fängt Enter vs. Shift+Enter im mehrzeiligen Textfeld ab
//---------------------------------------------------------

bool Agent::eventFilter(QObject* obj, QEvent* event) {
      if (obj == userInput && event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                  if (keyEvent->modifiers() & Qt::ShiftModifier)
                        return false;
                  sendMessage();
                  return true;
                  }
            }
      return QWidget::eventFilter(obj, event);
      }

//---------------------------------------------------------
//   openConfigDialog
//---------------------------------------------------------

void Agent::openConfigDialog() {
      configDialog->open();
      configDialog->move((_editor->width() - configDialog->width()) / 2,
                         (_editor->height() - configDialog->height()) / 2);
      }

//---------------------------------------------------------
//   updateChatDisplay
//---------------------------------------------------------

void Agent::updateChatDisplay() {
      QString md = "# AI Agent Session Log\n\n---\n\n";

      for (const auto& msg : chatHistory) {
            std::string role = msg.value("role", "");
            if (role == "system")
                  continue;

            // Content extrahieren, Anthropic Tool-Results erkennen
            QString contentText;
            bool isAnthropicToolResult = false;
            if (msg.contains("content")) {
                  if (msg["content"].is_string()) {
                        contentText = QString::fromStdString(msg["content"].get<std::string>());
                        }
                  else if (msg["content"].is_array()) {
                        for (const auto& block : msg["content"]) {
                              if (block.contains("type") && block["type"] == "tool_result") {
                                    isAnthropicToolResult = true;
                                    if (block.contains("content") && block["content"].is_string())
                                          contentText += QString::fromStdString(block["content"].get<std::string>());
                                    }
                              else if (block.contains("text") && block["text"].is_string()) {
                                    contentText += QString::fromStdString(block["text"].get<std::string>());
                                    }
                              }
                        }
                  }

            auto truncate = [](const QString& s) {
                  return s.length() > kChatResultMaxChars
                             ? s.left(kChatResultMaxChars) + "\n... [Output abgeschnitten]"
                             : s;
                  };

            if (role == "user" && isAnthropicToolResult) {
                  QString r = truncate(contentText);
                  md += "> **Tool Result**\n";
                  md += "> ```text\n> " + r.replace("\n", "\n> ") + "\n> ```\n\n";
                  }
            else if (role == "user") {
                  if (!contentText.isEmpty())
                        md += "### Du:\n" + contentText + "\n\n";
                  }
            else if (role == "assistant") {
                  md += "### AI:\n" + contentText + "\n\n";
                  if (msg.contains("tool_calls") && !msg["tool_calls"].empty())
                        for (const auto& tc : msg["tool_calls"]) {
                              QString toolName = "unknown";
                              if (tc.contains("function") && tc["function"].contains("name"))
                                    toolName = QString::fromStdString(tc["function"]["name"].get<std::string>());
                              md += "*[Hat Tool ausgeführt: " + toolName + "]*\n\n";
                              }
                  if (msg.contains("content") && msg["content"].is_array())
                        for (const auto& block : msg["content"])
                              if (block.contains("type") && block["type"] == "tool_use") {
                                    QString toolName = "unknown";
                                    if (block.contains("name"))
                                          toolName = QString::fromStdString(block["name"].get<std::string>());
                                    md += "*[Hat Tool ausgeführt: " + toolName + "]*\n\n";
                                    }
                  }
            else if (role == "tool") {
                  std::string name = msg.value("name", "unknown_tool");
                  QString r        = truncate(contentText);
                  md += "> **Tool Run (" + QString::fromStdString(name) + ")**\n";
                  md += "> ```text\n> " + r.replace("\n", "\n> ") + "\n> ```\n\n";
                  }
            }

      chatDisplay->setMarkdown(md);
      QTextCursor cursor = chatDisplay->textCursor();
      cursor.movePosition(QTextCursor::End);
      chatDisplay->setTextCursor(cursor);
      chatDisplay->verticalScrollBar()->setValue(chatDisplay->verticalScrollBar()->maximum());
      }
