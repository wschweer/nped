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

#include "screenshot.h"
#include "logger.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusObjectPath>
#include <QFile>
#include <QUrl>
#include <QImage>

//---------------------------------------------------------
//   ScreenshotHelper
//---------------------------------------------------------

ScreenshotHelper::ScreenshotHelper(QObject* parent) : QObject(parent)
      {
      }

//---------------------------------------------------------
//   takeScreenshot
//    Calls the XDG Desktop Portal Screenshot method.
//    The portal shows a permission dialog to the user (system enforced).
//    On success / failure, the "Response" signal is received on the
//    returned Request handle object.
//---------------------------------------------------------

void ScreenshotHelper::takeScreenshot()
      {
      // Disconnect any old pending handle signal to avoid duplicate connections
      if (!_pendingHandlePath.isEmpty()) {
            QDBusConnection::sessionBus().disconnect(
                "org.freedesktop.portal.Desktop",
                _pendingHandlePath,
                "org.freedesktop.portal.Request",
                "Response",
                this,
                SLOT(onPortalResponse(uint, QVariantMap)));
            _pendingHandlePath.clear();
            }

      QDBusInterface portal(
          "org.freedesktop.portal.Desktop",
          "/org/freedesktop/portal/desktop",
          "org.freedesktop.portal.Screenshot",
          QDBusConnection::sessionBus());

      if (!portal.isValid()) {
            QString err = QDBusConnection::sessionBus().lastError().message();
            emit screenshotFailed(
                QString("XDG Desktop Portal (Screenshot) not available: %1").arg(err));
            return;
            }

      // parent_window: empty string is fine for most compositors
      // options: interactive=false → no interactive cropping dialog
      QVariantMap options;
      options["interactive"] = true;

      QDBusMessage reply = portal.call("Screenshot", QString(""), options);

      if (reply.type() == QDBusMessage::ErrorMessage) {
            emit screenshotFailed(
                QString("Screenshot portal call failed: %1").arg(reply.errorMessage()));
            return;
            }

      // The return value is a QDBusObjectPath representing the Request handle
      if (reply.arguments().isEmpty()) {
            emit screenshotFailed("Screenshot portal returned no handle");
            return;
            }

      QDBusObjectPath handlePath = reply.arguments().at(0).value<QDBusObjectPath>();
      _pendingHandlePath         = handlePath.path();

      Debug("Screenshot portal handle: {}", _pendingHandlePath.toStdString());

      // Connect to the Response signal on the Request object
      bool connected = QDBusConnection::sessionBus().connect(
          "org.freedesktop.portal.Desktop",
          _pendingHandlePath,
          "org.freedesktop.portal.Request",
          "Response",
          this,
          SLOT(onPortalResponse(uint, QVariantMap)));

      if (!connected) {
            emit screenshotFailed(
                QString("Failed to connect to portal Response signal: %1")
                    .arg(QDBusConnection::sessionBus().lastError().message()));
            }
      }

//---------------------------------------------------------
//   onPortalResponse
//    Slot called when the XDG portal sends the Response signal.
//    response == 0  → success
//    response == 1  → user cancelled
//    response >= 2  → error
//---------------------------------------------------------

void ScreenshotHelper::onPortalResponse(uint response, const QVariantMap& results)
      {
      // Disconnect so we don't get called twice
      if (!_pendingHandlePath.isEmpty()) {
            QDBusConnection::sessionBus().disconnect(
                "org.freedesktop.portal.Desktop",
                _pendingHandlePath,
                "org.freedesktop.portal.Request",
                "Response",
                this,
                SLOT(onPortalResponse(uint, QVariantMap)));
            _pendingHandlePath.clear();
            }

      if (response != 0) {
            if (response == 1)
                  emit screenshotFailed("Screenshot cancelled by user");
            else
                  emit screenshotFailed(
                      QString("Screenshot failed (portal response code: %1)").arg(response));
            return;
            }

      QString uri = results.value("uri").toString();
      if (uri.isEmpty()) {
            emit screenshotFailed("Portal response contained no image URI");
            return;
            }

      // Convert file URI → local path and load image
      QString filePath = QUrl(uri).toLocalFile();
      Debug("Screenshot saved to: {}", filePath.toStdString());

      QImage image(filePath);
      if (image.isNull()) {
            emit screenshotFailed(
                QString("Could not load screenshot image from: %1").arg(filePath));
            return;
            }

      // Remove the temporary file created by the portal
      QFile::remove(filePath);

      emit screenshotReady(image);
      }
