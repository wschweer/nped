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

#include "webview.h"

class Editor;
class QVBoxLayout;

//---------------------------------------------------------
//   ConfigWebView
//
//   A MarkdownWebView-derived page that renders the
//   configuration UI from JSON schemas using json-editor.
//   It is stacked on top of the editor widget (index 2).
//---------------------------------------------------------

class ConfigWebView : public MarkdownWebView
      {
      Q_OBJECT

    signals:
      void configSaved();
      void configCancelled();
      void configResetRequested();

    public:
      explicit ConfigWebView(Editor* editor, QWidget* parent = nullptr);

      QVariant getProperty(const QString& name) const;
      void setProperty(const QString& name, const QJsonValue& value);
      /// Load the config.html page and inject current data
      void openConfig(const QString& activeListName = QString(), int activeListItem = -1);
      };
