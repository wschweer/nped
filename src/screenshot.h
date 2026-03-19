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

#include <QObject>
#include <QImage>
#include <QString>
#include <QVariantMap>

//---------------------------------------------------------
//   ScreenshotHelper
//
//   Takes a screenshot via the XDG Desktop Portal (D-Bus).
//   Works on Wayland and X11.
//   Usage:
//     connect(helper, &ScreenshotHelper::screenshotReady, ...);
//     helper->takeScreenshot();
//---------------------------------------------------------

class ScreenshotHelper : public QObject
      {
      Q_OBJECT

    public:
      explicit ScreenshotHelper(QObject* parent = nullptr);

      /// Triggers the portal screenshot dialog asynchronously.
      /// Emits screenshotReady() or screenshotFailed() when done.
      void takeScreenshot();

    signals:
      void screenshotReady(const QImage& image);
      void screenshotFailed(const QString& reason);

    private slots:
      void onPortalResponse(uint response, const QVariantMap& results);

    private:
      QString _pendingHandlePath;
      };
