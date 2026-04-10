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

import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: root
    property alias title: label.text

    Layout.fillWidth: true
    Layout.bottomMargin: 10
    spacing: 5

    Label {
        id: label
        font.pointSize: 18
        font.bold: true
        color: Material.foreground
        }

    Rectangle {
        implicitHeight: 1
        Layout.fillWidth: true
        color: Material.accent
        }

    }